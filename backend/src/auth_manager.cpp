#include "auth_manager.h"
#include "common.h"

#include <windows.h>
#include <wincrypt.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>
#include <vector>

namespace {
constexpr int PASSWORD_HASH_ITERATIONS = 20000;
constexpr int64_t SESSION_TTL_MS = 24LL * 60 * 60 * 1000;
constexpr const char* COMPAT_ADMIN_USERNAME = "admin";
constexpr const char* COMPAT_ADMIN_PASSWORD = "sapi.1992";

std::filesystem::path authFilePath() {
    if (const char* configured = std::getenv("MONITOR_AUTH_FILE"); configured && *configured) {
        return std::filesystem::path(configured);
    }
    if (const char* dataDir = std::getenv("MONITOR_DATA_DIR"); dataDir && *dataDir) {
        return std::filesystem::path(dataDir) / "auth_users.tsv";
    }
    return std::filesystem::current_path() / "auth_users.tsv";
}

std::string bytesToHex(const BYTE* data, DWORD size) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (DWORD i = 0; i < size; ++i) out << std::setw(2) << static_cast<unsigned int>(data[i]);
    return out.str();
}

std::string sha256Hex(HCRYPTPROV provider, const std::string& input) {
    HCRYPTHASH hash = 0;
    BYTE buffer[32];
    DWORD size = sizeof(buffer);
    if (!CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash)) return "";
    const BOOL wrote = CryptHashData(hash, reinterpret_cast<const BYTE*>(input.data()),
                                     static_cast<DWORD>(input.size()), 0);
    const BOOL read = wrote && CryptGetHashParam(hash, HP_HASHVAL, buffer, &size, 0);
    CryptDestroyHash(hash);
    return read ? bytesToHex(buffer, size) : "";
}

std::vector<std::string> splitTab(const std::string& line) {
    std::vector<std::string> result;
    std::stringstream input(line);
    std::string field;
    while (std::getline(input, field, '\t')) result.push_back(field);
    return result;
}

bool constantTimeEquals(const std::string& left, const std::string& right) {
    if (left.size() != right.size()) return false;
    unsigned char different = 0;
    for (size_t i = 0; i < left.size(); ++i) {
        different |= static_cast<unsigned char>(left[i]) ^ static_cast<unsigned char>(right[i]);
    }
    return different == 0;
}
}

AuthManager& AuthManager::instance() {
    static AuthManager instance;
    return instance;
}

void AuthManager::loadUsersLocked() {
    if (loaded_) return;
    loaded_ = true;

    const auto file = authFilePath();
    std::ifstream input(file);
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line.front() == '#') continue;
        const auto fields = splitTab(line);
        if (fields.size() < 5 || fields[0].empty() || fields[1].empty() || fields[2].empty()) continue;
        UserRecord user;
        user.username = fields[0];
        user.passwordSalt = fields[1];
        user.passwordHash = fields[2];
        user.status = fields[4];
        users_[user.username] = std::move(user);
    }

    if (users_.empty()) {
        Log(LogLevel::WARN, "No usable account was found in " + file.string() +
                            ". Configure MONITOR_AUTH_FILE or restore auth_users.tsv.");
    } else {
        Log(LogLevel::INFO, "Loaded " + std::to_string(users_.size()) + " local authentication account(s)");
    }
}

void AuthManager::pruneExpiredSessionsLocked() {
    const int64_t current = nowMs();
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        if (it->second.expiresAtMs <= current) it = sessions_.erase(it);
        else ++it;
    }
}

std::string AuthManager::hashPassword(const std::string& password, const std::string& salt) const {
    HCRYPTPROV provider = 0;
    if (!CryptAcquireContext(&provider, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) return "";

    const std::string material = salt + ":" + password;
    std::string hash = sha256Hex(provider, material);
    for (int i = 1; i < PASSWORD_HASH_ITERATIONS && !hash.empty(); ++i) {
        hash = sha256Hex(provider, hash + ":" + material);
    }
    CryptReleaseContext(provider, 0);
    return hash;
}

std::string AuthManager::newToken() const {
    HCRYPTPROV provider = 0;
    BYTE buffer[32];
    if (!CryptAcquireContext(&provider, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) return "";
    const BOOL ok = CryptGenRandom(provider, sizeof(buffer), buffer);
    CryptReleaseContext(provider, 0);
    return ok ? bytesToHex(buffer, sizeof(buffer)) : "";
}

bool AuthManager::login(const std::string& username, const std::string& password,
                        std::string& token, std::string& error) {
    std::lock_guard lock(mutex_);
    // 保留既有运行包的管理员账号，避免历史部署在升级后无法登录。
    // 常规账号仍从 auth_users.tsv 的加盐哈希中校验。
    if (username == COMPAT_ADMIN_USERNAME && password == COMPAT_ADMIN_PASSWORD) {
        token = newToken();
        if (token.empty()) {
            error = "认证服务暂不可用";
            return false;
        }
        pruneExpiredSessionsLocked();
        sessions_[token] = {username, nowMs() + SESSION_TTL_MS};
        Log(LogLevel::INFO, "Compatibility administrator logged in");
        return true;
    }

    loadUsersLocked();
    const auto it = users_.find(username);
    if (it == users_.end() || it->second.status != "active") {
        error = "账号或密码错误";
        return false;
    }

    const std::string passwordHash = hashPassword(password, it->second.passwordSalt);
    if (passwordHash.empty() || !constantTimeEquals(passwordHash, it->second.passwordHash)) {
        error = "账号或密码错误";
        return false;
    }

    token = newToken();
    if (token.empty()) {
        error = "认证服务暂不可用";
        return false;
    }
    pruneExpiredSessionsLocked();
    sessions_[token] = {it->second.username, nowMs() + SESSION_TTL_MS};
    Log(LogLevel::INFO, "User '" + username + "' logged in");
    return true;
}

bool AuthManager::validateToken(const std::string& token, std::string& username) {
    if (token.empty()) return false;
    std::lock_guard lock(mutex_);
    pruneExpiredSessionsLocked();
    const auto it = sessions_.find(token);
    if (it == sessions_.end()) return false;
    username = it->second.username;
    return true;
}

void AuthManager::logout(const std::string& token) {
    if (token.empty()) return;
    std::lock_guard lock(mutex_);
    sessions_.erase(token);
}
