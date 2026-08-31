#ifndef MONITORING_OPCUA_MANAGER_H
#define MONITORING_OPCUA_MANAGER_H

#include "common.h"
#include "open62541.h"
#include <thread>
#include <atomic>

/**
 * OPCUAManager
 * 管理 OPC UA 服务器和客户端
 *
 * 作为 OPC UA 服务器对外提供数据（兼容 WinCC 架构）
 * 同时作为 OPC UA 客户端从其他 OPC UA 服务器读取数据
 */
class OPCUAManager {
public:
    static OPCUAManager& instance();

    // ---- 服务器端 ----
    bool startServer(uint16_t port = 4840);
    void stopServer();
    bool isServerRunning() const;

    // 更新服务器上的变量值（供内部数据变更时同步到 OPC UA 空间）
    bool updateServerVariable(const std::string& nodeId, double value);

    // ---- 客户端 ----
    bool connectClient(const std::string& endpointUrl);
    void disconnectClient();
    bool isClientConnected() const;

    // 从远程 OPC UA 服务器读取变量
    bool readVariable(const std::string& nodeId, double& value, std::string& quality);
    bool readNodeVariant(const std::string& nodeId, double& numericValue,
                         std::string& stringValue, bool& isString,
                         std::string& quality);

    // 写入变量到远程 OPC UA 服务器
    bool writeVariable(const std::string& nodeId, double value);

    // Use an isolated client for a communication resource write. Resource
    // collection owns a different client on its worker thread, so a command
    // must never reuse or race that connection. When requested, the same
    // isolated command session reads the node back after a successful write.
    bool writeRemoteValue(const std::string& endpointUrl, const std::string& nodeId,
                          double value, std::string& error,
                          double* readbackValue = nullptr,
                          std::string* readbackError = nullptr);

    // 启动轮询（后台线程）
    void startPolling(int intervalMs);
    void stopPolling();
    bool isPolling() const { return polling_; }

    // ---- 远程数据采集（从通信资源配置中读取） ----
    bool startRemoteCollection(const std::string& endpoint,
                               const std::vector<ResourceNode>& nodes,
                               int intervalMs);
    void stopRemoteCollection();

private:
    OPCUAManager();
    ~OPCUAManager();
    OPCUAManager(const OPCUAManager&) = delete;
    OPCUAManager& operator=(const OPCUAManager&) = delete;

    // OPC UA 服务器
    UA_Server* server_ = nullptr;
    UA_Boolean serverRunning_ = false;
    std::thread serverThread_;

    // OPC UA 客户端
    UA_Client* client_ = nullptr;
    bool clientConnected_ = false;

    // 轮询线程
    std::thread pollThread_;
    std::atomic<bool> polling_{false};
    std::vector<ResourceNode> remoteNodes_;
    std::string remoteEndpoint_;
    int pollIntervalMs_ = 1000;

    // 服务器运行包装函数
    static void serverRunLoop(UA_Server* server, UA_Boolean* running);
    // 浏览远程 OPC UA 服务端节点（调试用）
    void browseRemoteNodes();
    void logNamespaceArray();
    std::string namespaceUriForIndex(UA_UInt16 namespaceIndex);
};

#endif // MONITORING_OPCUA_MANAGER_H
