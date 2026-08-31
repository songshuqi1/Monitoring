#ifndef MONITORING_COMMUNICATION_MANAGER_H
#define MONITORING_COMMUNICATION_MANAGER_H

#include "common.h"
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>

/**
 * CommunicationManager
 * 软件定义通信引擎：
 * - 通信资源（OPC UA / UDP）由前端通过 REST 动态定义并持久化到服务端；
 * - 支持多个通信任务并发运行（每个任务独立线程 + 独立连接/套接字）；
 * - 任务按 id 启动 / 停止，并对外提供运行状态；
 * - 采集到的数据统一写入 VariableManager（自动创建变量）。
 */
class CommunicationManager {
public:
    static CommunicationManager& instance();

    // ---- 持久化 ----
    void load();   // 从 data/communication_resources.json 加载资源
    void save();   // 保存资源到 data/communication_resources.json

    // ---- 资源 CRUD ----
    std::vector<CommunicationResource> listResources() const;
    CommunicationResource getResource(const std::string& id) const;
    bool upsertResource(const CommunicationResource& res);
    bool removeResource(const std::string& id);

    // ---- 运行控制 ----
    bool startTask(const std::string& id);
    bool stopTask(const std::string& id, bool disableResource = true);
    bool isTaskRunning(const std::string& id) const;
    void startAllEnabled();
    void stopAll();
    bool testSdcEndpoint(const std::string& endpoint, int& variableCount, std::string& error) const;
    bool testModbusEndpoint(const std::string& host, int port, int timeoutMs, std::string& error) const;
    bool readSdcValue(const std::string& binding, double& value, std::string& error) const;
    bool writeSdcValue(const std::string& binding, double value, std::string& error) const;
    bool readModbusValue(const std::string& resourceId, const std::string& binding, double& value, std::string& error) const;
    bool writeModbusValue(const std::string& resourceId, const std::string& binding, double value, std::string& error) const;
    bool writeUdpValue(const std::string& resourceId, const std::string& variableName, double value, std::string& error) const;

    // ---- JSON ----
    std::string resourcesToJson() const;   // 返回资源数组（含 running 状态）
    static bool parseResourceJson(const std::string& json, CommunicationResource& out);

private:
    CommunicationManager() = default;
    ~CommunicationManager() { stopAll(); }
    CommunicationManager(const CommunicationManager&) = delete;
    CommunicationManager& operator=(const CommunicationManager&) = delete;

    struct TaskContext {
        std::string id;
        std::string type;
        std::atomic<bool> running{false};
        std::thread worker;
        void* clientHandle = nullptr;   // OPC UA: UA_Client*；UDP: SOCKET
    };

    mutable std::mutex mutex_;
    std::map<std::string, CommunicationResource> resources_;
    std::map<std::string, std::shared_ptr<TaskContext>> tasks_;

    static std::string resourcesToJsonSnapshot(const std::map<std::string, CommunicationResource>& resources,
                                               const std::map<std::string, std::shared_ptr<TaskContext>>& tasks);
    void startTaskUnlocked(const CommunicationResource& res);
    static void opcuaWorker(std::shared_ptr<TaskContext> ctx, CommunicationResource res);
    static void udpWorker(std::shared_ptr<TaskContext> ctx, CommunicationResource res);
    static void sdcWorker(std::shared_ptr<TaskContext> ctx, CommunicationResource res);
    static void modbusWorker(std::shared_ptr<TaskContext> ctx, CommunicationResource res);
};

#endif // MONITORING_COMMUNICATION_MANAGER_H
