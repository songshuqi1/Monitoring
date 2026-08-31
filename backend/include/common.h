#ifndef MONITORING_COMMON_H
#define MONITORING_COMMON_H

#define _WIN32_WINNT 0x0600
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <mutex>
#include <atomic>
#include <functional>
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <ctime>

// 版本信息
constexpr const char* APP_NAME    = "MonitoringPlatform";
constexpr const char* APP_VERSION = "1.0.0";

inline std::tm localTimeFromTimeT(std::time_t value) {
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &value);
#else
    localtime_r(&value, &tm);
#endif
    return tm;
}

// 日志级别
enum class LogLevel {
    DEBUG = 0,
    INFO,
    WARN,
    ERR
};

inline void Log(LogLevel level, const std::string& msg) {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    auto tm  = localTimeFromTimeT(t);
    std::stringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    const char* levelStr[] = {"[DEBUG]", "[INFO]", "[WARN]", "[ERR]"};
    std::cout << ss.str() << " " << levelStr[static_cast<int>(level)] << " " << msg << std::endl;
}

// 变量信息结构体
struct VariableInfo {
    int         id          = 0;
    std::string name;
    std::string description;
    std::string dataType    = "FLOAT";
    std::string source      = "OPCUA";
    std::string opcuaNodeId;
    int         udpPort     = 0;
    std::string resourceId;
    std::string resourceName;
    std::string scopeId;
    std::string scopeName;
    double      minValue    = 0.0;
    double      maxValue    = 100.0;
    std::string unit;
    std::string color       = "#2196F3";
    bool        enabled     = true;
};

// 实时数据点
struct DataPoint {
    int         varId       = 0;
    std::string varName;
    double      value       = 0.0;
    std::string quality     = "GOOD";
    int64_t     timestampMs = 0;  // Unix timestamp milliseconds

    std::string toJson() const;
};

// 历史数据点
struct HistoryPoint {
    int         varId       = 0;
    std::string varName;
    double      value       = 0.0;
    std::string quality     = "GOOD";
    int64_t     timestampMs = 0;

    std::string toJson() const;
};

// 历史数据宽表行：同一时间点的多个变量横向展示
struct HistoryWideRow {
    int64_t id          = 0;
    int64_t timestampMs = 0;
    std::map<std::string, double> values;
};

struct HistoryWideTable {
    std::vector<std::string> columns;
    std::vector<HistoryWideRow> rows;

    std::string toJson() const;
};

// 报警记录
struct AlarmRecord {
    int64_t     id          = 0;
    int         varId       = 0;
    std::string varName;
    std::string alarmType;
    double      alarmValue  = 0.0;
    double      limitValue  = 0.0;
    int         severity    = 1;
    std::string message;
    bool        acknowledged = false;
    int64_t     occurredAtMs  = 0;
    int64_t     acknowledgedAtMs = 0;

    std::string toJson() const;
};

// 操作日志
struct OperationLog {
    int64_t     id = 0;
    int         varId = 0;
    std::string operationType;
    std::string varName;
    double      oldValue = 0.0;
    double      newValue = 0.0;
    std::string operator_;
    std::string detail;
    int64_t     operatedAtMs = 0;

    std::string toJson() const;
};

// JSON 辅助
inline std::string escapeJson(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;
        }
    }
    return out;
}

inline std::string DataPoint::toJson() const {
    std::stringstream ss;
    ss << "{\"varId\":" << varId
       << ",\"varName\":\"" << escapeJson(varName)
       << "\",\"value\":" << value
       << ",\"quality\":\"" << escapeJson(quality)
       << "\",\"timestamp\":" << timestampMs << "}";
    return ss.str();
}

inline std::string HistoryPoint::toJson() const {
    std::stringstream ss;
    ss << "{\"varId\":" << varId
       << ",\"varName\":\"" << escapeJson(varName)
       << "\",\"value\":" << value
       << ",\"quality\":\"" << escapeJson(quality)
       << "\",\"timestamp\":" << timestampMs << "}";
    return ss.str();
}

inline std::string HistoryWideTable::toJson() const {
    std::stringstream ss;
    ss << "{\"columns\":[";
    for (size_t i = 0; i < columns.size(); ++i) {
        ss << "\"" << escapeJson(columns[i]) << "\"";
        if (i < columns.size() - 1) ss << ",";
    }
    ss << "],\"rows\":[";

    for (size_t r = 0; r < rows.size(); ++r) {
        const auto& row = rows[r];
        ss << "{\"id\":" << row.id
           << ",\"time\":" << row.timestampMs;

        for (const auto& col : columns) {
            if (col == "id" || col == "time") continue;
            ss << ",\"" << escapeJson(col) << "\":";
            auto it = row.values.find(col);
            if (it == row.values.end()) {
                ss << "null";
            } else {
                ss << it->second;
            }
        }

        ss << "}";
        if (r < rows.size() - 1) ss << ",";
    }

    ss << "]}";
    return ss.str();
}

inline std::string AlarmRecord::toJson() const {
    std::stringstream ss;
    ss << "{\"id\":" << id
       << ",\"varId\":" << varId
       << ",\"varName\":\"" << escapeJson(varName)
       << "\",\"alarmType\":\"" << escapeJson(alarmType)
       << "\",\"alarmValue\":" << alarmValue
       << ",\"limitValue\":" << limitValue
       << ",\"severity\":" << severity
       << ",\"message\":\"" << escapeJson(message)
       << "\",\"acknowledged\":" << (acknowledged ? "true" : "false")
       << ",\"occurredAt\":" << occurredAtMs
       << ",\"acknowledgedAt\":" << acknowledgedAtMs << "}";
    return ss.str();
}

inline std::string OperationLog::toJson() const {
    std::stringstream ss;
    ss << "{\"id\":" << id
       << ",\"varId\":" << varId
       << ",\"operationType\":\"" << escapeJson(operationType)
       << "\",\"varName\":\"" << escapeJson(varName)
       << "\",\"oldValue\":" << oldValue
       << ",\"newValue\":" << newValue
       << ",\"operator\":\"" << escapeJson(operator_)
       << "\",\"detail\":\"" << escapeJson(detail)
       << "\",\"operatedAt\":" << operatedAtMs << "}";
    return ss.str();
}

// 时间戳工具
inline int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

inline std::string timestampToStr(int64_t ms) {
    auto t = static_cast<time_t>(ms / 1000);
    auto tm = localTimeFromTimeT(t);
    std::stringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    auto frac = ms % 1000;
    ss << '.' << std::setfill('0') << std::setw(3) << frac;
    return ss.str();
}

// 通信资源节点
struct ResourceNode {
    std::string nodeId;      // OPC UA NodeId, 如 "ns=1;s=temperature_1"
    std::string name;        // 显示名称
    std::string browsePath;  // 浏览路径, 如 "\Root\Objects\temperature_1"
};

// Modbus TCP point definition. Addresses are zero-based protocol addresses.
struct ModbusPoint {
    std::string name;
    int         address = 0;
    std::string functionCode = "holding_register"; // coil / discrete_input / holding_register / input_register
    std::string dataType = "uint16";               // bool / uint16 / int16 / uint32 / int32 / float32 / float32_swap
    double      scale = 1.0;
    std::string unit;
};

// 通信资源定义
struct CommunicationResource {
    std::string id;
    std::string name;
    std::string type;        // "OPCUA" / "UDP"
    std::string endpoint;    // OPC UA 端点 URL
    std::string address;     // UDP 绑定地址（默认 0.0.0.0）
    std::string udpCommandHost; // UDP 下发目标主机；留空时仅采集，不允许下发
    std::string sdcUrl;
    std::string modbusHost;
    std::string scopeId;
    std::string scopeName;
    int         port     = 0;    // UDP 端口
    int         udpCommandPort = 0;
    int         udpAckTimeoutMs = 2000;
    bool        udpRequireAck = false;
    int         pollIntervalMs = 1000;
    int         modbusUnitId = 1;
    int         modbusTimeoutMs = 2500;
    bool        enabled  = false;
    std::vector<ResourceNode> nodes;
    std::vector<ModbusPoint> modbusPoints;

    std::string toJson() const {
        std::stringstream ss;
        ss << "{\"id\":\"" << escapeJson(id)
           << "\",\"name\":\"" << escapeJson(name)
           << "\",\"type\":\"" << escapeJson(type)
           << "\",\"endpoint\":\"" << escapeJson(endpoint)
           << "\",\"address\":\"" << escapeJson(address)
           << "\",\"udpCommandHost\":\"" << escapeJson(udpCommandHost)
           << "\",\"sdcUrl\":\"" << escapeJson(sdcUrl)
           << "\",\"modbusHost\":\"" << escapeJson(modbusHost)
           << "\",\"scopeId\":\"" << escapeJson(scopeId)
           << "\",\"scopeName\":\"" << escapeJson(scopeName)
           << "\",\"port\":" << port
           << ",\"udpCommandPort\":" << udpCommandPort
           << ",\"udpAckTimeoutMs\":" << udpAckTimeoutMs
           << ",\"udpRequireAck\":" << (udpRequireAck ? "true" : "false")
           << ",\"pollIntervalMs\":" << pollIntervalMs
           << ",\"modbusUnitId\":" << modbusUnitId
           << ",\"modbusTimeoutMs\":" << modbusTimeoutMs
           << ",\"enabled\":" << (enabled ? "true" : "false")
           << ",\"nodes\":[";
        for (size_t i = 0; i < nodes.size(); ++i) {
            ss << "{\"nodeId\":\"" << escapeJson(nodes[i].nodeId)
               << "\",\"name\":\"" << escapeJson(nodes[i].name)
               << "\",\"browsePath\":\"" << escapeJson(nodes[i].browsePath) << "\"}";
            if (i < nodes.size() - 1) ss << ",";
        }
        ss << "],\"modbusPoints\":[";
        for (size_t i = 0; i < modbusPoints.size(); ++i) {
            const auto& point = modbusPoints[i];
            ss << "{\"name\":\"" << escapeJson(point.name)
               << "\",\"address\":" << point.address
               << ",\"functionCode\":\"" << escapeJson(point.functionCode)
               << "\",\"dataType\":\"" << escapeJson(point.dataType)
               << "\",\"scale\":" << std::setprecision(15) << point.scale
               << ",\"unit\":\"" << escapeJson(point.unit) << "\"}";
            if (i < modbusPoints.size() - 1) ss << ",";
        }
        ss << "]}";
        return ss.str();
    }
};

#endif // MONITORING_COMMON_H
