#ifndef MONITORING_AUTH_MANAGER_H
#define MONITORING_AUTH_MANAGER_H

#include <cstdint>
#include <map>
#include <mutex>
#include <string>

/**
 * Local account authentication backed by auth_users.tsv.
 * Passwords are verified against the existing salted hash store; session
 * tokens are deliberately memory-only so a process restart revokes them.
 */
class AuthManager {
public:
    static AuthManager& instance();

    bool login(const std::string& username, const std::string& password,
               std::string& token, std::string& error);
    bool validateToken(const std::string& token, std::string& username);
    void logout(const std::string& token);

private:
    struct UserRecord {
        std::string username;
        std::string passwordSalt;
        std::string passwordHash;
        std::string status;
    };

    struct Session {
        std::string username;
        int64_t expiresAtMs = 0;
    };

    AuthManager() = default;
    void loadUsersLocked();
    void pruneExpiredSessionsLocked();
    std::string hashPassword(const std::string& password, const std::string& salt) const;
    std::string newToken() const;

    std::map<std::string, UserRecord> users_;
    std::map<std::string, Session> sessions_;
    std::mutex mutex_;
    bool loaded_ = false;
};

#endif // MONITORING_AUTH_MANAGER_H
