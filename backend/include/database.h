#ifndef MONITORING_DATABASE_H
#define MONITORING_DATABASE_H

#include "common.h"

// MySQL 类型：有真实连接时直接包含头文件，无连接时前向声明
#ifdef MONITOR_NO_MYSQL
struct st_mysql;
typedef struct st_mysql MYSQL;
#else
#include <mysql.h>
#endif

/**
 * DatabaseManager
 * MySQL 数据库操作封装
 * 使用 MySQL Connector/C (libmysql) 直接连接
 */
class DatabaseManager {
public:
    static DatabaseManager& instance();

    bool connect(const std::string& host, int port,
                 const std::string& user, const std::string& pass,
                 const std::string& dbname);
    void disconnect();
    bool isConnected() const;

    // ---- 变量定义操作 ----
    std::vector<VariableInfo> loadVariableDefinitions();
    bool saveVariableDefinition(const VariableInfo& info);
    bool deleteVariableDefinition(int varId);
    bool deleteVariableDefinitions(const std::vector<int>& varIds);
    bool updateVariableDefinition(const VariableInfo& info);

    // ---- 实时数据操作 ----
    bool upsertRealtimeData(int varId, const std::string& varName, double value, const std::string& quality, int64_t timestampMs);
    std::vector<DataPoint> loadAllRealtimeData();

    // ---- 历史数据操作 ----
    bool insertHistoricalData(int varId, const std::string& varName, double value, const std::string& quality, int64_t timestampMs);
    bool insertHistoricalSnapshot(const std::vector<DataPoint>& points, int64_t timestampMs);
    std::vector<HistoryPoint> queryHistoricalData(int varId, int64_t fromMs, int64_t toMs, int limit = 2000);
    HistoryWideTable queryHistoricalWideData(int64_t fromMs = 0, int64_t toMs = 0, int limit = 5000);
    bool repairHistoricalVarNames();

    // ---- 报警操作 ----
    bool insertAlarm(const AlarmRecord& alarm);
    bool acknowledgeAlarm(int64_t alarmId);
    std::vector<AlarmRecord> queryAlarms(int varId = -1, int64_t fromMs = 0, int limit = 200);

    // ---- 操作日志 ----
    bool insertOperationLog(const OperationLog& log);
    std::vector<OperationLog> queryOperationLogs(int limit = 100);

    // ---- 下发记录 ----
    bool insertWriteRecord(int varId, const std::string& varName, double value,
                           const std::string& opcuaNodeId, const std::string& status,
                           const std::string& operatorName, const std::string& detail,
                           int64_t timestampMs);

    // ---- 配置操作 ----
    std::string getConfig(const std::string& key, const std::string& defaultVal = "");
    bool setConfig(const std::string& key, const std::string& value);

private:
    DatabaseManager() : conn_(nullptr) {}
    ~DatabaseManager() { disconnect(); }
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    bool executeQuery(const std::string& sql);
    void initTables();
    MYSQL* conn_;
    std::mutex dbMutex_;
};

#endif // MONITORING_DATABASE_H
