#ifndef MONITORING_VARIABLE_MANAGER_H
#define MONITORING_VARIABLE_MANAGER_H

#include "common.h"
#include <unordered_map>
#include <shared_mutex>
#include <deque>

/**
 * VariableManager
 * 管理所有监控变量的定义、实时值和历史数据缓存
 * 充当 OPC UA / UDP 数据源与前端展示之间的桥梁
 */
class VariableManager {
public:
    static VariableManager& instance();

    // ---- 变量定义 ----
    bool loadDefinitionsFromDB();                            // 从MySQL加载变量定义
    bool addOrUpdateDefinition(const VariableInfo& info);   // 添加/更新变量定义
    bool removeDefinition(int varId);
    std::vector<int> removeDefinitions(const std::vector<int>& varIds);
    std::vector<int> removeOfflineDefinitions();
    std::vector<VariableInfo> getAllDefinitions() const;
    VariableInfo getDefinition(int varId) const;

    // ---- 实时数据 ----
    void updateValue(int varId, double value, const std::string& quality = "GOOD");
    DataPoint getLatestData(int varId) const;
    std::vector<DataPoint> getAllLatestData() const;

    // ---- 历史数据缓存（内存环形缓冲区，用于前端快速展示） ----
    void appendHistory(int varId, double value, int64_t timestampMs);
    std::vector<HistoryPoint> getHistory(int varId, int64_t fromMs = 0, int64_t toMs = 0, int maxPoints = 500) const;
    void clearHistory(int varId);

    // ---- 写入数据库 ----
    bool flushToDatabase();   // 批量写入实时快照和历史数据到MySQL

    // ---- 回调机制 ----
    using DataCallback = std::function<void(const DataPoint&)>;
    void setDataCallback(DataCallback cb) { dataCallback_ = cb; }

private:
    VariableManager() = default;
    ~VariableManager() = default;
    VariableManager(const VariableManager&) = delete;
    VariableManager& operator=(const VariableManager&) = delete;

    // 变量定义
    mutable std::shared_mutex defMutex_;
    std::unordered_map<int, VariableInfo> definitions_;
    std::unordered_map<std::string, int> nameToId_;

    // 实时数据
    mutable std::shared_mutex rtMutex_;
    std::unordered_map<int, DataPoint> realtimeData_;

    // 历史数据缓存（每个变量保留最近 10000 个点）
    static constexpr size_t MAX_HISTORY_POINTS = 10000;
    mutable std::shared_mutex histMutex_;
    std::unordered_map<int, std::deque<HistoryPoint>> historyCache_;

    // 回调
    DataCallback dataCallback_;
};

#endif // MONITORING_VARIABLE_MANAGER_H
