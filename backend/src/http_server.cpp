#include "http_server.h"
#include "variable_manager.h"
#include "database.h"
#include "opcua_manager.h"
#include "udp_manager.h"
#include "frontend_assets.h"
#include <wincrypt.h>
#include <ws2tcpip.h>
#include <algorithm>
#include <cstring>
#include <map>
#include <regex>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <random>

// ============ 硬编码认证 ============
static const char* HARDCODED_USERNAME = "admin";
static const char* HARDCODED_PASSWORD = "sapi.1992";
static const char* HARDCODED_TOKEN = "monitoring-platform-fixed-token";
static const char* LAN_ACCESS_HOST = "202.118.21.28";
static const int DEV_FRONTEND_PORT = 5170;

// 简易会话管理
static std::map<std::string, std::string> g_sessions;  // token -> username
static std::mutex g_sessionMutex;

static std::string generateToken() {
    static const char hexChars[] = "0123456789abcdef";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    std::string token(64, '\0');
    for (int i = 0; i < 64; ++i) {
        token[i] = hexChars[dis(gen)];
    }
    return token;
}

static bool verifySession(const std::string& token, std::string& username) {
    if (token == HARDCODED_TOKEN) {
        username = HARDCODED_USERNAME;
        return true;
    }

    std::lock_guard lock(g_sessionMutex);
    auto it = g_sessions.find(token);
    if (it != g_sessions.end()) {
        username = it->second;
        return true;
    }
    return false;
}

static bool removeSession(const std::string& token) {
    std::lock_guard lock(g_sessionMutex);
    return g_sessions.erase(token) > 0;
}

// 判断 API 路径是否需要认证
static bool isPublicApiPath(const std::vector<std::string>& parts) {
    // 允许无需认证的公开 API
    if (parts.size() >= 2 && parts[0] == "api" && parts[1] == "auth") return true;
    if (parts.size() >= 2 && parts[0] == "api" && parts[1] == "status") return true;
    if (parts.size() >= 2 && parts[0] == "api" && parts[1] == "network") return true;
    return false;
}

HttpServer& HttpServer::instance() {
    static HttpServer inst;
    return inst;
}

std::vector<std::string> HttpServer::splitPath(const std::string& path) {
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string item;
    while (std::getline(ss, item, '/')) {
        if (!item.empty()) parts.push_back(item);
    }
    return parts;
}

// HTML 响应包装
static std::string jsonResponse(const std::string& body, int statusCode = 200) {
    std::string statusStr = (statusCode == 200) ? "200 OK" :
                            (statusCode == 400) ? "400 Bad Request" :
                            (statusCode == 401) ? "401 Unauthorized" :
                            (statusCode == 404) ? "404 Not Found" :
                            (statusCode == 405) ? "405 Method Not Allowed" :
                            (statusCode == 500) ? "500 Internal Server Error" : "200 OK";
    std::stringstream ss;
    ss << "HTTP/1.1 " << statusStr << "\r\n"
       << "Content-Type: application/json\r\n"
       << "Access-Control-Allow-Origin: *\r\n"
       << "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
       << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
       << "Content-Length: " << body.size() << "\r\n"
       << "Connection: close\r\n"
       << "\r\n"
       << body;
    return ss.str();
}

static std::string htmlResponse(const std::string& body, int statusCode = 200) {
    std::string statusStr = (statusCode == 200) ? "200 OK" :
                            (statusCode == 400) ? "400 Bad Request" :
                            (statusCode == 404) ? "404 Not Found" :
                            (statusCode == 500) ? "500 Internal Server Error" : "200 OK";
    std::stringstream ss;
    ss << "HTTP/1.1 " << statusStr << "\r\n"
       << "Content-Type: text/html; charset=utf-8\r\n"
       << "Access-Control-Allow-Origin: *\r\n"
       << "Content-Length: " << body.size() << "\r\n"
       << "Connection: close\r\n"
       << "\r\n"
       << body;
    return ss.str();
}

static std::string urlDecode(const std::string& str);

static std::string fileResponse(const std::string& body, const std::string& contentType, int statusCode = 200) {
    std::string statusStr = (statusCode == 200) ? "200 OK" :
                            (statusCode == 404) ? "404 Not Found" :
                            (statusCode == 500) ? "500 Internal Server Error" : "200 OK";
    std::stringstream ss;
    ss << "HTTP/1.1 " << statusStr << "\r\n"
       << "Content-Type: " << contentType << "\r\n"
       << "Access-Control-Allow-Origin: *\r\n"
       << "Content-Length: " << body.size() << "\r\n"
       << "Connection: close\r\n"
       << "\r\n";
    std::string header = ss.str();
    std::string response;
    response.reserve(header.size() + body.size());
    response += header;
    response += body;
    return response;
}

static std::string mimeTypeForPath(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
    if (ext == ".html") return "text/html; charset=utf-8";
    if (ext == ".js") return "application/javascript; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".json") return "application/json; charset=utf-8";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".ico") return "image/x-icon";
    if (ext == ".woff2") return "font/woff2";
    return "application/octet-stream";
}

static bool readFileBinary(const std::filesystem::path& path, std::string& body) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    body = ss.str();
    return true;
}

static std::mutex g_customComponentsFileMutex;

static std::filesystem::path customComponentsFilePath() {
    namespace fs = std::filesystem;
    fs::path dir = fs::current_path() / "data";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir / "custom_components.json";
}

static std::string trimAsciiWhitespace(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

static bool looksLikeJsonArray(const std::string& body) {
    std::string trimmed = trimAsciiWhitespace(body);
    return trimmed.size() >= 2 && trimmed.front() == '[' && trimmed.back() == ']';
}

static std::string readCustomComponentsJson() {
    std::lock_guard lock(g_customComponentsFileMutex);
    std::string body;
    if (readFileBinary(customComponentsFilePath(), body) && looksLikeJsonArray(body)) {
        return body;
    }
    return "[]";
}

static bool writeCustomComponentsJson(const std::string& body) {
    if (!looksLikeJsonArray(body)) return false;
    std::lock_guard lock(g_customComponentsFileMutex);
    std::ofstream out(customComponentsFilePath(), std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << body;
    return static_cast<bool>(out);
}

static std::filesystem::path findFrontendDist() {
    namespace fs = std::filesystem;
    fs::path cwd = fs::current_path();
    std::vector<fs::path> candidates = {
        cwd / "frontend" / "dist",
        cwd / ".." / "frontend" / "dist",
        cwd / ".." / ".." / "frontend" / "dist",
        cwd / ".." / ".." / ".." / "frontend" / "dist"
    };

    for (const auto& candidate : candidates) {
        std::error_code ec;
        fs::path full = fs::weakly_canonical(candidate, ec);
        if (!ec && fs::exists(full / "index.html")) return full;
    }
    return {};
}

static std::string staticFileResponse(const std::string& path) {
    namespace fs = std::filesystem;
    std::string cleanPath = urlDecode(path);
    if (cleanPath.empty() || cleanPath == "/") cleanPath = "/index.html";
    if (cleanPath.find("..") != std::string::npos) {
        return htmlResponse("<h1>Bad Request</h1>", 400);
    }
    if (!cleanPath.empty() && cleanPath.front() == '/') cleanPath.erase(cleanPath.begin());

    fs::path dist = findFrontendDist();
    if (dist.empty() && embeddedFrontendAvailable()) {
        const EmbeddedFrontendAsset* asset = findEmbeddedFrontendAsset(cleanPath);
        if (!asset) asset = findEmbeddedFrontendAsset("index.html");
        if (asset) {
            std::string body(reinterpret_cast<const char*>(asset->data), asset->size);
            return fileResponse(body, asset->contentType);
        }
    }

    if (dist.empty()) {
        return htmlResponse("<h1>Frontend runtime assets not found</h1><p>Run npm run build and tools/embed_frontend.ps1.</p>", 500);
    }

    fs::path target = dist / cleanPath;
    std::error_code ec;
    target = fs::weakly_canonical(target, ec);
    if (ec || target.string().find(dist.string()) != 0 || !fs::exists(target) || fs::is_directory(target)) {
        target = dist / "index.html";
    }

    std::string body;
    if (!readFileBinary(target, body)) {
        return htmlResponse("<h1>Static file not found</h1>", 404);
    }
    return fileResponse(body, mimeTypeForPath(target));
}

static std::string urlDecode(const std::string& str) {
    std::string result;
    result.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%' && i + 2 < str.size()) {
            int val;
            std::istringstream iss(str.substr(i + 1, 2));
            if (iss >> std::hex >> val) {
                result += static_cast<char>(val);
                i += 2;
            } else {
                result += str[i];
            }
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

static std::string extractBody(const std::string& request) {
    auto pos = request.find("\r\n\r\n");
    if (pos == std::string::npos) return "";
    return request.substr(pos + 4);
}

static std::string extractMethod(const std::string& request) {
    auto pos = request.find(' ');
    if (pos == std::string::npos) return "";
    return request.substr(0, pos);
}

static std::string extractPath(const std::string& request) {
    auto start = request.find(' ');
    if (start == std::string::npos) return "";
    auto end = request.find(' ', start + 1);
    if (end == std::string::npos) return "";
    std::string path = request.substr(start + 1, end - start - 1);
    // 去掉查询参数
    auto qPos = path.find('?');
    if (qPos != std::string::npos) path = path.substr(0, qPos);
    return path;
}

static std::map<std::string, std::string> extractQueryParams(const std::string& request) {
    std::map<std::string, std::string> params;
    auto start = request.find(' ');
    if (start == std::string::npos) return params;
    auto end = request.find(' ', start + 1);
    if (end == std::string::npos) return params;
    std::string path = request.substr(start + 1, end - start - 1);
    auto qPos = path.find('?');
    if (qPos == std::string::npos) return params;

    std::string qs = path.substr(qPos + 1);
    std::stringstream ss(qs);
    std::string pair;
    while (std::getline(ss, pair, '&')) {
        auto eq = pair.find('=');
        if (eq != std::string::npos) {
            std::string key = urlDecode(pair.substr(0, eq));
            std::string val = urlDecode(pair.substr(eq + 1));
            params[key] = val;
        }
    }
    return params;
}

static std::string toLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

static std::string trimHeaderValue(std::string val) {
    while (!val.empty() && std::isspace(static_cast<unsigned char>(val.front()))) {
        val.erase(val.begin());
    }
    while (!val.empty() && std::isspace(static_cast<unsigned char>(val.back()))) {
        val.pop_back();
    }
    return val;
}

static std::string extractHeader(const std::string& request, const std::string& header) {
    const std::string target = toLowerAscii(header);
    size_t lineStart = 0;
    while (lineStart < request.size()) {
        size_t lineEnd = request.find("\r\n", lineStart);
        if (lineEnd == std::string::npos) lineEnd = request.size();
        if (lineEnd == lineStart) break;

        size_t colon = request.find(':', lineStart);
        if (colon != std::string::npos && colon < lineEnd) {
            std::string name = request.substr(lineStart, colon - lineStart);
            if (toLowerAscii(name) == target) {
                return trimHeaderValue(request.substr(colon + 1, lineEnd - colon - 1));
            }
        }

        if (lineEnd == request.size()) break;
        lineStart = lineEnd + 2;
    }
    return "";
}

// 从 Authorization header 提取 Bearer token
static std::string extractBearerToken(const std::string& request) {
    std::string authHeader = extractHeader(request, "Authorization");
    const std::string prefix = "Bearer ";
    if (authHeader.size() > prefix.size() &&
        authHeader.compare(0, prefix.size(), prefix) == 0) {
        return authHeader.substr(prefix.size());
    }
    return "";
}

static std::vector<std::string> getLocalIPv4Addresses() {
    std::vector<std::string> addresses;
    char hostname[256] = {0};
    if (gethostname(hostname, sizeof(hostname)) != 0) return addresses;

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* result = nullptr;
    if (getaddrinfo(hostname, nullptr, &hints, &result) != 0) return addresses;

    for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
        sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(ptr->ai_addr);
        char ip[INET_ADDRSTRLEN] = {0};
        if (inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip))) {
            std::string value(ip);
            if (value != "127.0.0.1" &&
                std::find(addresses.begin(), addresses.end(), value) == addresses.end()) {
                addresses.push_back(value);
            }
        }
    }

    freeaddrinfo(result);
    return addresses;
}

// ============ WebSocket ============

static void encodeWSFrame(std::vector<char>& frame, const std::string& data, int opcode = 0x01) {
    frame.clear();
    frame.push_back(0x80 | (opcode & 0x0F)); // FIN + opcode
    size_t len = data.size();
    if (len < 126) {
        frame.push_back(static_cast<char>(len));
    } else if (len < 65536) {
        frame.push_back(126);
        frame.push_back(static_cast<char>((len >> 8) & 0xFF));
        frame.push_back(static_cast<char>(len & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<char>((len >> (i * 8)) & 0xFF));
        }
    }
    frame.insert(frame.end(), data.begin(), data.end());
}

static bool decodeWSFrame(const std::vector<char>& buffer, size_t size, std::string& payload) {
    if (size < 2) return false;
    bool masked = (buffer[1] & 0x80) != 0;
    size_t payloadLen = buffer[1] & 0x7F;
    size_t offset = 2;

    if (payloadLen == 126) {
        if (size < 4) return false;
        payloadLen = ((size_t)buffer[2] << 8) | (size_t)buffer[3];
        offset = 4;
    } else if (payloadLen == 127) {
        if (size < 10) return false;
        payloadLen = 0;
        for (int i = 0; i < 8; ++i) {
            payloadLen = (payloadLen << 8) | (size_t)buffer[2 + i];
        }
        offset = 10;
    }

    if (masked) {
        if (size < offset + 4 + payloadLen) return false;
        unsigned char mask[4];
        memcpy(mask, &buffer[offset], 4);
        offset += 4;
        payload.resize(payloadLen);
        for (size_t i = 0; i < payloadLen; ++i) {
            payload[i] = buffer[offset + i] ^ mask[i % 4];
        }
    } else {
        if (size < offset + payloadLen) return false;
        payload.assign(&buffer[offset], payloadLen);
    }
    return true;
}

bool HttpServer::performWebSocketHandshake(SOCKET clientSock, const std::string& request) {
    auto key = extractHeader(request, "Sec-WebSocket-Key");
    if (key.empty()) return false;

    // WebSocket accept key (RFC 6455)
    const std::string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string acceptInput = key + magic;

    // SHA-1 hash
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE hash[20];
    DWORD hashLen = 20;

    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        return false;
    }
    if (!CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return false;
    }
    CryptHashData(hHash, (BYTE*)acceptInput.c_str(), (DWORD)acceptInput.size(), 0);
    CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0);
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    // Base64 encode
    DWORD base64Len = 0;
    CryptBinaryToStringA(hash, hashLen, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &base64Len);
    std::string acceptKey(base64Len, '\0');
    CryptBinaryToStringA(hash, hashLen, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &acceptKey[0], &base64Len);
    acceptKey.resize(base64Len);
    while (!acceptKey.empty() && acceptKey.back() == '\0') {
        acceptKey.pop_back();
    }

    std::string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + acceptKey + "\r\n"
        "\r\n";

    send(clientSock, response.c_str(), (int)response.size(), 0);
    return true;
}

void HttpServer::handleWSClient(SOCKET clientSock) {
    try {
    std::vector<char> buffer(65536);
    while (running_) {
        int bytes = recv(clientSock, buffer.data(), (int)buffer.size(), 0);
        if (bytes <= 0) break;

        std::string payload;
        int opcode = buffer[0] & 0x0F;

        if (opcode == 0x08) break; // Close
        if (opcode == 0x09) {      // Ping -> Pong
            std::vector<char> pong;
            encodeWSFrame(pong, "", 0x0A);
            send(clientSock, pong.data(), (int)pong.size(), 0);
            continue;
        }
        if (opcode == 0x01) {      // Text
            if (decodeWSFrame(buffer, bytes, payload)) {
                // Handle incoming WS messages (e.g., subscribe to variables)
            }
        }
    }
    } catch (const std::exception& e) {
        Log(LogLevel::ERR, std::string("WebSocket client handler crashed: ") + e.what());
    } catch (...) {
        Log(LogLevel::ERR, "WebSocket client handler crashed: unknown exception");
    }
    // Remove from client list
    std::lock_guard lock(wsMutex_);
    wsClients_.erase(std::remove_if(wsClients_.begin(), wsClients_.end(),
        [clientSock](const WSClient& c) { return c.sock == clientSock; }),
        wsClients_.end());
    closesocket(clientSock);
}

void HttpServer::broadcastToWS(const std::string& data) {
    std::lock_guard lock(wsMutex_);
    std::vector<char> frame;
    encodeWSFrame(frame, data);

    for (auto& client : wsClients_) {
        if (client.handshakeDone) {
            send(client.sock, frame.data(), (int)frame.size(), 0);
        }
    }
}

void HttpServer::broadcastRealtime(const std::string& jsonData) {
    broadcastToWS(jsonData);
}

// ============ HTTP 请求处理 ============

std::string HttpServer::handleRequest(const std::string& method, const std::string& path,
                                       const std::string& body, const std::string& contentType,
                                       const std::map<std::string, std::string>& queryParams,
                                       const std::string& authToken) {
    // 保存 authToken 供内部使用
    std::string authToken_ = authToken;
    auto parts = splitPath(path);
    auto getQueryInt64 = [&](const std::string& key, int64_t fallback) -> int64_t {
        auto it = queryParams.find(key);
        if (it == queryParams.end() || it->second.empty()) return fallback;
        try { return std::stoll(it->second); } catch (...) { return fallback; }
    };
    auto getQueryInt = [&](const std::string& key, int fallback) -> int {
        auto it = queryParams.find(key);
        if (it == queryParams.end() || it->second.empty()) return fallback;
        try { return std::stoi(it->second); } catch (...) { return fallback; }
    };

    // CORS preflight
    if (method == "OPTIONS") {
        return "HTTP/1.1 204 No Content\r\n"
               "Access-Control-Allow-Origin: *\r\n"
               "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
               "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
               "Access-Control-Max-Age: 86400\r\n"
               "\r\n";
    }

    try {
        // ========== 会话认证检查 ==========
        // 对非公开 API 路径进行认证检查（authToken 由调用方从 HTTP 请求头中提取并传入）
        if (parts.size() >= 2 && parts[0] == "api" && !isPublicApiPath(parts)) {
            std::string username;
            if (!verifySession(authToken_, username)) {
                return jsonResponse("{\"error\":\"unauthorized\",\"message\":\"请先登录\"}", 401);
            }
        }

        // ========== API: 认证 ==========
        if (parts.size() >= 3 && parts[0] == "api" && parts[1] == "auth") {
            // POST /api/auth/login
            if (parts[2] == "login" && method == "POST") {
                auto extractStr = [&](const std::string& key) -> std::string {
                    std::string searchKey = "\"" + key + "\"";
                    auto pos = body.find(searchKey);
                    if (pos == std::string::npos) return "";
                    auto colon = body.find(':', pos);
                    if (colon == std::string::npos) return "";
                    auto start = body.find('"', colon + 1);
                    if (start == std::string::npos) return "";
                    auto end = body.find('"', start + 1);
                    if (end == std::string::npos) return "";
                    return body.substr(start + 1, end - start - 1);
                };
                std::string username = extractStr("username");
                std::string password = extractStr("password");

                if (username == HARDCODED_USERNAME && password == HARDCODED_PASSWORD) {
                    std::string token = HARDCODED_TOKEN;
                    {
                        std::lock_guard lock(g_sessionMutex);
                        g_sessions[token] = username;
                    }
                    Log(LogLevel::INFO, "User '" + username + "' logged in, token=" + token.substr(0, 16) + "...");
                    std::string json = "{\"status\":\"ok\",\"token\":\"" + escapeJson(token) +
                                      "\",\"username\":\"" + escapeJson(username) + "\"}";
                    return jsonResponse(json);
                } else {
                    return jsonResponse("{\"error\":\"invalid_credentials\",\"message\":\"账号或密码错误\"}", 401);
                }
            }
            // POST /api/auth/logout
            if (parts[2] == "logout" && method == "POST") {
                removeSession(authToken_);
                return jsonResponse("{\"status\":\"ok\"}");
            }
            // GET /api/auth/check
            if (parts[2] == "check" && method == "GET") {
                std::string username;
                if (verifySession(authToken_, username)) {
                    return jsonResponse("{\"authenticated\":true,\"username\":\"" + escapeJson(username) + "\"}");
                } else {
                    return jsonResponse("{\"authenticated\":false}", 401);
                }
            }
        }

        // ========== API: 自定义组件库 ==========
        if (parts.size() == 2 && parts[0] == "api" && parts[1] == "custom-components") {
            if (method == "GET") {
                return jsonResponse(readCustomComponentsJson());
            }
            if (method == "POST" || method == "PUT") {
                if (!writeCustomComponentsJson(body)) {
                    return jsonResponse("{\"error\":\"invalid component library payload\"}", 400);
                }
                return jsonResponse(body.empty() ? "[]" : body);
            }
            return jsonResponse("{\"error\":\"method not allowed\"}", 405);
        }

        // ========== API: 变量定义 ==========
        if (parts.size() >= 2 && parts[0] == "api" && parts[1] == "variables") {
            if (method == "GET" && parts.size() == 2) {
                auto vars = VariableManager::instance().getAllDefinitions();
                std::string json = "[";
                for (size_t i = 0; i < vars.size(); ++i) {
                    const auto& v = vars[i];
                    json += "{\"id\":" + std::to_string(v.id) +
                            ",\"name\":\"" + escapeJson(v.name) +
                            "\",\"description\":\"" + escapeJson(v.description) +
                            "\",\"dataType\":\"" + v.dataType +
                            "\",\"source\":\"" + v.source +
                            "\",\"opcuaNodeId\":\"" + escapeJson(v.opcuaNodeId) +
                            "\",\"udpPort\":" + std::to_string(v.udpPort) +
                            ",\"minValue\":" + (v.minValue ? std::to_string(v.minValue) : "null") +
                            ",\"maxValue\":" + (v.maxValue ? std::to_string(v.maxValue) : "null") +
                            ",\"unit\":\"" + escapeJson(v.unit) +
                            "\",\"color\":\"" + v.color +
                            "\",\"enabled\":" + (v.enabled ? "true" : "false") + "}";
                    if (i < vars.size() - 1) json += ",";
                }
                json += "]";
                return jsonResponse(json);
            }
            else if (method == "GET" && parts.size() >= 3) {
                int id = std::stoi(parts[2]);
                auto v = VariableManager::instance().getDefinition(id);
                if (v.id == 0) return jsonResponse("{\"error\":\"not found\"}", 404);
                std::string json = "{\"id\":" + std::to_string(v.id) + "...}"; // simplified
                return jsonResponse(json);
            }
            else if (method == "POST") {
                // Parse JSON body (simple)
                // In production use a proper JSON parser
                auto namePos = body.find("\"name\"");
                if (namePos == std::string::npos) return jsonResponse("{\"error\":\"name required\"}", 400);
                VariableInfo v;
                // Simple field extraction
                auto extractStr = [&](const std::string& key) -> std::string {
                    auto pos = body.find("\"" + key + "\"");
                    if (pos == std::string::npos) return "";
                    auto colon = body.find(':', pos);
                    if (colon == std::string::npos) return "";
                    auto start = body.find('"', colon + 1);
                    if (start == std::string::npos) return "";
                    auto end = body.find('"', start + 1);
                    if (end == std::string::npos) return "";
                    return body.substr(start + 1, end - start - 1);
                };
                auto extractNum = [&](const std::string& key) -> double {
                    auto pos = body.find("\"" + key + "\"");
                    if (pos == std::string::npos) return 0;
                    auto colon = body.find(':', pos);
                    if (colon == std::string::npos) return 0;
                    auto start = body.find_first_of("-0123456789", colon);
                    if (start == std::string::npos) return 0;
                    auto end = body.find_first_not_of(".0123456789eE-+", start);
                    if (end == std::string::npos) return std::stod(body.substr(start));
                    return std::stod(body.substr(start, end - start));
                };

                v.name = extractStr("name");
                v.description = extractStr("description");
                v.dataType = extractStr("dataType");
                if (v.dataType.empty()) v.dataType = "FLOAT";
                v.source = extractStr("source");
                if (v.source.empty()) v.source = "OPCUA";
                if (v.source != "OPCUA" && v.source != "UDP") {
                    return jsonResponse("{\"error\":\"unsupported source\"}", 400);
                }
                v.opcuaNodeId = extractStr("opcuaNodeId");
                v.unit = extractStr("unit");
                v.color = extractStr("color");
                if (v.color.empty()) v.color = "#2196F3";

                if (v.name.empty()) return jsonResponse("{\"error\":\"name required\"}", 400);

                if (VariableManager::instance().addOrUpdateDefinition(v)) {
                    return jsonResponse("{\"status\":\"ok\"}");
                }
                return jsonResponse("{\"error\":\"db error\"}", 500);
            }
            else if (method == "DELETE" && parts.size() >= 3) {
                int id = std::stoi(parts[2]);
                if (VariableManager::instance().removeDefinition(id)) {
                    return jsonResponse("{\"status\":\"ok\"}");
                }
                return jsonResponse("{\"error\":\"delete failed\"}", 500);
            }
        }

        // ========== API: 实时数据 ==========
        if (parts.size() >= 2 && parts[0] == "api" && parts[1] == "realtime") {
            if (method == "GET") {
                auto data = VariableManager::instance().getAllLatestData();
                std::string json = "[";
                for (size_t i = 0; i < data.size(); ++i) {
                    json += data[i].toJson();
                    if (i < data.size() - 1) json += ",";
                }
                json += "]";
                return jsonResponse(json);
            }
        }

        // ========== API: 历史数据 ==========
        if (parts.size() >= 3 && parts[0] == "api" && parts[1] == "history") {
            if (method == "GET") {
                int varId = std::stoi(parts[2]);
                auto history = VariableManager::instance().getHistory(
                    varId,
                    getQueryInt64("from", 0),
                    getQueryInt64("to", 0),
                    getQueryInt("max", 500)
                );
                std::string json = "[";
                for (size_t i = 0; i < history.size(); ++i) {
                    json += history[i].toJson();
                    if (i < history.size() - 1) json += ",";
                }
                json += "]";
                return jsonResponse(json);
            }
        }

        // ========== API: 写入变量 ==========
        if (parts.size() >= 3 && parts[0] == "api" && parts[1] == "write") {
            if (method == "POST") {
                int varId = std::stoi(parts[2]);
                // Parse value from body
                auto valPos = body.find("\"value\"");
                if (valPos == std::string::npos) return jsonResponse("{\"error\":\"value required\"}", 400);
                auto colon = body.find(':', valPos);
                auto start = body.find_first_of("-0123456789.", colon);
                if (start == std::string::npos) return jsonResponse("{\"error\":\"invalid value\"}", 400);
                auto end = body.find_first_not_of(".0123456789eE-+", start);
                double value = (end == std::string::npos) ? std::stod(body.substr(start))
                                                          : std::stod(body.substr(start, end - start));

                auto v = VariableManager::instance().getDefinition(varId);
                auto oldData = VariableManager::instance().getLatestData(varId);
                VariableManager::instance().updateValue(varId, value, "GOOD");

                std::string writeStatus = "LOCAL_ONLY";
                std::string writeDetail = "Local value updated";

                // 也写入 OPC UA 服务器
                if (!v.opcuaNodeId.empty()) {
                    bool writeOk = OPCUAManager::instance().writeVariable(v.opcuaNodeId, value);
                    writeStatus = writeOk ? "SUCCESS" : "OPCUA_FAILED";
                    writeDetail = writeOk ? "OPC UA write succeeded" : "OPC UA write failed";
                }

                DatabaseManager::instance().insertWriteRecord(
                    varId,
                    v.name,
                    value,
                    v.opcuaNodeId,
                    writeStatus,
                    "OPERATOR",
                    writeDetail,
                    nowMs()
                );

                // 记录操作日志
                OperationLog log;
                log.varId = varId;
                log.varName = v.name;
                log.operationType = "SET_VALUE";
                log.oldValue = oldData.varId == varId ? oldData.value : 0.0;
                log.newValue = value;
                log.operator_ = "OPERATOR";
                log.detail = "Set variable to " + std::to_string(value) + " (" + writeStatus + ")";
                DatabaseManager::instance().insertOperationLog(log);

                return jsonResponse("{\"status\":\"ok\",\"writeStatus\":\"" + writeStatus + "\"}");
            }
        }

        // ========== API: 报警 ==========
        if (parts.size() >= 2 && parts[0] == "api" && parts[1] == "alarms") {
            if (method == "GET") {
                auto alarms = DatabaseManager::instance().queryAlarms();
                std::string json = "[";
                for (size_t i = 0; i < alarms.size(); ++i) {
                    json += alarms[i].toJson();
                    if (i < alarms.size() - 1) json += ",";
                }
                json += "]";
                return jsonResponse(json);
            }
            if (method == "POST" && parts.size() >= 4 && parts[3] == "ack") {
                int64_t alarmId = std::stoll(parts[2]);
                if (DatabaseManager::instance().acknowledgeAlarm(alarmId)) {
                    return jsonResponse("{\"status\":\"ok\"}");
                }
                return jsonResponse("{\"error\":\"ack failed\"}", 500);
            }
        }

        // ========== API: 操作日志 ==========
        if (parts.size() >= 2 && parts[0] == "api" && parts[1] == "logs") {
            if (method == "GET") {
                auto logs = DatabaseManager::instance().queryOperationLogs();
                std::string json = "[";
                for (size_t i = 0; i < logs.size(); ++i) {
                    json += logs[i].toJson();
                    if (i < logs.size() - 1) json += ",";
                }
                json += "]";
                return jsonResponse(json);
            }
        }

        // ========== API: 配置 ==========
        if (parts.size() >= 3 && parts[0] == "api" && parts[1] == "config") {
            if (method == "GET") {
                auto val = DatabaseManager::instance().getConfig(parts[2]);
                std::string json = "{\"key\":\"" + escapeJson(parts[2]) +
                                   "\",\"value\":\"" + escapeJson(val) + "\"}";
                return jsonResponse(json);
            }
            if (method == "POST") {
                auto valPos = body.find("\"value\"");
                if (valPos == std::string::npos) return jsonResponse("{\"error\":\"value required\"}", 400);
                auto colon = body.find(':', valPos);
                auto start = body.find('"', colon + 1);
                if (start == std::string::npos) return jsonResponse("{\"error\":\"invalid value\"}", 400);
                auto end = body.find('"', start + 1);
                auto val = body.substr(start + 1, end - start - 1);
                DatabaseManager::instance().setConfig(parts[2], val);
                return jsonResponse("{\"status\":\"ok\"}");
            }
        }

        // ========== API: 数据源状态 ==========
        if (parts.size() >= 2 && parts[0] == "api" && parts[1] == "status") {
            std::string json = "{\"opcuaServer\":" +
                std::string(OPCUAManager::instance().isServerRunning() ? "true" : "false") +
                ",\"opcuaClient\":" +
                std::string(OPCUAManager::instance().isClientConnected() ? "true" : "false") +
                ",\"udpReceiver\":" +
                std::string(UDPManager::instance().isReceiving() ? "true" : "false") +
                ",\"database\":" +
                std::string(DatabaseManager::instance().isConnected() ? "true" : "false") +
                "}";
            return jsonResponse(json);
        }

        // ========== API: 局域网访问地址 ==========
        if (parts.size() >= 3 && parts[0] == "api" && parts[1] == "network" && parts[2] == "urls") {
            auto addresses = getLocalIPv4Addresses();
            addresses.erase(std::remove(addresses.begin(), addresses.end(), LAN_ACCESS_HOST), addresses.end());
            addresses.insert(addresses.begin(), LAN_ACCESS_HOST);

            std::string json = "{\"frontendPort\":" + std::to_string(port_) +
                               ",\"backendPort\":" + std::to_string(port_) +
                               ",\"devFrontendPort\":" + std::to_string(DEV_FRONTEND_PORT) +
                               ",\"primaryUrl\":\"http://" + std::string(LAN_ACCESS_HOST) + ":" + std::to_string(port_) +
                               "\",\"addresses\":[";
            for (size_t i = 0; i < addresses.size(); ++i) {
                json += "\"" + escapeJson(addresses[i]) + "\"";
                if (i < addresses.size() - 1) json += ",";
            }
            json += "],\"frontendUrls\":[";
            for (size_t i = 0; i < addresses.size(); ++i) {
                json += "\"http://" + escapeJson(addresses[i]) + ":" + std::to_string(port_) + "\"";
                if (i < addresses.size() - 1) json += ",";
            }
            json += "],\"devFrontendUrls\":[";
            for (size_t i = 0; i < addresses.size(); ++i) {
                json += "\"http://" + escapeJson(addresses[i]) + ":" + std::to_string(DEV_FRONTEND_PORT) + "\"";
                if (i < addresses.size() - 1) json += ",";
            }
            json += "],\"backendUrls\":[";
            for (size_t i = 0; i < addresses.size(); ++i) {
                json += "\"http://" + escapeJson(addresses[i]) + ":" + std::to_string(port_) + "\"";
                if (i < addresses.size() - 1) json += ",";
            }
            json += "]}";
            return jsonResponse(json);
        }

        // ========== API: DB历史数据查询 ==========
        if (parts.size() >= 3 && parts[0] == "api" && parts[1] == "historydb") {
            if (method == "GET") {
                if (parts[2] == "wide") {
                    auto table = DatabaseManager::instance().queryHistoricalWideData(
                        getQueryInt64("from", 0),
                        getQueryInt64("to", 0),
                        getQueryInt("max", 5000)
                    );
                    return jsonResponse(table.toJson());
                }

                int varId = std::stoi(parts[2]);
                auto history = DatabaseManager::instance().queryHistoricalData(
                    varId,
                    getQueryInt64("from", 0),
                    getQueryInt64("to", 0),
                    getQueryInt("max", 2000)
                );
                std::string json = "[";
                for (size_t i = 0; i < history.size(); ++i) {
                    json += history[i].toJson();
                    if (i < history.size() - 1) json += ",";
                }
                json += "]";
                return jsonResponse(json);
            }
        }

        // ========== API: 数据库连接测试 ==========
        if (parts.size() >= 3 && parts[0] == "api" && parts[1] == "db" && parts[2] == "test") {
            bool connected = DatabaseManager::instance().isConnected();
            std::string json = "{\"connected\":" + std::string(connected ? "true" : "false") +
                               ",\"message\":\"" + (connected ? "Database connected successfully" : "Database not connected") + "\"}";
            return jsonResponse(json);
        }

        // ========== API: 数据库动态连接 ==========
        if (parts.size() >= 3 && parts[0] == "api" && parts[1] == "db" && parts[2] == "connect") {
            if (method == "POST") {
                Log(LogLevel::DEBUG, "db/connect request received");

                auto extractStr = [&](const std::string& key) -> std::string {
                    // 查找 "key": 或 "key" : 模式
                    std::string searchKey = "\"" + key + "\"";
                    auto pos = body.find(searchKey);
                    if (pos == std::string::npos) return "";
                    auto colon = body.find(':', pos);
                    if (colon == std::string::npos) return "";
                    // 跳过冒号后的空白字符，找到第一个引号
                    auto start = body.find_first_of("\"", colon + 1);
                    if (start == std::string::npos) return "";
                    auto end = body.find('"', start + 1);
                    if (end == std::string::npos) return "";
                    std::string val = body.substr(start + 1, end - start - 1);
                    // 解码 JSON 转义序列
                    std::string decoded;
                    decoded.reserve(val.size());
                    for (size_t i = 0; i < val.size(); ++i) {
                        if (val[i] == '\\' && i + 1 < val.size()) {
                            switch (val[i + 1]) {
                                case '"': decoded += '"'; break;
                                case '\\': decoded += '\\'; break;
                                case '/': decoded += '/'; break;
                                case 'n': decoded += '\n'; break;
                                case 'r': decoded += '\r'; break;
                                case 't': decoded += '\t'; break;
                                default: decoded += val[i + 1]; break;
                            }
                            ++i;
                        } else {
                            decoded += val[i];
                        }
                    }
                    return decoded;
                };
                auto extractNum = [&](const std::string& key) -> int {
                    std::string searchKey = "\"" + key + "\"";
                    auto pos = body.find(searchKey);
                    if (pos == std::string::npos) return 0;
                    auto colon = body.find(':', pos);
                    if (colon == std::string::npos) return 0;
                    auto start = body.find_first_of("0123456789", colon);
                    if (start == std::string::npos) return 0;
                    auto end = body.find_first_not_of("0123456789", start);
                    if (end == std::string::npos) return std::stoi(body.substr(start));
                    return std::stoi(body.substr(start, end - start));
                };

                std::string host = extractStr("host");
                int port = extractNum("port");
                if (port == 0) port = 3306;
                std::string user = extractStr("user");
                std::string pass = extractStr("pass");
                std::string dbname = extractStr("dbname");
                if (host.empty()) host = "localhost";
                if (user.empty()) user = "root";
                if (pass.empty()) pass = "123456";
                if (dbname.empty()) dbname = "software_plc_monitoring";

                Log(LogLevel::DEBUG, "DB connect parsed: host=[" + host + "] port=" + std::to_string(port) +
                    " user=[" + user + "] pass=[***] db=[" + dbname + "]");

                bool ok = DatabaseManager::instance().connect(host, port, user, pass, dbname);
                // 连接成功后从数据库加载变量定义
                if (ok) {
                    VariableManager::instance().loadDefinitionsFromDB();
                }
                std::string json = "{\"connected\":" + std::string(ok ? "true" : "false") +
                                   ",\"message\":\"" + (ok ? "Connected successfully" : "Connection failed - check credentials and ensure MySQL is running") + "\"}";
                return jsonResponse(json);
            }
        }

        // ========== API: OPC UA 客户端连接 ==========
        if (parts.size() >= 3 && parts[0] == "api" && parts[1] == "opcua" && parts[2] == "connect") {
            if (method != "POST") return jsonResponse("{\"error\":\"POST required\"}", 405);
            std::string url = "opc.tcp://localhost:4840";
            auto urlPos = body.find("\"url\"");
            if (urlPos != std::string::npos) {
                auto colon = body.find(':', urlPos);
                auto start = body.find('"', colon + 1);
                if (start != std::string::npos) {
                    auto end = body.find('"', start + 1);
                    if (end != std::string::npos) url = body.substr(start + 1, end - start - 1);
                }
            }
            bool ok = OPCUAManager::instance().connectClient(url);
            std::string json = "{\"connected\":" + std::string(ok ? "true" : "false") +
                               ",\"message\":\"" + (ok ? "Connected to " + url : "Connection failed") + "\"}";
            return jsonResponse(json);
        }

        // ========== API: OPC UA 节点列表 ==========
        if (parts.size() >= 3 && parts[0] == "api" && parts[1] == "opcua" && parts[2] == "nodes") {
            auto vars = VariableManager::instance().getAllDefinitions();
            std::string json = "[";
            for (size_t i = 0; i < vars.size(); ++i) {
                const auto& v = vars[i];
                std::string path = "\\Root\\Objects\\" + v.name;
                json += "{\"id\":" + std::to_string(v.id) +
                        ",\"name\":\"" + escapeJson(v.name) +
                        "\",\"nodeId\":\"ns=1;s=" + escapeJson(v.name) +
                        "\",\"browsePath\":\"" + path +
                        "\",\"source\":\"" + v.source +
                        "\"}";
                if (i < vars.size() - 1) json += ",";
            }
            json += "]";
            return jsonResponse(json);
        }

        // ========== API: OPC UA 远程采集 ==========
        if (parts.size() >= 4 && parts[0] == "api" && parts[1] == "opcua" && parts[2] == "poll") {
            // POST /api/opcua/poll/start - 启动远程采集
            if (parts[3] == "start" && method == "POST") {
                // 改进版 extractStr（支持空白字符和格式化的 JSON）
                auto extractStr = [&](const std::string& key) -> std::string {
                    std::string searchKey = "\"" + key + "\"";
                    auto pos = body.find(searchKey);
                    if (pos == std::string::npos) return "";
                    auto colon = body.find(':', pos);
                    if (colon == std::string::npos) return "";
                    auto start = body.find_first_of("\"", colon + 1);
                    if (start == std::string::npos) return "";
                    auto end = body.find('"', start + 1);
                    if (end == std::string::npos) return "";
                    std::string val = body.substr(start + 1, end - start - 1);
                    std::string decoded;
                    decoded.reserve(val.size());
                    for (size_t i = 0; i < val.size(); ++i) {
                        if (val[i] == '\\' && i + 1 < val.size()) {
                            switch (val[i + 1]) {
                                case '"': decoded += '"'; break;
                                case '\\': decoded += '\\'; break;
                                case '/': decoded += '/'; break;
                                default: decoded += val[i + 1]; break;
                            }
                            ++i;
                        } else {
                            decoded += val[i];
                        }
                    }
                    return decoded;
                };

                std::string endpoint = extractStr("endpoint");
                int interval = 1000;
                auto intervalPos = body.find("\"interval\"");
                if (intervalPos != std::string::npos) {
                    auto colon = body.find(':', intervalPos);
                    if (colon != std::string::npos) {
                        auto start = body.find_first_of("0123456789", colon);
                        if (start != std::string::npos)
                            interval = std::stoi(body.substr(start));
                    }
                }

                // 解析节点数组
                std::vector<ResourceNode> nodes;
                auto nodesPos = body.find("\"nodes\"");
                if (nodesPos != std::string::npos) {
                    auto arrStart = body.find('[', nodesPos);
                    if (arrStart != std::string::npos) {
                        auto arrEnd = body.find(']', arrStart);
                        if (arrEnd != std::string::npos) {
                            std::string arr = body.substr(arrStart + 1, arrEnd - arrStart - 1);
                            // Simple parse: find "nodeId" and "name" in each object
                            size_t objPos = 0;
                            while (objPos < arr.size()) {
                                auto objStart = arr.find('{', objPos);
                                if (objStart == std::string::npos) break;
                                auto objEnd = arr.find('}', objStart);
                                if (objEnd == std::string::npos) break;
                                std::string obj = arr.substr(objStart, objEnd - objStart + 1);

                                ResourceNode rn;
                                auto nidKey = obj.find("\"nodeId\"");
                                if (nidKey != std::string::npos) {
                                    auto c = obj.find(':', nidKey);
                                    auto s = obj.find('"', c + 1);
                                    auto e = obj.find('"', s + 1);
                                    if (s != std::string::npos && e != std::string::npos)
                                        rn.nodeId = obj.substr(s + 1, e - s - 1);
                                }
                                auto nKey = obj.find("\"name\"");
                                if (nKey != std::string::npos) {
                                    auto c = obj.find(':', nKey);
                                    auto s = obj.find('"', c + 1);
                                    auto e = obj.find('"', s + 1);
                                    if (s != std::string::npos && e != std::string::npos)
                                        rn.name = obj.substr(s + 1, e - s - 1);
                                }
                                if (!rn.nodeId.empty() || !rn.name.empty()) {
                                    if (rn.name.empty()) rn.name = rn.nodeId;
                                    nodes.push_back(rn);
                                }
                                objPos = objEnd + 1;
                            }
                        }
                    }
                }

                bool started = OPCUAManager::instance().startRemoteCollection(endpoint, nodes, interval);
                std::string json = "{\"started\":" + std::string(started ? "true" : "false") +
                                  ",\"nodes\":" + std::to_string(nodes.size()) + "}";
                return jsonResponse(json);
            }
            // POST /api/opcua/poll/stop
            if (parts[3] == "stop" && method == "POST") {
                OPCUAManager::instance().stopRemoteCollection();
                return jsonResponse("{\"stopped\":true}");
            }
            // GET /api/opcua/poll/status
            if (parts[3] == "status" && method == "GET") {
                bool running = OPCUAManager::instance().isPolling();
                return jsonResponse("{\"polling\":" + std::string(running ? "true" : "false") + "}");
            }
        }

        // ========== API: UDP 采集控制 ==========
        if (parts.size() >= 3 && parts[0] == "api" && parts[1] == "udp" && parts[2] == "start") {
            if (method == "POST") {
                    // 从 body 提取地址和端口: {"address": "0.0.0.0", "port": 8888}
                    std::string bindAddr = "0.0.0.0";
                int port = 8888;
                    auto addrKey = body.find("\"address\"");
                    if (addrKey != std::string::npos) {
                        auto colon = body.find(':', addrKey);
                        if (colon != std::string::npos) {
                            auto start = body.find_first_of("\"", colon + 1);
                            if (start != std::string::npos) {
                                auto end = body.find_first_of("\"", start + 1);
                                if (end != std::string::npos) {
                                    bindAddr = body.substr(start + 1, end - start - 1);
                                }
                            }
                        }
                    }
                auto portKey = body.find("\"port\"");
                if (portKey != std::string::npos) {
                    auto colon = body.find(':', portKey);
                    if (colon != std::string::npos) {
                        auto start = body.find_first_of("0123456789", colon);
                        if (start != std::string::npos)
                            port = std::stoi(body.substr(start));
                    }
                }

                // 先停止旧的再启动新的
                if (UDPManager::instance().isReceiving()) {
                    UDPManager::instance().stopReceiver();
                }
                    bool ok = UDPManager::instance().startReceiver(bindAddr, (uint16_t)port);
                std::string json = "{\"started\":" + std::string(ok ? "true" : "false") +
                                       ",\"address\":\"" + bindAddr + "\",\"port\":" + std::to_string(port) + "}";
                return jsonResponse(json);
            }
        }
        if (parts.size() >= 3 && parts[0] == "api" && parts[1] == "udp" && parts[2] == "stop") {
            if (method == "POST") {
                UDPManager::instance().stopReceiver();
                return jsonResponse("{\"stopped\":true}");
            }
        }
        if (parts.size() >= 3 && parts[0] == "api" && parts[1] == "udp" && parts[2] == "status") {
            if (method == "GET") {
                bool running = UDPManager::instance().isReceiving();
                return jsonResponse("{\"receiving\":" + std::string(running ? "true" : "false") + "}");
            }
        }

        // ========== 前端静态资源 ==========
        if (method == "GET" && (parts.empty() || parts[0] != "api")) {
            return staticFileResponse(path);
        }

        // 404
        return jsonResponse("{\"error\":\"not found\",\"path\":\"" + escapeJson(path) + "\"}", 404);

    } catch (const std::exception& e) {
        Log(LogLevel::ERR, "Request handler exception: " + std::string(e.what()));
        return jsonResponse("{\"error\":\"internal error\"}", 500);
    }
}

// ============ 服务器主循环 ============

void HttpServer::serverLoop() {
    try {
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        Log(LogLevel::ERR, "HTTP Server WSAStartup failed");
        return;
    }

    listenSock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock_ == INVALID_SOCKET) {
        Log(LogLevel::ERR, "HTTP Server socket creation failed");
        WSACleanup();
        return;
    }

    int opt = 1;
    setsockopt(listenSock_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listenSock_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        Log(LogLevel::ERR, "HTTP Server bind failed on port " + std::to_string(port_));
        closesocket(listenSock_);
        listenSock_ = INVALID_SOCKET;
        WSACleanup();
        return;
    }

    if (listen(listenSock_, SOMAXCONN) == SOCKET_ERROR) {
        Log(LogLevel::ERR, "HTTP Server listen failed");
        closesocket(listenSock_);
        listenSock_ = INVALID_SOCKET;
        WSACleanup();
        return;
    }

    Log(LogLevel::INFO, "HTTP Server started on port " + std::to_string(port_));

    fd_set readSet;
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    while (running_) {
        FD_ZERO(&readSet);
        FD_SET(listenSock_, &readSet);

        int ret = select(0, &readSet, NULL, NULL, &tv);
        if (ret > 0 && FD_ISSET(listenSock_, &readSet)) {
            sockaddr_in clientAddr;
            int addrLen = sizeof(clientAddr);
            SOCKET clientSock = accept(listenSock_, (sockaddr*)&clientAddr, &addrLen);

            if (clientSock == INVALID_SOCKET) continue;

            char buffer[65536];
            int total = recv(clientSock, buffer, sizeof(buffer) - 1, 0);
            if (total <= 0) { closesocket(clientSock); continue; }
            buffer[total] = '\0';

            // 设置 3s 超时用于后续补读
            DWORD rcvto = 3000;
            setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&rcvto, sizeof(rcvto));

            std::string request(buffer);

            // 按 Content-Length 补读 body（处理 TCP 分片）
            std::string clLen = extractHeader(request, "Content-Length");
            if (!clLen.empty()) {
                try {
                int cLen = std::stoi(clLen);
                int hEnd = (int)request.find("\r\n\r\n");
                if (hEnd != (int)std::string::npos) {
                    int bOff = hEnd + 4;
                    int have = std::max(0, total - bOff);
                    while (have < cLen && total < (int)sizeof(buffer) - 1) {
                        int room = (int)sizeof(buffer) - 1 - total;
                        int want = std::min(cLen - have, room);
                        int r = recv(clientSock, buffer + total, want, 0);
                        if (r <= 0) break;
                        total += r;
                        have = total - bOff;
                    }
                    buffer[total] = '\0';
                    request = std::string(buffer, total);
                }
                } catch (...) {
                    Log(LogLevel::WARN, "Invalid Content-Length header: " + clLen);
                }
            }

            std::string method = extractMethod(request);
            std::string path = extractPath(request);
            std::string upgrade = extractHeader(request, "Upgrade");

            if (upgrade == "websocket") {
                if (performWebSocketHandshake(clientSock, request)) {
                    // HTTP body reads use a short timeout, but WebSocket is a
                    // long-lived idle connection. Clear the timeout after the
                    // protocol upgrade or the socket is closed every 3 seconds.
                    DWORD wsReceiveTimeout = 0;
                    setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO,
                               (const char*)&wsReceiveTimeout, sizeof(wsReceiveTimeout));

                    WSClient wsClient;
                    wsClient.sock = clientSock;
                    wsClient.handshakeDone = true;

                    bool isNew = true;
                    {
                        std::lock_guard lock(wsMutex_);
                        for (auto& c : wsClients_) {
                            if (c.sock == clientSock) { isNew = false; break; }
                        }
                        if (isNew) wsClients_.push_back(wsClient);
                    }
                    if (isNew) {
                        std::thread wsThread(&HttpServer::handleWSClient, this, clientSock);
                        wsThread.detach();
                    }
                } else {
                    closesocket(clientSock);
                }
            } else {
                std::string body = extractBody(request);
                std::string contentType = extractHeader(request, "Content-Type");
                auto queryParams = extractQueryParams(request);
                std::string authToken = extractBearerToken(request);
                std::string response = handleRequest(method, path, body, contentType, queryParams, authToken);
                send(clientSock, response.c_str(), (int)response.size(), 0);
                closesocket(clientSock);
            }
        }
    }

    closesocket(listenSock_);
    listenSock_ = INVALID_SOCKET;

    {
        std::lock_guard lock(wsMutex_);
        for (auto& client : wsClients_) {
            closesocket(client.sock);
        }
        wsClients_.clear();
    }

    WSACleanup();
    Log(LogLevel::INFO, "HTTP Server stopped");
    } catch (const std::exception& e) {
        Log(LogLevel::ERR, std::string("HTTP Server loop crashed: ") + e.what());
        if (listenSock_ != INVALID_SOCKET) { closesocket(listenSock_); listenSock_ = INVALID_SOCKET; }
        WSACleanup();
    } catch (...) {
        Log(LogLevel::ERR, "HTTP Server loop crashed: unknown exception");
        if (listenSock_ != INVALID_SOCKET) { closesocket(listenSock_); listenSock_ = INVALID_SOCKET; }
        WSACleanup();
    }
}
bool HttpServer::start(uint16_t port) {
    if (running_) return true;
    port_ = port;
    running_ = true;
    serverThread_ = std::thread(&HttpServer::serverLoop, this);
    // 等待服务器启动
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return true;
}

void HttpServer::stop() {
    running_ = false;
    if (serverThread_.joinable()) serverThread_.join();
}

bool HttpServer::isRunning() const {
    return running_;
}
