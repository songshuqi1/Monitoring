#include "database.h"

#ifdef MONITOR_NO_MYSQL

// ============================================================
// 桩实现 —— MySQL Connector/C 未安装，数据库功能不可用
// ============================================================

DatabaseManager& DatabaseManager::instance() {
    static DatabaseManager inst;
    return inst;
}

bool DatabaseManager::connect(const std::string&, int,
                               const std::string&, const std::string&,
                               const std::string&) {
    Log(LogLevel::WARN, "MySQL not available — database features disabled");
    return false;
}
void DatabaseManager::disconnect() {}
bool DatabaseManager::isConnected() const { return false; }

bool DatabaseManager::executeQuery(const std::string&) { return false; }

std::vector<VariableInfo> DatabaseManager::loadVariableDefinitions() { return {}; }
bool DatabaseManager::saveVariableDefinition(const VariableInfo&) { return false; }
bool DatabaseManager::deleteVariableDefinition(int) { return false; }
bool DatabaseManager::deleteVariableDefinitions(const std::vector<int>&) { return false; }
bool DatabaseManager::updateVariableDefinition(const VariableInfo&) { return false; }

bool DatabaseManager::upsertRealtimeData(int, const std::string&, double, const std::string&, int64_t) { return false; }
std::vector<DataPoint> DatabaseManager::loadAllRealtimeData() { return {}; }

bool DatabaseManager::insertHistoricalData(int, const std::string&, double, const std::string&, int64_t) { return false; }
bool DatabaseManager::insertHistoricalSnapshot(const std::vector<DataPoint>&, int64_t) { return false; }
std::vector<HistoryPoint> DatabaseManager::queryHistoricalData(int, int64_t, int64_t, int) { return {}; }
HistoryWideTable DatabaseManager::queryHistoricalWideData(int64_t, int64_t, int) { return {}; }
bool DatabaseManager::repairHistoricalVarNames() { return false; }

bool DatabaseManager::insertAlarm(const AlarmRecord&) { return false; }
bool DatabaseManager::acknowledgeAlarm(int64_t) { return false; }
std::vector<AlarmRecord> DatabaseManager::queryAlarms(int, int64_t, int) { return {}; }

bool DatabaseManager::insertOperationLog(const OperationLog&) { return false; }
std::vector<OperationLog> DatabaseManager::queryOperationLogs(int) { return {}; }
bool DatabaseManager::insertWriteRecord(int, const std::string&, double, const std::string&,
                                        const std::string&, const std::string&,
                                        const std::string&, int64_t) { return false; }

std::string DatabaseManager::getConfig(const std::string&, const std::string& defaultVal) { return defaultVal; }
bool DatabaseManager::setConfig(const std::string&, const std::string&) { return false; }

#else

// ============================================================
// 真实实现 —— MySQL Connector/C 可用
// ============================================================

#include <mysql.h>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <set>
#include <unordered_map>

static std::string escapeSql(MYSQL* conn, const std::string& s) {
    if (!conn || s.empty()) return s;
    std::string out;
    out.resize(s.size() * 2 + 1);
    unsigned long len = mysql_real_escape_string(conn, out.data(), s.c_str(), (unsigned long)s.size());
    out.resize(len);
    return out;
}

static std::string fallbackVariableName(int varId) {
    return "变量" + std::to_string(varId);
}

static std::string quoteIdentifier(const std::string& name) {
    std::string out = "`";
    for (char c : name) {
        if (c == '`') out += "``";
        else out += c;
    }
    out += "`";
    return out;
}

static bool tableExists(MYSQL* conn, const std::string& table) {
    std::string sql = "SHOW TABLES LIKE '" + escapeSql(conn, table) + "'";
    if (mysql_query(conn, sql.c_str()) != 0) return false;
    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) return false;
    bool exists = mysql_num_rows(res) > 0;
    mysql_free_result(res);
    return exists;
}

static bool columnExists(MYSQL* conn, const std::string& table, const std::string& column) {
    std::string sql = "SHOW COLUMNS FROM " + quoteIdentifier(table) +
                      " LIKE '" + escapeSql(conn, column) + "'";
    if (mysql_query(conn, sql.c_str()) != 0) return false;
    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) return false;
    bool exists = mysql_num_rows(res) > 0;
    mysql_free_result(res);
    return exists;
}

static std::vector<std::string> getTableColumns(MYSQL* conn, const std::string& table) {
    std::vector<std::string> columns;
    std::string sql = "SHOW COLUMNS FROM " + quoteIdentifier(table);
    if (mysql_query(conn, sql.c_str()) != 0) return columns;
    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) return columns;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        if (row[0]) columns.push_back(row[0]);
    }
    mysql_free_result(res);
    return columns;
}

static std::string getVariableNameLocked(MYSQL* conn, int varId, const std::string& preferred = "") {
    if (!preferred.empty()) return preferred;

    std::string name;
    std::string sql = "SELECT name FROM variable_definitions WHERE id=" + std::to_string(varId) + " LIMIT 1";
    if (mysql_query(conn, sql.c_str()) == 0) {
        MYSQL_RES* res = mysql_store_result(conn);
        if (res) {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row && row[0]) name = row[0];
            mysql_free_result(res);
        }
    }

    if (name.empty() && tableExists(conn, "realtime_data") &&
        columnExists(conn, "realtime_data", "var_id") &&
        columnExists(conn, "realtime_data", "var_name")) {
        sql = "SELECT var_name FROM realtime_data WHERE var_id=" + std::to_string(varId) + " LIMIT 1";
        if (mysql_query(conn, sql.c_str()) == 0) {
            MYSQL_RES* res = mysql_store_result(conn);
            if (res) {
                MYSQL_ROW row = mysql_fetch_row(res);
                if (row && row[0]) name = row[0];
                mysql_free_result(res);
            }
        }
    }
    return name.empty() ? fallbackVariableName(varId) : name;
}

static bool ensureWideHistoryTable(MYSQL* conn, const std::string& table = "historical_data") {
    const bool isRealtimeTable = table.rfind("realtime_data", 0) == 0;
    const bool isWriteTable = table.rfind("write_records", 0) == 0;
    const std::string uniqueKey = isRealtimeTable ? "uk_realtime_time" :
                                 (isWriteTable ? "uk_write_time" : "uk_history_time");
    const std::string indexKey = isRealtimeTable ? "idx_realtime_time" :
                                (isWriteTable ? "idx_write_time" : "idx_history_time");
    std::string sql =
        "CREATE TABLE IF NOT EXISTS " + quoteIdentifier(table) + " ("
        "id BIGINT AUTO_INCREMENT PRIMARY KEY,"
        "`time` DATETIME(3) NOT NULL,"
        "UNIQUE KEY " + quoteIdentifier(uniqueKey) + " (`time`),"
        "INDEX " + quoteIdentifier(indexKey) + " (`time`)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4";
    if (mysql_query(conn, sql.c_str()) != 0) {
        Log(LogLevel::ERR, "Create wide historical table failed: " + std::string(mysql_error(conn)));
        return false;
    }
    std::string addUnique = "ALTER TABLE " + quoteIdentifier(table) +
                            " ADD UNIQUE KEY " + quoteIdentifier(uniqueKey) + " (`time`)";
    mysql_query(conn, addUnique.c_str());
    return true;
}

static bool ensureHistoryValueColumn(MYSQL* conn, const std::string& table, const std::string& varName) {
    if (varName.empty()) return false;
    if (columnExists(conn, table, varName)) return true;

    std::string sql = "ALTER TABLE " + quoteIdentifier(table) +
                      " ADD COLUMN " + quoteIdentifier(varName) + " DOUBLE NULL";
    if (mysql_query(conn, sql.c_str()) != 0) {
        Log(LogLevel::ERR, "Add historical value column failed: " + std::string(mysql_error(conn)) +
            " | column=" + varName);
        return false;
    }
    return true;
}

static bool isLongHistoricalTable(MYSQL* conn) {
    return tableExists(conn, "historical_data") &&
           columnExists(conn, "historical_data", "var_id") &&
           columnExists(conn, "historical_data", "var_name") &&
           columnExists(conn, "historical_data", "value") &&
           columnExists(conn, "historical_data", "timestamp");
}

static bool isLongRealtimeTable(MYSQL* conn) {
    return tableExists(conn, "realtime_data") &&
           columnExists(conn, "realtime_data", "var_id") &&
           columnExists(conn, "realtime_data", "var_name") &&
           columnExists(conn, "realtime_data", "value") &&
           columnExists(conn, "realtime_data", "timestamp");
}

static bool isLongWriteRecordsTable(MYSQL* conn) {
    return tableExists(conn, "write_records") &&
           columnExists(conn, "write_records", "var_id") &&
           columnExists(conn, "write_records", "var_name") &&
           columnExists(conn, "write_records", "write_value") &&
           columnExists(conn, "write_records", "written_at");
}

static bool insertWideHistoricalValueLocked(MYSQL* conn, const std::string& table,
                                            const std::string& varName, double value,
                                            int64_t timestampMs) {
    if (!ensureHistoryValueColumn(conn, table, varName)) return false;

    std::string col = quoteIdentifier(varName);
    std::string sql =
        "INSERT INTO " + quoteIdentifier(table) + " (`time`, " + col + ") VALUES "
        "(FROM_UNIXTIME(" + std::to_string(timestampMs / 1000.0) + "), " + std::to_string(value) + ") "
        "ON DUPLICATE KEY UPDATE " + col + "=VALUES(" + col + ")";
    if (mysql_query(conn, sql.c_str()) != 0) {
        Log(LogLevel::ERR, "Insert wide historical value failed: " + std::string(mysql_error(conn)));
        return false;
    }
    return true;
}

static bool upsertWideRealtimeValueLocked(MYSQL* conn, const std::string& varName, double value, int64_t timestampMs) {
    if (!ensureWideHistoryTable(conn, "realtime_data")) return false;
    if (!ensureHistoryValueColumn(conn, "realtime_data", varName)) return false;

    std::string col = quoteIdentifier(varName);
    std::string sql =
        "INSERT INTO realtime_data (id, `time`, " + col + ") VALUES "
        "(1, FROM_UNIXTIME(" + std::to_string(timestampMs / 1000.0) + "), " + std::to_string(value) + ") "
        "ON DUPLICATE KEY UPDATE `time`=VALUES(`time`), " + col + "=VALUES(" + col + ")";
    if (mysql_query(conn, sql.c_str()) != 0) {
        Log(LogLevel::ERR, "Upsert wide realtime value failed: " + std::string(mysql_error(conn)));
        return false;
    }
    return true;
}

static bool migrateHistoricalTableToWide(MYSQL* conn) {
    if (!tableExists(conn, "historical_data")) {
        return ensureWideHistoryTable(conn);
    }
    if (!isLongHistoricalTable(conn)) {
        return ensureWideHistoryTable(conn);
    }

    std::string suffix = std::to_string(nowMs());
    std::string tmpTable = "historical_data_wide_tmp_" + suffix;
    std::string backupTable = "historical_data_long_backup_" + suffix;

    if (!ensureWideHistoryTable(conn, tmpTable)) return false;

    std::string sql =
        "SELECT h.var_id, "
        "COALESCE(NULLIF(h.var_name, ''), NULLIF(v.name, ''), CONCAT('变量', h.var_id)) AS var_name, "
        "h.value, UNIX_TIMESTAMP(h.timestamp)*1000 + MICROSECOND(h.timestamp)/1000 AS ts "
        "FROM historical_data h "
        "LEFT JOIN variable_definitions v ON v.id = h.var_id "
        "ORDER BY h.timestamp ASC, h.id ASC";
    if (mysql_query(conn, sql.c_str()) != 0) {
        Log(LogLevel::ERR, "Read old historical_data failed: " + std::string(mysql_error(conn)));
        return false;
    }

    struct OldHistoryRow {
        std::string varName;
        double value = 0.0;
        int64_t timestampMs = 0;
    };
    std::vector<OldHistoryRow> oldRows;

    MYSQL_RES* res = mysql_store_result(conn);
    if (res) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res))) {
            int varId = row[0] ? std::stoi(row[0]) : 0;
            OldHistoryRow oldRow;
            oldRow.varName = row[1] ? row[1] : fallbackVariableName(varId);
            oldRow.value = row[2] ? std::stod(row[2]) : 0.0;
            oldRow.timestampMs = row[3] ? std::stoll(row[3]) : 0;
            oldRows.push_back(oldRow);
        }
        mysql_free_result(res);
    }

    for (const auto& oldRow : oldRows) {
        insertWideHistoricalValueLocked(conn, tmpTable, oldRow.varName, oldRow.value, oldRow.timestampMs);
    }

    std::string renameSql =
        "RENAME TABLE historical_data TO " + quoteIdentifier(backupTable) +
        ", " + quoteIdentifier(tmpTable) + " TO historical_data";
    if (mysql_query(conn, renameSql.c_str()) != 0) {
        Log(LogLevel::ERR, "Rename historical wide table failed: " + std::string(mysql_error(conn)));
        return false;
    }

    Log(LogLevel::INFO, "Migrated historical_data to wide table; old table kept as " + backupTable);
    return true;
}

static bool migrateRealtimeTableToWide(MYSQL* conn) {
    if (!tableExists(conn, "realtime_data")) {
        return ensureWideHistoryTable(conn, "realtime_data");
    }
    if (!isLongRealtimeTable(conn)) {
        return ensureWideHistoryTable(conn, "realtime_data");
    }

    std::string suffix = std::to_string(nowMs());
    std::string tmpTable = "realtime_data_wide_tmp_" + suffix;
    std::string backupTable = "realtime_data_long_backup_" + suffix;

    if (!ensureWideHistoryTable(conn, tmpTable)) return false;

    struct OldRealtimeRow {
        std::string varName;
        double value = 0.0;
        int64_t timestampMs = 0;
    };
    std::vector<OldRealtimeRow> oldRows;
    int64_t snapshotTime = nowMs();

    std::string sql =
        "SELECT r.var_id, COALESCE(NULLIF(r.var_name, ''), NULLIF(v.name, ''), CONCAT('变量', r.var_id)), "
        "r.value, UNIX_TIMESTAMP(r.timestamp)*1000 + MICROSECOND(r.timestamp)/1000 "
        "FROM realtime_data r "
        "LEFT JOIN variable_definitions v ON v.id = r.var_id "
        "ORDER BY r.id ASC";
    if (mysql_query(conn, sql.c_str()) != 0) {
        Log(LogLevel::ERR, "Read old realtime_data failed: " + std::string(mysql_error(conn)));
        return false;
    }

    MYSQL_RES* res = mysql_store_result(conn);
    if (res) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res))) {
            int varId = row[0] ? std::stoi(row[0]) : 0;
            OldRealtimeRow oldRow;
            oldRow.varName = row[1] ? row[1] : fallbackVariableName(varId);
            oldRow.value = row[2] ? std::stod(row[2]) : 0.0;
            oldRow.timestampMs = row[3] ? std::stoll(row[3]) : snapshotTime;
            if (oldRow.timestampMs > snapshotTime) snapshotTime = oldRow.timestampMs;
            oldRows.push_back(oldRow);
        }
        mysql_free_result(res);
    }

    for (const auto& oldRow : oldRows) {
        insertWideHistoricalValueLocked(conn, tmpTable, oldRow.varName, oldRow.value, snapshotTime);
    }

    std::string renameSql =
        "RENAME TABLE realtime_data TO " + quoteIdentifier(backupTable) +
        ", " + quoteIdentifier(tmpTable) + " TO realtime_data";
    if (mysql_query(conn, renameSql.c_str()) != 0) {
        Log(LogLevel::ERR, "Rename realtime wide table failed: " + std::string(mysql_error(conn)));
        return false;
    }

    Log(LogLevel::INFO, "Migrated realtime_data to wide table; old table kept as " + backupTable);
    return true;
}

static bool migrateWriteRecordsTableToWide(MYSQL* conn) {
    if (!tableExists(conn, "write_records")) {
        return ensureWideHistoryTable(conn, "write_records");
    }
    if (!isLongWriteRecordsTable(conn)) {
        return ensureWideHistoryTable(conn, "write_records");
    }

    std::string suffix = std::to_string(nowMs());
    std::string tmpTable = "write_records_wide_tmp_" + suffix;
    std::string backupTable = "write_records_long_backup_" + suffix;

    if (!ensureWideHistoryTable(conn, tmpTable)) return false;

    struct OldWriteRow {
        std::string varName;
        double value = 0.0;
        int64_t timestampMs = 0;
    };
    std::vector<OldWriteRow> oldRows;

    std::string sql =
        "SELECT w.var_id, "
        "COALESCE(NULLIF(w.var_name, ''), NULLIF(v.name, ''), CONCAT('变量', w.var_id)) AS var_name, "
        "w.write_value, UNIX_TIMESTAMP(w.written_at)*1000 + MICROSECOND(w.written_at)/1000 AS ts "
        "FROM write_records w "
        "LEFT JOIN variable_definitions v ON v.id = w.var_id "
        "ORDER BY w.written_at ASC, w.id ASC";
    if (mysql_query(conn, sql.c_str()) != 0) {
        Log(LogLevel::ERR, "Read old write_records failed: " + std::string(mysql_error(conn)));
        return false;
    }

    MYSQL_RES* res = mysql_store_result(conn);
    if (res) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res))) {
            int varId = row[0] ? std::stoi(row[0]) : 0;
            OldWriteRow oldRow;
            oldRow.varName = row[1] ? row[1] : fallbackVariableName(varId);
            oldRow.value = row[2] ? std::stod(row[2]) : 0.0;
            oldRow.timestampMs = row[3] ? std::stoll(row[3]) : 0;
            oldRows.push_back(oldRow);
        }
        mysql_free_result(res);
    }

    for (const auto& oldRow : oldRows) {
        insertWideHistoricalValueLocked(conn, tmpTable, oldRow.varName, oldRow.value, oldRow.timestampMs);
    }

    std::string renameSql =
        "RENAME TABLE write_records TO " + quoteIdentifier(backupTable) +
        ", " + quoteIdentifier(tmpTable) + " TO write_records";
    if (mysql_query(conn, renameSql.c_str()) != 0) {
        Log(LogLevel::ERR, "Rename write_records wide table failed: " + std::string(mysql_error(conn)));
        return false;
    }

    Log(LogLevel::INFO, "Migrated write_records to wide table; old table kept as " + backupTable);
    return true;
}

DatabaseManager& DatabaseManager::instance() {
    static DatabaseManager inst;
    return inst;
}

bool DatabaseManager::connect(const std::string& host, int port,
                               const std::string& user, const std::string& pass,
                               const std::string& dbname) {
    std::lock_guard lock(dbMutex_);
    if (conn_) {
        mysql_close(conn_);
        conn_ = nullptr;
    }

    conn_ = mysql_init(nullptr);
    if (!conn_) {
        Log(LogLevel::ERR, "mysql_init failed");
        return false;
    }

    int timeout = 5;
    mysql_options(conn_, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

    MYSQL* ret = mysql_real_connect(conn_, host.c_str(), user.c_str(),
                                     pass.c_str(), dbname.c_str(), port, nullptr, 0);
    if (!ret) {
        Log(LogLevel::ERR, "MySQL connection failed: " + std::string(mysql_error(conn_)));
        mysql_close(conn_);
        conn_ = nullptr;
        return false;
    }

    mysql_set_character_set(conn_, "utf8mb4");
    // 自动建表
    initTables();
    Log(LogLevel::INFO, "Connected to MySQL at " + host + ":" + std::to_string(port) + "/" + dbname);
    return true;
}

void DatabaseManager::disconnect() {
    std::lock_guard lock(dbMutex_);
    if (conn_) {
        mysql_close(conn_);
        conn_ = nullptr;
        Log(LogLevel::INFO, "Disconnected from MySQL");
    }
}

bool DatabaseManager::isConnected() const {
    return conn_ != nullptr;
}

bool DatabaseManager::executeQuery(const std::string& sql) {
    if (!conn_) {
        Log(LogLevel::ERR, "Database not connected");
        return false;
    }
    if (mysql_query(conn_, sql.c_str()) != 0) {
        Log(LogLevel::ERR, "Query failed: " + std::string(mysql_error(conn_)) + " | SQL: " + sql);
        return false;
    }
    return true;
}

// ============ 自动建表 ============

void DatabaseManager::initTables() {
    const char* tables[] = {
        R"(CREATE TABLE IF NOT EXISTS variable_definitions (
            id              INT AUTO_INCREMENT PRIMARY KEY,
            name            VARCHAR(128)    NOT NULL UNIQUE,
            description     VARCHAR(256)    DEFAULT '',
            data_type       VARCHAR(32)     NOT NULL DEFAULT 'FLOAT',
            source          VARCHAR(32)     NOT NULL DEFAULT 'OPCUA',
            opcua_node_id   VARCHAR(256)    DEFAULT '',
            udp_port        INT             DEFAULT 0,
            resource_id     VARCHAR(160)    DEFAULT '',
            resource_name   VARCHAR(128)    DEFAULT '',
            scope_id        VARCHAR(160)    DEFAULT '',
            scope_name      VARCHAR(128)    DEFAULT '',
            min_value       DOUBLE          DEFAULT NULL,
            max_value       DOUBLE          DEFAULT NULL,
            unit            VARCHAR(32)     DEFAULT '',
            color           VARCHAR(16)     DEFAULT '#2196F3',
            enabled         TINYINT(1)      DEFAULT 1,
            created_at      DATETIME        DEFAULT CURRENT_TIMESTAMP,
            updated_at      DATETIME        DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
            INDEX idx_source (source),
            INDEX idx_enabled (enabled)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4)",

        R"(CREATE TABLE IF NOT EXISTS realtime_data (
            id              BIGINT AUTO_INCREMENT PRIMARY KEY,
            `time`          DATETIME(3)     NOT NULL,
            UNIQUE KEY uk_realtime_time (`time`),
            INDEX idx_realtime_time (`time`)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4)",

        R"(CREATE TABLE IF NOT EXISTS historical_data (
            id              BIGINT AUTO_INCREMENT PRIMARY KEY,
            `time`          DATETIME(3)     NOT NULL,
            UNIQUE KEY uk_history_time (`time`),
            INDEX idx_history_time (`time`)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4)",

        R"(CREATE TABLE IF NOT EXISTS alarm_records (
            id              BIGINT AUTO_INCREMENT PRIMARY KEY,
            var_id          INT             NOT NULL,
            var_name        VARCHAR(128)    NOT NULL,
            alarm_type      VARCHAR(32)     NOT NULL,
            alarm_value     DOUBLE          NOT NULL,
            limit_value     DOUBLE          NOT NULL,
            severity        INT             DEFAULT 1,
            message         VARCHAR(256)    DEFAULT '',
            acknowledged    TINYINT(1)      DEFAULT 0,
            occurred_at     DATETIME(3)     NOT NULL,
            acknowledged_at DATETIME(3)     DEFAULT NULL,
            INDEX idx_var_alarm (var_id, occurred_at),
            INDEX idx_ack (acknowledged)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4)",

        R"(CREATE TABLE IF NOT EXISTS operation_log (
            id              BIGINT AUTO_INCREMENT PRIMARY KEY,
            var_id          INT             DEFAULT NULL,
            operation_type  VARCHAR(32)     NOT NULL,
            var_name        VARCHAR(128)    DEFAULT '',
            old_value       DOUBLE          DEFAULT NULL,
            new_value       DOUBLE          DEFAULT NULL,
            operator        VARCHAR(64)     DEFAULT 'SYSTEM',
            detail          VARCHAR(512)    DEFAULT '',
            operated_at     DATETIME(3)     DEFAULT CURRENT_TIMESTAMP(3),
            INDEX idx_type (operation_type),
            INDEX idx_time (operated_at)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4)",

        R"(CREATE TABLE IF NOT EXISTS write_records (
            id              BIGINT AUTO_INCREMENT PRIMARY KEY,
            `time`          DATETIME(3)     NOT NULL,
            UNIQUE KEY uk_write_time (`time`),
            INDEX idx_write_time (`time`)
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4)",

        R"(CREATE TABLE IF NOT EXISTS user_config (
            id              INT AUTO_INCREMENT PRIMARY KEY,
            `key`           VARCHAR(128)    NOT NULL UNIQUE,
            `value`         TEXT,
            description     VARCHAR(256)    DEFAULT '',
            updated_at      DATETIME        DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4)"
    };

    int created = 0;
    for (const auto& sql : tables) {
        if (mysql_query(conn_, sql) == 0) created++;
    }

    // Existing installations already have variable_definitions.  Add the
    // ownership columns individually, so an upgrade does not require users
    // to recreate their database or lose historical data.
    const auto addVariableColumnIfMissing = [this](const char* column, const char* definition) {
        const std::string existsSql = "SELECT 1 FROM information_schema.COLUMNS WHERE "
            "TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'variable_definitions' AND COLUMN_NAME = '" +
            std::string(column) + "' LIMIT 1";
        if (mysql_query(conn_, existsSql.c_str()) != 0) return;
        MYSQL_RES* result = mysql_store_result(conn_);
        const bool exists = result && mysql_fetch_row(result);
        if (result) mysql_free_result(result);
        if (exists) return;
        const std::string alterSql = "ALTER TABLE variable_definitions ADD COLUMN " +
            std::string(column) + " " + definition;
        if (mysql_query(conn_, alterSql.c_str()) != 0) {
            Log(LogLevel::WARN, "Unable to add variable_definitions." + std::string(column) + ": " + mysql_error(conn_));
        }
    };
    addVariableColumnIfMissing("resource_id", "VARCHAR(160) DEFAULT ''");
    addVariableColumnIfMissing("resource_name", "VARCHAR(128) DEFAULT ''");
    addVariableColumnIfMissing("scope_id", "VARCHAR(160) DEFAULT ''");
    addVariableColumnIfMissing("scope_name", "VARCHAR(128) DEFAULT ''");

    if (!migrateHistoricalTableToWide(conn_)) {
        Log(LogLevel::WARN, "historical_data wide-table migration/check failed");
    }
    if (!migrateRealtimeTableToWide(conn_)) {
        Log(LogLevel::WARN, "realtime_data wide-table migration/check failed");
    }
    if (!migrateWriteRecordsTableToWide(conn_)) {
        Log(LogLevel::WARN, "write_records wide-table migration/check failed");
    }

    Log(LogLevel::INFO, "Database tables initialized (" + std::to_string(created) + "/" +
        std::to_string(sizeof(tables)/sizeof(tables[0])) + " OK)");
}

// ============ 变量定义 ============

std::vector<VariableInfo> DatabaseManager::loadVariableDefinitions() {
    std::vector<VariableInfo> result;
    std::lock_guard lock(dbMutex_);
    if (!conn_) return result;

    if (mysql_query(conn_, "SELECT id, name, description, data_type, source, "
                          "opcua_node_id, udp_port, resource_id, resource_name, scope_id, scope_name, "
                          "min_value, max_value, unit, color, enabled "
                          "FROM variable_definitions WHERE enabled = 1")) {
        Log(LogLevel::ERR, "loadVariableDefinitions query failed: " + std::string(mysql_error(conn_)));
        return result;
    }

    MYSQL_RES* res = mysql_store_result(conn_);
    if (!res) return result;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        VariableInfo v;
        int col = 0;
        v.id          = row[col] ? std::stoi(row[col]) : 0; col++;
        v.name        = row[col] ? row[col] : ""; col++;
        v.description = row[col] ? row[col] : ""; col++;
        v.dataType    = row[col] ? row[col] : "FLOAT"; col++;
        v.source      = row[col] ? row[col] : "OPCUA"; col++;
        v.opcuaNodeId = row[col] ? row[col] : ""; col++;
        v.udpPort     = row[col] ? std::stoi(row[col]) : 0; col++;
        v.resourceId  = row[col] ? row[col] : ""; col++;
        v.resourceName = row[col] ? row[col] : ""; col++;
        v.scopeId     = row[col] ? row[col] : ""; col++;
        v.scopeName   = row[col] ? row[col] : ""; col++;
        v.minValue    = row[col] ? std::stod(row[col]) : 0.0; col++;
        v.maxValue    = row[col] ? std::stod(row[col]) : 100.0; col++;
        v.unit        = row[col] ? row[col] : ""; col++;
        v.color       = row[col] ? row[col] : "#2196F3"; col++;
        v.enabled     = row[col] ? (std::stoi(row[col]) != 0) : true;
        result.push_back(v);
    }
    mysql_free_result(res);
    return result;
}

bool DatabaseManager::saveVariableDefinition(const VariableInfo& info) {
    std::lock_guard lock(dbMutex_);
    if (!conn_) return false;
    std::string sql;
    if (info.id > 0) {
        sql = "UPDATE variable_definitions SET name='" + escapeSql(conn_, info.name) +
              "', description='" + escapeSql(conn_, info.description) +
              "', data_type='" + escapeSql(conn_, info.dataType) +
              "', source='" + escapeSql(conn_, info.source) +
              "', opcua_node_id='" + escapeSql(conn_, info.opcuaNodeId) +
              "', udp_port=" + std::to_string(info.udpPort) +
              ", resource_id='" + escapeSql(conn_, info.resourceId) +
              "', resource_name='" + escapeSql(conn_, info.resourceName) +
              "', scope_id='" + escapeSql(conn_, info.scopeId) +
              "', scope_name='" + escapeSql(conn_, info.scopeName) +
               ", min_value=" + std::to_string(info.minValue) +
               ", max_value=" + std::to_string(info.maxValue) +
              ", unit='" + escapeSql(conn_, info.unit) +
              "', color='" + escapeSql(conn_, info.color) +
              "', enabled=" + (info.enabled ? "1" : "0") +
              " WHERE id=" + std::to_string(info.id);
    } else {
        sql = "INSERT INTO variable_definitions (name, description, data_type, source, "
              "opcua_node_id, udp_port, resource_id, resource_name, scope_id, scope_name, min_value, max_value, unit, color, enabled) VALUES ('" +
              escapeSql(conn_, info.name) + "','" + escapeSql(conn_, info.description) +
              "','" + escapeSql(conn_, info.dataType) + "','" + escapeSql(conn_, info.source) +
              "','" + escapeSql(conn_, info.opcuaNodeId) + "'," + std::to_string(info.udpPort) +
              ",'" + escapeSql(conn_, info.resourceId) + "','" + escapeSql(conn_, info.resourceName) +
              "','" + escapeSql(conn_, info.scopeId) + "','" + escapeSql(conn_, info.scopeName) + "'" +
               "," + std::to_string(info.minValue) +
               "," + std::to_string(info.maxValue) +
              ",'" + escapeSql(conn_, info.unit) + "','" + escapeSql(conn_, info.color) +
              "'," + (info.enabled ? "1" : "0") + ")";
    }
    return executeQuery(sql);
}

bool DatabaseManager::deleteVariableDefinition(int varId) {
    return deleteVariableDefinitions({varId});
}

bool DatabaseManager::deleteVariableDefinitions(const std::vector<int>& varIds) {
    std::vector<int> ids;
    std::set<int> uniqueIds;
    for (int id : varIds) {
        if (id > 0 && uniqueIds.insert(id).second) ids.push_back(id);
    }
    if (ids.empty()) return true;

    std::lock_guard lock(dbMutex_);
    if (!conn_) return false;

    std::string idList;
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i) idList += ",";
        idList += std::to_string(ids[i]);
    }

    std::vector<std::string> variableNames;
    const std::string namesSql = "SELECT name FROM variable_definitions WHERE id IN (" + idList + ")";
    if (mysql_query(conn_, namesSql.c_str()) != 0) {
        Log(LogLevel::ERR, "Unable to resolve variable names before delete: " + std::string(mysql_error(conn_)));
        return false;
    }
    if (MYSQL_RES* namesResult = mysql_store_result(conn_)) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(namesResult))) {
            if (row[0] && *row[0]) variableNames.emplace_back(row[0]);
        }
        mysql_free_result(namesResult);
    }

    const std::vector<std::string> metadataDeletes = {
        "DELETE FROM alarm_records WHERE var_id IN (" + idList + ")",
        "DELETE FROM operation_log WHERE var_id IN (" + idList + ")",
        "DELETE FROM variable_definitions WHERE id IN (" + idList + ")"
    };

    const std::vector<std::string> valueTables = {"realtime_data", "historical_data", "write_records"};
    for (const auto& table : valueTables) {
        if (!tableExists(conn_, table)) continue;
        const auto columns = getTableColumns(conn_, table);
        std::set<std::string> existingColumns(columns.begin(), columns.end());
        std::vector<std::string> toDrop;
        for (const auto& name : variableNames) {
            if (existingColumns.count(name)) toDrop.push_back(name);
        }
        if (toDrop.empty()) continue;

        std::string alter = "ALTER TABLE " + quoteIdentifier(table) + " ";
        for (size_t i = 0; i < toDrop.size(); ++i) {
            if (i) alter += ", ";
            alter += "DROP COLUMN " + quoteIdentifier(toDrop[i]);
        }
        if (mysql_query(conn_, alter.c_str()) != 0) {
            Log(LogLevel::ERR, "Unable to remove deleted variable data columns: " + std::string(mysql_error(conn_)));
            return false;
        }
    }

    for (const auto& sql : metadataDeletes) {
        if (mysql_query(conn_, sql.c_str()) != 0) {
            Log(LogLevel::ERR, "Variable delete query failed: " + std::string(mysql_error(conn_)));
            return false;
        }
    }
    return true;
}

bool DatabaseManager::updateVariableDefinition(const VariableInfo& info) {
    return saveVariableDefinition(info);
}

// ============ 实时数据 ============

bool DatabaseManager::upsertRealtimeData(int varId, const std::string& varName,
                                          double value, const std::string& quality,
                                          int64_t timestampMs) {
    std::lock_guard lock(dbMutex_);
    if (!conn_) return false;
    (void)quality;
    migrateRealtimeTableToWide(conn_);
    std::string storedVarName = getVariableNameLocked(conn_, varId, varName);
    return upsertWideRealtimeValueLocked(conn_, storedVarName, value, timestampMs);
}

std::vector<DataPoint> DatabaseManager::loadAllRealtimeData() {
    std::vector<DataPoint> result;
    std::lock_guard lock(dbMutex_);
    if (!conn_) return result;
    migrateRealtimeTableToWide(conn_);

    struct VarColumn {
        int id = 0;
        std::string name;
    };
    std::vector<VarColumn> vars;

    if (mysql_query(conn_, "SELECT id, name FROM variable_definitions WHERE enabled = 1 ORDER BY id ASC") != 0) {
        return result;
    }
    MYSQL_RES* res = mysql_store_result(conn_);
    if (!res) return result;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        VarColumn var;
        var.id = row[0] ? std::stoi(row[0]) : 0;
        var.name = row[1] ? row[1] : fallbackVariableName(var.id);
        vars.push_back(var);
    }
    mysql_free_result(res);

    vars.erase(std::remove_if(vars.begin(), vars.end(), [&](const VarColumn& var) {
        return !columnExists(conn_, "realtime_data", var.name);
    }), vars.end());
    if (vars.empty()) return result;

    std::string sql = "SELECT UNIX_TIMESTAMP(`time`)*1000 + MICROSECOND(`time`)/1000";
    for (const auto& var : vars) {
        sql += ", " + quoteIdentifier(var.name);
    }
    sql += " FROM realtime_data ORDER BY id ASC LIMIT 1";

    if (mysql_query(conn_, sql.c_str()) != 0) return result;
    res = mysql_store_result(conn_);
    if (!res) return result;

    row = mysql_fetch_row(res);
    if (row) {
        int64_t timestampMs = row[0] ? std::stoll(row[0]) : 0;
        for (size_t i = 0; i < vars.size(); ++i) {
            const char* value = row[i + 1];
            if (!value) continue;
            DataPoint dp;
            dp.varId = vars[i].id;
            dp.varName = vars[i].name;
            dp.value = std::stod(value);
            dp.quality = "GOOD";
            dp.timestampMs = timestampMs;
            result.push_back(dp);
        }
    }
    mysql_free_result(res);
    return result;
}

// ============ 历史数据 ============

bool DatabaseManager::insertHistoricalData(int varId, const std::string& varName,
                                            double value, const std::string& quality,
                                            int64_t timestampMs) {
    std::lock_guard lock(dbMutex_);
    if (!conn_) return false;
    (void)quality;
    migrateHistoricalTableToWide(conn_);
    std::string storedVarName = getVariableNameLocked(conn_, varId, varName);
    return insertWideHistoricalValueLocked(conn_, "historical_data", storedVarName, value, timestampMs);
}

bool DatabaseManager::insertHistoricalSnapshot(const std::vector<DataPoint>& points, int64_t timestampMs) {
    std::lock_guard lock(dbMutex_);
    if (!conn_ || points.empty()) return false;
    migrateHistoricalTableToWide(conn_);

    std::vector<std::pair<std::string, double>> values;
    values.reserve(points.size());
    for (const auto& point : points) {
        std::string name = getVariableNameLocked(conn_, point.varId, point.varName);
        if (name.empty()) continue;
        if (!ensureHistoryValueColumn(conn_, "historical_data", name)) continue;
        values.emplace_back(name, point.value);
    }
    if (values.empty()) return false;

    std::string columns = "`time`";
    std::string rowValues = "FROM_UNIXTIME(" + std::to_string(timestampMs / 1000.0) + ")";
    std::string updates;

    for (size_t i = 0; i < values.size(); ++i) {
        const auto& [name, value] = values[i];
        std::string col = quoteIdentifier(name);
        columns += ", " + col;
        rowValues += ", " + std::to_string(value);
        if (!updates.empty()) updates += ", ";
        updates += col + "=VALUES(" + col + ")";
    }

    std::string sql = "INSERT INTO historical_data (" + columns + ") VALUES (" + rowValues + ") "
                      "ON DUPLICATE KEY UPDATE " + updates;
    if (mysql_query(conn_, sql.c_str()) != 0) {
        Log(LogLevel::ERR, "Insert wide historical snapshot failed: " + std::string(mysql_error(conn_)));
        return false;
    }
    return true;
}

std::vector<HistoryPoint> DatabaseManager::queryHistoricalData(int varId, int64_t fromMs, int64_t toMs, int limit) {
    std::vector<HistoryPoint> result;
    std::lock_guard lock(dbMutex_);
    if (!conn_) return result;
    if (limit <= 0) limit = 2000;
    migrateHistoricalTableToWide(conn_);

    std::string varName = getVariableNameLocked(conn_, varId);
    if (!columnExists(conn_, "historical_data", varName)) return result;

    std::string sql =
        "SELECT id, " + quoteIdentifier(varName) + ", "
        "UNIX_TIMESTAMP(`time`)*1000 + MICROSECOND(`time`)/1000 AS ts "
        "FROM historical_data "
        "WHERE 1=1";
    if (fromMs > 0) sql += " AND `time` >= FROM_UNIXTIME(" + std::to_string(fromMs / 1000.0) + ")";
    if (toMs > 0) sql += " AND `time` <= FROM_UNIXTIME(" + std::to_string(toMs / 1000.0) + ")";
    sql += " AND " + quoteIdentifier(varName) + " IS NOT NULL ORDER BY `time` ASC LIMIT " + std::to_string(limit);

    if (mysql_query(conn_, sql.c_str()) != 0) {
        Log(LogLevel::ERR, "queryHistoricalData wide-table query failed: " + std::string(mysql_error(conn_)));
        return result;
    }

    MYSQL_RES* res = mysql_store_result(conn_);
    if (!res) return result;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        HistoryPoint hp;
        hp.varId = varId;
        hp.varName = varName;
        hp.value = row[1] ? std::stod(row[1]) : 0.0;
        hp.quality = "GOOD";
        hp.timestampMs = row[2] ? std::stoll(row[2]) : 0;
        result.push_back(hp);
    }
    mysql_free_result(res);
    return result;
}

bool DatabaseManager::repairHistoricalVarNames() {
    std::lock_guard lock(dbMutex_);
    if (!conn_) return false;
    return migrateHistoricalTableToWide(conn_);
}

HistoryWideTable DatabaseManager::queryHistoricalWideData(int64_t fromMs, int64_t toMs, int limit) {
    HistoryWideTable table;
    table.columns = {"id", "time"};

    std::lock_guard lock(dbMutex_);
    if (!conn_) return table;
    if (limit <= 0) limit = 5000;
    migrateHistoricalTableToWide(conn_);

    auto columns = getTableColumns(conn_, "historical_data");
    std::vector<std::string> valueColumns;
    for (const auto& column : columns) {
        if (column == "id" || column == "time") continue;
        valueColumns.push_back(column);
        table.columns.push_back(column);
    }

    std::string sql = "SELECT id, UNIX_TIMESTAMP(`time`)*1000 + MICROSECOND(`time`)/1000";
    for (const auto& column : valueColumns) {
        sql += ", " + quoteIdentifier(column);
    }
    sql += " FROM historical_data WHERE 1=1";
    if (fromMs > 0) sql += " AND `time` >= FROM_UNIXTIME(" + std::to_string(fromMs / 1000.0) + ")";
    if (toMs > 0) sql += " AND `time` <= FROM_UNIXTIME(" + std::to_string(toMs / 1000.0) + ")";
    sql += " ORDER BY `time` ASC LIMIT " + std::to_string(limit);

    if (mysql_query(conn_, sql.c_str()) != 0) {
        Log(LogLevel::ERR, "queryHistoricalWideData failed: " + std::string(mysql_error(conn_)));
        return table;
    }

    MYSQL_RES* res = mysql_store_result(conn_);
    if (!res) return table;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        HistoryWideRow wideRow;
        wideRow.id = row[0] ? std::stoll(row[0]) : 0;
        wideRow.timestampMs = row[1] ? std::stoll(row[1]) : 0;

        for (size_t i = 0; i < valueColumns.size(); ++i) {
            const char* value = row[i + 2];
            if (value) wideRow.values[valueColumns[i]] = std::stod(value);
        }
        table.rows.push_back(wideRow);
    }
    mysql_free_result(res);
    return table;
}

// ============ 报警 ============

bool DatabaseManager::insertAlarm(const AlarmRecord& alarm) {
    std::lock_guard lock(dbMutex_);
    char buf[8192];
    snprintf(buf, sizeof(buf),
        "INSERT INTO alarm_records (var_id, var_name, alarm_type, alarm_value, "
        "limit_value, severity, message, occurred_at) "
        "VALUES (%d, '%s', '%s', %f, %f, %d, '%s', FROM_UNIXTIME(%lld / 1000))",
        alarm.varId, escapeJson(alarm.varName).c_str(),
        alarm.alarmType.c_str(), alarm.alarmValue, alarm.limitValue,
        alarm.severity, escapeJson(alarm.message).c_str(),
        (long long)alarm.occurredAtMs);
    return executeQuery(buf);
}

bool DatabaseManager::acknowledgeAlarm(int64_t alarmId) {
    return executeQuery("UPDATE alarm_records SET acknowledged=1, "
                        "acknowledged_at=NOW() WHERE id=" + std::to_string(alarmId));
}

std::vector<AlarmRecord> DatabaseManager::queryAlarms(int varId, int64_t fromMs, int limit) {
    std::vector<AlarmRecord> result;
    std::lock_guard lock(dbMutex_);

    std::string sql = "SELECT id, var_id, var_name, alarm_type, alarm_value, "
                      "limit_value, severity, message, acknowledged, "
                      "UNIX_TIMESTAMP(occurred_at)*1000, "
                      "IFNULL(UNIX_TIMESTAMP(acknowledged_at)*1000, 0) "
                      "FROM alarm_records WHERE 1=1";
    if (varId > 0) sql += " AND var_id=" + std::to_string(varId);
    if (fromMs > 0) sql += " AND UNIX_TIMESTAMP(occurred_at)*1000 > " + std::to_string(fromMs);
    sql += " ORDER BY occurred_at DESC LIMIT " + std::to_string(limit);

    if (mysql_query(conn_, sql.c_str())) return result;
    MYSQL_RES* res = mysql_store_result(conn_);
    if (!res) return result;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        AlarmRecord ar;
        ar.id            = row[0]  ? std::stoll(row[0]) : 0;
        ar.varId         = row[1]  ? std::stoi(row[1]) : 0;
        ar.varName       = row[2]  ? row[2] : "";
        ar.alarmType     = row[3]  ? row[3] : "";
        ar.alarmValue    = row[4]  ? std::stod(row[4]) : 0.0;
        ar.limitValue    = row[5]  ? std::stod(row[5]) : 0.0;
        ar.severity      = row[6]  ? std::stoi(row[6]) : 0;
        ar.message       = row[7]  ? row[7] : "";
        ar.acknowledged  = row[8]  ? (std::stoi(row[8]) != 0) : false;
        ar.occurredAtMs  = row[9]  ? std::stoll(row[9]) : 0;
        ar.acknowledgedAtMs = row[10] ? std::stoll(row[10]) : 0;
        result.push_back(ar);
    }
    mysql_free_result(res);
    return result;
}

// ============ 操作日志 ============

bool DatabaseManager::insertOperationLog(const OperationLog& log) {
    std::lock_guard lock(dbMutex_);
    char buf[4096];
    snprintf(buf, sizeof(buf),
    "INSERT INTO operation_log (operation_type, var_id, var_name, old_value, new_value, operator, detail) "
        "VALUES ('%s', %d, '%s', %f, %f, '%s', '%s')",
        escapeJson(log.operationType).c_str(),
        log.varId,
        escapeJson(log.varName).c_str(),
        log.oldValue,
        log.newValue,
        escapeJson(log.operator_).c_str(),
        escapeJson(log.detail).c_str());
    return executeQuery(buf);
}

std::vector<OperationLog> DatabaseManager::queryOperationLogs(int limit) {
    std::vector<OperationLog> result;
    std::lock_guard lock(dbMutex_);

    std::string sql = "SELECT id, operation_type, var_id, var_name, old_value, new_value, operator, detail, "
                      "UNIX_TIMESTAMP(operated_at)*1000 "
                      "FROM operation_log ORDER BY id DESC LIMIT " + std::to_string(limit);

    if (mysql_query(conn_, sql.c_str())) return result;
    MYSQL_RES* res = mysql_store_result(conn_);
    if (!res) return result;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        OperationLog ol;
        ol.id           = row[0] ? std::stoll(row[0]) : 0;
        ol.operationType = row[1] ? row[1] : "";
        ol.varId        = row[2] ? std::stoi(row[2]) : 0;
        ol.varName      = row[3] ? row[3] : "";
        ol.oldValue     = row[4] ? std::stod(row[4]) : 0.0;
        ol.newValue     = row[5] ? std::stod(row[5]) : 0.0;
        ol.operator_    = row[6] ? row[6] : "";
        ol.detail       = row[7] ? row[7] : "";
        ol.operatedAtMs = row[8] ? std::stoll(row[8]) : 0;
        result.push_back(ol);
    }
    mysql_free_result(res);
    return result;
}

// ============ 下发记录 ============

bool DatabaseManager::insertWriteRecord(int varId, const std::string& varName, double value,
                                        const std::string& opcuaNodeId, const std::string& status,
                                        const std::string& operatorName, const std::string& detail,
                                        int64_t timestampMs) {
    std::lock_guard lock(dbMutex_);
    if (!conn_) return false;
    if (timestampMs <= 0) timestampMs = nowMs();
    (void)opcuaNodeId;
    (void)status;
    (void)operatorName;
    (void)detail;

    if (!migrateWriteRecordsTableToWide(conn_)) return false;
    std::string storedVarName = getVariableNameLocked(conn_, varId, varName);
    return insertWideHistoricalValueLocked(conn_, "write_records", storedVarName, value, timestampMs);
}

// ============ 配置 ============

std::string DatabaseManager::getConfig(const std::string& key, const std::string& defaultVal) {
    std::lock_guard lock(dbMutex_);
    std::string sql = "SELECT config_value FROM system_config WHERE config_key='" +
                      escapeJson(key) + "'";
    if (mysql_query(conn_, sql.c_str())) return defaultVal;
    MYSQL_RES* res = mysql_store_result(conn_);
    if (!res) return defaultVal;
    MYSQL_ROW row = mysql_fetch_row(res);
    std::string val = (row && row[0]) ? row[0] : defaultVal;
    mysql_free_result(res);
    return val;
}

bool DatabaseManager::setConfig(const std::string& key, const std::string& value) {
    std::lock_guard lock(dbMutex_);
    std::string sql = "INSERT INTO system_config (config_key, config_value) VALUES ('" +
                      escapeJson(key) + "', '" + escapeJson(value) +
                      "') ON DUPLICATE KEY UPDATE config_value='" + escapeJson(value) + "'";
    return executeQuery(sql);
}

#endif
