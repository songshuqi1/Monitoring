#include "udp_manager.h"
#include "variable_manager.h"

UDPManager& UDPManager::instance() {
    static UDPManager inst;
    return inst;
}

UDPManager::UDPManager() {}

UDPManager::~UDPManager() {
    stopReceiver();
    cleanupWinsock();
}

bool UDPManager::initWinsock() {
    if (winsockInitialized_) return true;
    int ret = WSAStartup(MAKEWORD(2, 2), &wsaData_);
    if (ret != 0) {
        Log(LogLevel::ERR, "WSAStartup failed: " + std::to_string(ret));
        return false;
    }
    winsockInitialized_ = true;
    return true;
}

void UDPManager::cleanupWinsock() {
    if (winsockInitialized_) {
        WSACleanup();
        winsockInitialized_ = false;
    }
}

bool UDPManager::startReceiver(const std::string& bindAddress, uint16_t port) {
    if (running_) return true;
    if (!initWinsock()) return false;

    sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_ == INVALID_SOCKET) {
        Log(LogLevel::ERR, "UDP socket creation failed: " + std::to_string(WSAGetLastError()));
        return false;
    }

    // 设置端口重用
    int opt = 1;
    setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (bindAddress.empty() || bindAddress == "0.0.0.0") {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        addr.sin_addr.s_addr = inet_addr(bindAddress.c_str());
    }

    if (bind(sock_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        Log(LogLevel::ERR, "UDP bind failed on port " + std::to_string(port) +
            " error: " + std::to_string(WSAGetLastError()));
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
        return false;
    }

    // 设置超时
    DWORD timeout = 1000; // 1秒
    setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    listenPort_ = port;
    running_ = true;
    recvThread_ = std::thread(&UDPManager::receiveLoop, this);

    Log(LogLevel::INFO, "UDP receiver started on " + bindAddress + ":" + std::to_string(port));
    return true;
}

void UDPManager::stopReceiver() {
    running_ = false;
    if (recvThread_.joinable()) recvThread_.join();
    if (sock_ != INVALID_SOCKET) {
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
    }
    Log(LogLevel::INFO, "UDP receiver stopped");
}

bool UDPManager::isReceiving() const {
    return running_;
}

bool UDPManager::sendData(const std::string& ip, uint16_t port, const std::string& data) {
    if (!initWinsock()) return false;

    SOCKET sendSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sendSock == INVALID_SOCKET) return false;

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    int sent = sendto(sendSock, data.c_str(), (int)data.size(), 0,
                      (sockaddr*)&addr, sizeof(addr));
    closesocket(sendSock);

    return sent == (int)data.size();
}

// 解析扁平 JSON 对象: {"key1": value1, "key2": value2, "s": "str", ...}
// 提取所有数值字段，按名称匹配 UDP 变量并更新值，未找到则自动创建
static void parseJsonUdpData(const std::string& data) {
    auto braceOpen = data.find('{');
    auto braceClose = data.find('}');
    if (braceOpen == std::string::npos || braceClose == std::string::npos) return;

    std::string inner = data.substr(braceOpen + 1, braceClose - braceOpen - 1);
    size_t pos = 0;
    auto& varMgr = VariableManager::instance();

    while (pos < inner.size()) {
        // 查找键名 (双引号包围)
        auto ks = inner.find('"', pos);
        if (ks == std::string::npos) break;
        auto ke = inner.find('"', ks + 1);
        if (ke == std::string::npos) break;
        std::string key = inner.substr(ks + 1, ke - ks - 1);

        // 查找冒号
        auto col = inner.find(':', ke + 1);
        if (col == std::string::npos) break;

        // 跳过空白找到值的起始位置
        auto vs = inner.find_first_not_of(" \t\r\n", col + 1);
        if (vs == std::string::npos) break;

        // 跳过大括号内的嵌套对象和数组
        if (inner[vs] == '{' || inner[vs] == '[') {
            char closing = (inner[vs] == '{') ? '}' : ']';
            int depth = 1;
            size_t scan = vs + 1;
            while (scan < inner.size() && depth > 0) {
                if (inner[scan] == inner[vs]) depth++;
                else if (inner[scan] == closing) depth--;
                scan++;
            }
            pos = scan;
            continue;
        }

        // 字符串值: 跳过 (如 "timestamp")
        if (inner[vs] == '"') {
            auto strEnd = inner.find('"', vs + 1);
            pos = (strEnd != std::string::npos) ? strEnd + 1 : inner.size();
            continue;
        }

        // 布尔值和 null: 跳过
        if (inner.substr(vs, 4) == "true" || inner.substr(vs, 5) == "false" || inner.substr(vs, 4) == "null") {
            auto ve = inner.find_first_of(",}", vs);
            pos = (ve != std::string::npos) ? ve + 1 : inner.size();
            continue;
        }

        // --- 数值: 整数或浮点数 ---
        auto ve = inner.find_first_of(",}", vs);
        if (ve == std::string::npos) ve = inner.size();
        std::string valStr = inner.substr(vs, ve - vs);
        // 去除首尾空格
        valStr.erase(0, valStr.find_first_not_of(" \t\r\n"));
        valStr.erase(valStr.find_last_not_of(" \t\r\n") + 1);

        if (!key.empty() && !valStr.empty()) {
            try {
                double value = std::stod(valStr);

                // 在已定义的 UDP 源变量中查找
                auto vars = varMgr.getAllDefinitions();
                bool found = false;
                for (const auto& v : vars) {
                    if (v.source == "UDP" && v.name == key) {
                        varMgr.updateValue(v.id, value, "GOOD");
                        varMgr.appendHistory(v.id, value, nowMs());
                        found = true;
                        break;
                    }
                }

                // 未找到则自动创建变量
                if (!found) {
                    VariableInfo newVar;
                    newVar.name = key;
                    newVar.description = "从 UDP 自动创建";
                    newVar.source = "UDP";
                    newVar.dataType = "FLOAT";
                    varMgr.addOrUpdateDefinition(newVar);
                    Log(LogLevel::INFO, "UDP auto-created variable: " + key);

                    // 重新加载并写入首次值
                    vars = varMgr.getAllDefinitions();
                    for (const auto& v : vars) {
                        if (v.name == key) {
                            varMgr.updateValue(v.id, value, "GOOD");
                            varMgr.appendHistory(v.id, value, nowMs());
                            break;
                        }
                    }
                }
            } catch (const std::exception&) {
                // 单个字段解析失败，跳过
            }
        }
        pos = ve + 1;
    }
}

void UDPManager::receiveLoop() {
    char buffer[65536];
    sockaddr_in senderAddr;
    int addrLen = sizeof(senderAddr);

    while (running_) {
        int bytes = recvfrom(sock_, buffer, sizeof(buffer) - 1, 0,
                             (sockaddr*)&senderAddr, &addrLen);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            std::string data(buffer);

            char ipStr[64];
            inet_ntop(AF_INET, &senderAddr.sin_addr, ipStr, sizeof(ipStr));
            uint16_t senderPort = ntohs(senderAddr.sin_port);

            // 回调通知
            if (dataCallback_) {
                dataCallback_(ipStr, senderPort, data);
            }

            // 尝试解析 JSON 格式: {"key1": value1, "key2": value2, ...}
            if (!data.empty() && (data[0] == '{' || data[0] == '[')) {
                parseJsonUdpData(data);
                continue;
            }

            // 尝试解析 UDP 数据格式: "varName=value" 或 "varId:value"
            // 支持格式: var_name=123.45
            auto eqPos = data.find('=');
            if (eqPos != std::string::npos) {
                std::string name = data.substr(0, eqPos);
                double value = std::stod(data.substr(eqPos + 1));

                // 查找变量ID
                auto vars = VariableManager::instance().getAllDefinitions();
                for (const auto& v : vars) {
                    if (v.source == "UDP" && v.name == name) {
                        VariableManager::instance().updateValue(v.id, value, "GOOD");
                        VariableManager::instance().appendHistory(v.id, value, nowMs());
                        break;
                    }
                }
            }
        }
    }
}
