#ifndef MONITORING_UDP_MANAGER_H
#define MONITORING_UDP_MANAGER_H

#include "common.h"
#include <thread>
#include <atomic>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

/**
 * UDPManager
 * UDP 通信管理器：
 * - 监听指定端口接收 PLC/设备数据
 * - 发送数据到指定地址
 */
class UDPManager {
public:
    static UDPManager& instance();

        bool startReceiver(const std::string& bindAddress, uint16_t port);
    void stopReceiver();
    bool isReceiving() const;

    // 发送 UDP 数据
    bool sendData(const std::string& ip, uint16_t port, const std::string& data);

    // 注册回调：收到数据时通知
    using UDPDataCallback = std::function<void(const std::string& senderIp, uint16_t senderPort, const std::string& data)>;
    void setDataCallback(UDPDataCallback cb) { dataCallback_ = cb; }

private:
    UDPManager();
    ~UDPManager();
    UDPManager(const UDPManager&) = delete;
    UDPManager& operator=(const UDPManager&) = delete;

    void receiveLoop();

    SOCKET sock_ = INVALID_SOCKET;
    std::thread recvThread_;
    std::atomic<bool> running_{false};
    uint16_t listenPort_ = 8888;
    UDPDataCallback dataCallback_;

    // Windows socket 初始化/清理
    bool initWinsock();
    void cleanupWinsock();
    WSADATA wsaData_;
    bool winsockInitialized_ = false;
};

#endif // MONITORING_UDP_MANAGER_H
