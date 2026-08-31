#include "communication_manager.h"
#include "variable_manager.h"
#include "open62541.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <regex>
#include <sstream>

CommunicationManager& CommunicationManager::instance() {
    static CommunicationManager inst;
    return inst;
}

// ==================== 通用工具 ====================

static std::string trimCopy(const std::string& value) {
    auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch);
    });
    auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
        return std::isspace(ch);
    }).base();
    if (begin >= end) return "";
    return std::string(begin, end);
}

static std::string uaStringToStd(const UA_String& value) {
    if (!value.data || value.length == 0) return "";
    return std::string(reinterpret_cast<const char*>(value.data), value.length);
}

static bool jsonExtractString(const std::string& body, const std::string& key, std::string& out) {
    std::string searchKey = "\"" + key + "\"";
    auto pos = body.find(searchKey);
    if (pos == std::string::npos) return false;
    auto colon = body.find(':', pos);
    if (colon == std::string::npos) return false;
    auto start = body.find('"', colon + 1);
    if (start == std::string::npos) return false;
    auto end = body.find('"', start + 1);
    if (end == std::string::npos) return false;
    std::string val = body.substr(start + 1, end - start - 1);
    // 解码常见 JSON 转义
    std::string decoded;
    decoded.reserve(val.size());
    for (size_t i = 0; i < val.size(); ++i) {
        if (val[i] == '\\' && i + 1 < val.size()) {
            switch (val[i + 1]) {
                case '"': decoded += '"'; break;
                case '\\': decoded += '\\'; break;
                case '/': decoded += '/'; break;
                case 'n': decoded += '\n'; break;
                case 'r': decoded += '\r'; break;
                case 't': decoded += '\t'; break;
                default: decoded += val[i + 1]; break;
            }
            ++i;
        } else {
            decoded += val[i];
        }
    }
    out = decoded;
    return true;
}

static int jsonExtractInt(const std::string& body, const std::string& key, int fallback) {
    std::string searchKey = "\"" + key + "\"";
    auto pos = body.find(searchKey);
    if (pos == std::string::npos) return fallback;
    auto colon = body.find(':', pos);
    if (colon == std::string::npos) return fallback;
    auto start = body.find_first_of("-0123456789", colon);
    if (start == std::string::npos) return fallback;
    try { return std::stoi(body.substr(start)); } catch (...) { return fallback; }
}

static bool jsonExtractBool(const std::string& body, const std::string& key, bool fallback) {
    std::string searchKey = "\"" + key + "\"";
    auto pos = body.find(searchKey);
    if (pos == std::string::npos) return fallback;
    auto colon = body.find(':', pos);
    if (colon == std::string::npos) return fallback;
    auto start = body.find_first_not_of(" \t\r\n", colon + 1);
    if (start == std::string::npos) return fallback;
    if (body.compare(start, 4, "true") == 0) return true;
    if (body.compare(start, 5, "false") == 0) return false;
    return fallback;
}

static std::vector<ResourceNode> parseNodeArray(const std::string& body) {
    std::vector<ResourceNode> nodes;
    auto nodesPos = body.find("\"nodes\"");
    if (nodesPos == std::string::npos) return nodes;
    auto arrStart = body.find('[', nodesPos);
    if (arrStart == std::string::npos) return nodes;

    bool inString = false;
    bool escaped = false;
    int depth = 0;
    size_t arrEnd = std::string::npos;
    for (size_t i = arrStart; i < body.size(); ++i) {
        const char ch = body[i];
        if (inString) {
            if (escaped) escaped = false;
            else if (ch == '\\') escaped = true;
            else if (ch == '"') inString = false;
            continue;
        }
        if (ch == '"') inString = true;
        else if (ch == '[') ++depth;
        else if (ch == ']' && --depth == 0) {
            arrEnd = i;
            break;
        }
    }
    if (arrEnd == std::string::npos) return nodes;
    std::string arr = body.substr(arrStart + 1, arrEnd - arrStart - 1);

    size_t objPos = 0;
    while (objPos < arr.size()) {
        auto objStart = arr.find('{', objPos);
        if (objStart == std::string::npos) break;
        inString = false;
        escaped = false;
        depth = 0;
        size_t objEnd = std::string::npos;
        for (size_t i = objStart; i < arr.size(); ++i) {
            const char ch = arr[i];
            if (inString) {
                if (escaped) escaped = false;
                else if (ch == '\\') escaped = true;
                else if (ch == '"') inString = false;
                continue;
            }
            if (ch == '"') inString = true;
            else if (ch == '{') ++depth;
            else if (ch == '}' && --depth == 0) {
                objEnd = i;
                break;
            }
        }
        if (objEnd == std::string::npos) break;
        std::string obj = arr.substr(objStart, objEnd - objStart + 1);

        ResourceNode rn;
        jsonExtractString(obj, "nodeId", rn.nodeId);
        jsonExtractString(obj, "name", rn.name);
        jsonExtractString(obj, "browsePath", rn.browsePath);
        if (rn.name.empty() && !rn.nodeId.empty()) rn.name = rn.nodeId;
        if (!rn.nodeId.empty() || !rn.name.empty()) nodes.push_back(rn);

        objPos = objEnd + 1;
    }
    return nodes;
}

static double jsonExtractDouble(const std::string& body, const std::string& key, double fallback);

static std::vector<ModbusPoint> parseModbusPointArray(const std::string& body) {
    std::vector<ModbusPoint> points;
    auto pointsPos = body.find("\"modbusPoints\"");
    if (pointsPos == std::string::npos) return points;
    auto arrStart = body.find('[', pointsPos);
    if (arrStart == std::string::npos) return points;
    bool inString = false, escaped = false;
    int depth = 0;
    size_t arrEnd = std::string::npos;
    for (size_t i = arrStart; i < body.size(); ++i) {
        const char ch = body[i];
        if (inString) {
            if (escaped) escaped = false;
            else if (ch == '\\') escaped = true;
            else if (ch == '"') inString = false;
            continue;
        }
        if (ch == '"') inString = true;
        else if (ch == '[') ++depth;
        else if (ch == ']' && --depth == 0) { arrEnd = i; break; }
    }
    if (arrEnd == std::string::npos) return points;
    const std::string arr = body.substr(arrStart + 1, arrEnd - arrStart - 1);
    size_t pos = 0;
    while (pos < arr.size()) {
        const auto start = arr.find('{', pos);
        if (start == std::string::npos) break;
        bool objectString = false, objectEscaped = false;
        int objectDepth = 0;
        size_t end = std::string::npos;
        for (size_t i = start; i < arr.size(); ++i) {
            const char ch = arr[i];
            if (objectString) {
                if (objectEscaped) objectEscaped = false;
                else if (ch == '\\') objectEscaped = true;
                else if (ch == '"') objectString = false;
                continue;
            }
            if (ch == '"') objectString = true;
            else if (ch == '{') ++objectDepth;
            else if (ch == '}' && --objectDepth == 0) { end = i; break; }
        }
        if (end == std::string::npos) break;
        const std::string object = arr.substr(start, end - start + 1);
        ModbusPoint point;
        jsonExtractString(object, "name", point.name);
        point.address = jsonExtractInt(object, "address", 0);
        jsonExtractString(object, "functionCode", point.functionCode);
        jsonExtractString(object, "dataType", point.dataType);
        point.scale = jsonExtractDouble(object, "scale", 1.0);
        jsonExtractString(object, "unit", point.unit);
        if (!point.name.empty()) points.push_back(point);
        pos = end + 1;
    }
    return points;
}

bool CommunicationManager::parseResourceJson(const std::string& json, CommunicationResource& out) {
    if (json.empty()) return false;
    std::string s = trimCopy(json);
    if (s.empty() || s[0] != '{') return false;

    jsonExtractString(s, "id", out.id);
    jsonExtractString(s, "name", out.name);
    jsonExtractString(s, "type", out.type);
    jsonExtractString(s, "endpoint", out.endpoint);
    jsonExtractString(s, "address", out.address);
    jsonExtractString(s, "udpCommandHost", out.udpCommandHost);
    jsonExtractString(s, "sdcUrl", out.sdcUrl);
    jsonExtractString(s, "modbusHost", out.modbusHost);
    jsonExtractString(s, "scopeId", out.scopeId);
    jsonExtractString(s, "scopeName", out.scopeName);
    out.port = jsonExtractInt(s, "port", 0);
    out.udpCommandPort = jsonExtractInt(s, "udpCommandPort", 0);
    out.udpAckTimeoutMs = std::clamp(jsonExtractInt(s, "udpAckTimeoutMs", 2000), 100, 15000);
    out.udpRequireAck = jsonExtractBool(s, "udpRequireAck", false);
    out.modbusUnitId = std::clamp(jsonExtractInt(s, "modbusUnitId", 1), 0, 255);
    out.modbusTimeoutMs = std::clamp(jsonExtractInt(s, "modbusTimeoutMs", 2500), 300, 15000);
    out.pollIntervalMs = jsonExtractInt(s, "pollIntervalMs", 1000);
    out.pollIntervalMs = std::clamp(out.pollIntervalMs, 50, 600000);
    out.enabled = jsonExtractBool(s, "enabled", false);
    out.nodes = parseNodeArray(s);
    out.modbusPoints = parseModbusPointArray(s);

    std::transform(out.type.begin(), out.type.end(), out.type.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    if (out.type != "OPCUA" && out.type != "UDP" && out.type != "SDC" && out.type != "MODBUS") return false;
    if (out.type == "UDP" && (out.port <= 0 || out.port > 65535)) return false;
    if (out.type == "UDP" && !out.udpCommandHost.empty() &&
        (out.udpCommandPort <= 0 || out.udpCommandPort > 65535)) return false;
    if (out.type == "OPCUA" && out.endpoint.empty()) return false;
    if (out.type == "SDC" && out.sdcUrl.empty()) return false;
    if (out.type == "MODBUS") {
        if (out.modbusHost.empty() || out.port <= 0 || out.port > 65535 || out.modbusPoints.empty()) return false;
        for (auto& point : out.modbusPoints) {
            std::transform(point.functionCode.begin(), point.functionCode.end(), point.functionCode.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            std::transform(point.dataType.begin(), point.dataType.end(), point.dataType.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            if (point.address < 0 || point.address > 65535 || !std::isfinite(point.scale) || std::abs(point.scale) < 1e-12) return false;
            if (point.functionCode != "coil" && point.functionCode != "discrete_input" && point.functionCode != "holding_register" && point.functionCode != "input_register") return false;
            if (point.dataType != "bool" && point.dataType != "uint16" && point.dataType != "int16" && point.dataType != "uint32" && point.dataType != "int32" && point.dataType != "float32" && point.dataType != "float32_swap") return false;
            if ((point.functionCode == "coil" || point.functionCode == "discrete_input") && point.dataType != "bool") return false;
        }
    }

    if (out.name.empty()) out.name = out.type == "UDP" ? "UDP 监听" : (out.type == "SDC" ? "软件定义通信" : "OPC UA 采集");
    return !out.type.empty();
}

// ==================== 文件持久化 ====================

static std::filesystem::path communicationResourcesFilePath() {
    namespace fs = std::filesystem;
    const char* configuredDir = std::getenv("MONITOR_DATA_DIR");
    fs::path dir = configuredDir && *configuredDir ? fs::path(configuredDir) : (fs::current_path() / "data");
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir / "communication_resources.json";
}

static double jsonExtractDouble(const std::string& body, const std::string& key, double fallback) {
    std::string searchKey = "\"" + key + "\"";
    auto pos = body.find(searchKey);
    if (pos == std::string::npos) return fallback;
    auto colon = body.find(':', pos);
    if (colon == std::string::npos) return fallback;
    auto start = body.find_first_of("-0123456789", colon);
    if (start == std::string::npos) return fallback;
    try { return std::stod(body.substr(start)); } catch (...) { return fallback; }
}

static size_t findJsonObjectEnd(const std::string& text, size_t objectStart) {
    if (objectStart == std::string::npos || objectStart >= text.size() || text[objectStart] != '{') {
        return std::string::npos;
    }
    bool inString = false;
    bool escaped = false;
    int depth = 0;
    for (size_t i = objectStart; i < text.size(); ++i) {
        const char ch = text[i];
        if (inString) {
            if (escaped) escaped = false;
            else if (ch == '\\') escaped = true;
            else if (ch == '"') inString = false;
            continue;
        }
        if (ch == '"') inString = true;
        else if (ch == '{') ++depth;
        else if (ch == '}' && --depth == 0) return i;
    }
    return std::string::npos;
}

void CommunicationManager::load() {
    std::lock_guard lock(mutex_);
    resources_.clear();

    std::ifstream in(communicationResourcesFilePath());
    if (!in) return;
    std::ostringstream ss;
    ss << in.rdbuf();
    std::string body = ss.str();

    // 解析顶层数组：按 "{" ... "}" 逐段解析（兼容无第三方 JSON 库）
    size_t pos = 0;
    while (pos < body.size()) {
        auto objStart = body.find('{', pos);
        if (objStart == std::string::npos) break;
        auto objEnd = findJsonObjectEnd(body, objStart);
        if (objEnd == std::string::npos) break;
        std::string obj = body.substr(objStart, objEnd - objStart + 1);

        CommunicationResource res;
        if (parseResourceJson(obj, res) && !res.id.empty()) {
            resources_[res.id] = res;
        }
        pos = objEnd + 1;
    }
    Log(LogLevel::INFO, "Communication resources loaded: " + std::to_string(resources_.size()));
}

void CommunicationManager::save() {
    std::map<std::string, CommunicationResource> resources;
    std::map<std::string, std::shared_ptr<TaskContext>> tasks;
    {
        std::lock_guard lock(mutex_);
        resources = resources_;
        tasks = tasks_;
    }

    const auto target = communicationResourcesFilePath();
    const std::string temporary = target.string() + ".tmp";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out) {
            Log(LogLevel::ERR, "Unable to open communication resource file for writing: " + target.string());
            return;
        }
        out << resourcesToJsonSnapshot(resources, tasks);
        out.flush();
        if (!out.good()) {
            Log(LogLevel::ERR, "Unable to write communication resource file: " + target.string());
            return;
        }
    }

#ifdef _WIN32
    if (!MoveFileExA(temporary.c_str(), target.string().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        Log(LogLevel::ERR, "Unable to replace communication resource file: " + target.string());
    }
#else
    std::error_code ec;
    std::filesystem::rename(temporary, target, ec);
    if (ec) Log(LogLevel::ERR, "Unable to replace communication resource file: " + target.string());
#endif
}

std::string CommunicationManager::resourcesToJson() const {
    std::lock_guard lock(mutex_);
    return resourcesToJsonSnapshot(resources_, tasks_);
}

std::string CommunicationManager::resourcesToJsonSnapshot(
    const std::map<std::string, CommunicationResource>& resources,
    const std::map<std::string, std::shared_ptr<TaskContext>>& tasks) {
    std::stringstream ss;
    ss << "[";
    size_t i = 0;
    for (const auto& kv : resources) {
        const auto& res = kv.second;
        bool running = false;
        auto it = tasks.find(res.id);
        if (it != tasks.end()) running = it->second->running.load();

        std::string base = res.toJson();
        // 在对象末尾注入 running 字段
        if (base.size() >= 1 && base.back() == '}') {
            base.pop_back();
            base += ",\"running\":" + std::string(running ? "true" : "false") + "}";
        }
        ss << base;
        if (i < resources.size() - 1) ss << ",";
        ++i;
    }
    ss << "]";
    return ss.str();
}

// ==================== CRUD ====================

std::vector<CommunicationResource> CommunicationManager::listResources() const {
    std::lock_guard lock(mutex_);
    std::vector<CommunicationResource> result;
    result.reserve(resources_.size());
    for (const auto& kv : resources_) result.push_back(kv.second);
    return result;
}

CommunicationResource CommunicationManager::getResource(const std::string& id) const {
    std::lock_guard lock(mutex_);
    auto it = resources_.find(id);
    if (it == resources_.end()) return CommunicationResource{};
    return it->second;
}

bool CommunicationManager::upsertResource(const CommunicationResource& res) {
    if (res.id.empty()) return false;
    bool existed;
    bool wasRunning = false;
    {
        std::lock_guard lock(mutex_);
        existed = resources_.count(res.id) > 0;
        auto task = tasks_.find(res.id);
        wasRunning = task != tasks_.end() && task->second->running.load();
        resources_[res.id] = res;
    }

    // Keep already collected variables aligned when a resource is renamed or
    // moved to another project/application.  The real-time values and history
    // remain untouched because the variable id is preserved.
    for (auto variable : VariableManager::instance().getAllDefinitions()) {
        if (variable.resourceId != res.id) continue;
        variable.resourceName = res.name;
        variable.scopeId = res.scopeId;
        variable.scopeName = res.scopeName;
        VariableManager::instance().addOrUpdateDefinition(variable);
    }

    save();
    if (wasRunning) {
        // Endpoint, port and interval changes must take effect immediately;
        // otherwise a running task would keep using its old copied config.
        stopTask(res.id, false);
        startTask(res.id);
    }
    return existed;
}

bool CommunicationManager::removeResource(const std::string& id) {
    stopTask(id, false);
    {
        std::lock_guard lock(mutex_);
        if (resources_.erase(id) == 0) return false;
    }
    save();
    return true;
}

// ==================== 运行控制 ====================

bool CommunicationManager::isTaskRunning(const std::string& id) const {
    std::lock_guard lock(mutex_);
    auto it = tasks_.find(id);
    if (it == tasks_.end()) return false;
    return it->second->running.load();
}

void CommunicationManager::startAllEnabled() {
    std::vector<CommunicationResource> enabled;
    {
        std::lock_guard lock(mutex_);
        for (const auto& kv : resources_) {
            if (kv.second.enabled) enabled.push_back(kv.second);
        }
    }
    for (const auto& res : enabled) startTask(res.id);
}

void CommunicationManager::stopAll() {
    std::vector<std::string> ids;
    {
        std::lock_guard lock(mutex_);
        for (const auto& kv : tasks_) ids.push_back(kv.first);
    }
    for (const auto& id : ids) stopTask(id, false);
}

bool CommunicationManager::startTask(const std::string& id) {
    CommunicationResource res;
    {
        std::lock_guard lock(mutex_);
        auto it = resources_.find(id);
        if (it == resources_.end()) return false;
        res = it->second;
        auto taskIt = tasks_.find(id);
        if (taskIt != tasks_.end() && taskIt->second->running.load()) return true; // 已在运行
    }
    startTaskUnlocked(res);
    {
        std::lock_guard lock(mutex_);
        auto it = resources_.find(id);
        if (it != resources_.end()) it->second.enabled = true;
    }
    save();
    return true;
}

void CommunicationManager::startTaskUnlocked(const CommunicationResource& res) {
    stopTask(res.id, false);

    auto ctx = std::make_shared<TaskContext>();
    ctx->id = res.id;
    ctx->type = res.type;
    ctx->running = true;

    if (res.type == "OPCUA") {
        ctx->worker = std::thread(&CommunicationManager::opcuaWorker, ctx, res);
    } else if (res.type == "UDP") {
        ctx->worker = std::thread(&CommunicationManager::udpWorker, ctx, res);
    } else if (res.type == "SDC") {
        ctx->worker = std::thread(&CommunicationManager::sdcWorker, ctx, res);
    } else if (res.type == "MODBUS") {
        ctx->worker = std::thread(&CommunicationManager::modbusWorker, ctx, res);
    } else {
        ctx->running = false;
        Log(LogLevel::WARN, "Unsupported communication type: " + res.type);
        return;
    }

    std::lock_guard lock(mutex_);
    tasks_[res.id] = ctx;
}

bool CommunicationManager::stopTask(const std::string& id, bool disableResource) {
    std::shared_ptr<TaskContext> ctx;
    bool resourceExists = false;
    {
        std::lock_guard lock(mutex_);
        auto it = tasks_.find(id);
        if (it != tasks_.end()) {
            ctx = it->second;
        } else if (disableResource) {
            auto resourceIt = resources_.find(id);
            if (resourceIt != resources_.end()) {
                resourceIt->second.enabled = false;
                resourceExists = true;
            }
        }
    }

    if (!ctx) {
        if (resourceExists) save();
        return resourceExists;
    }

    ctx->running = false;

    // UDP 通过关闭 socket 打断 recvfrom；OPC UA 通过断开客户端打断
    if ((ctx->type == "UDP" || ctx->type == "MODBUS") && ctx->clientHandle) {
        closesocket(reinterpret_cast<SOCKET>(ctx->clientHandle));
        ctx->clientHandle = nullptr;
    }

    if (ctx->worker.joinable()) ctx->worker.join();

    {
        std::lock_guard lock(mutex_);
        tasks_.erase(id);
        if (disableResource) {
            auto resourceIt = resources_.find(id);
            if (resourceIt != resources_.end()) resourceIt->second.enabled = false;
        }
    }
    if (disableResource) save();
    Log(LogLevel::INFO, "Communication task stopped: " + id);
    return true;
}

// ==================== 变量写入辅助 ====================

static std::string makeSdcBinding(const std::string& resourceId, const std::string& variableName) {
    return "sdc:" + std::to_string(resourceId.size()) + ":" + resourceId + variableName;
}

static bool isSameCollectedBinding(const VariableInfo& variable,
                                   const CommunicationResource& resource,
                                   const std::string& source,
                                   const std::string& sourceNodeId) {
    return !resource.id.empty() && variable.resourceId == resource.id &&
           variable.source == source && variable.opcuaNodeId == sourceNodeId;
}

static std::string uniqueCollectedVariableName(const std::string& requested,
                                               const CommunicationResource& resource,
                                               const std::vector<VariableInfo>& allVars) {
    const auto nameExists = [&allVars](const std::string& candidate) {
        return std::any_of(allVars.begin(), allVars.end(), [&candidate](const VariableInfo& variable) {
            return variable.name == candidate;
        });
    };
    if (!nameExists(requested)) return requested;

    const std::string prefix = (resource.name.empty() ? resource.id : resource.name) + ".";
    const size_t maxNameLength = 120;
    for (int suffix = 1; ; ++suffix) {
        const std::string suffixText = suffix == 1 ? "" : "." + std::to_string(suffix);
        const size_t available = maxNameLength > prefix.size() + suffixText.size()
            ? maxNameLength - prefix.size() - suffixText.size() : 0;
        const std::string candidate = prefix + requested.substr(0, available) + suffixText;
        if (!nameExists(candidate)) return candidate;
    }
}

static void upsertCollectedValue(const CommunicationResource& resource,
                                 const std::string& variableName,
                                 const std::string& sourceNodeId,
                                 const std::string& source,
                                 const std::string& description,
                                 double value,
                                 const std::string& quality) {
    if (variableName.empty()) return;

    auto allVars = VariableManager::instance().getAllDefinitions();
    for (const auto& v : allVars) {
        if (isSameCollectedBinding(v, resource, source, sourceNodeId)) {
            VariableManager::instance().updateValue(v.id, value, quality);
            VariableManager::instance().appendHistory(v.id, value, nowMs());
            return;
        }
    }

    VariableInfo newVar;
    newVar.name = uniqueCollectedVariableName(variableName, resource, allVars);
    newVar.description = description;
    newVar.source = source;
    newVar.opcuaNodeId = sourceNodeId;
    newVar.resourceId = resource.id;
    newVar.resourceName = resource.name;
    newVar.scopeId = resource.scopeId;
    newVar.scopeName = resource.scopeName;
    VariableManager::instance().addOrUpdateDefinition(newVar);

    auto refreshed = VariableManager::instance().getAllDefinitions();
    for (const auto& v : refreshed) {
        if (isSameCollectedBinding(v, resource, source, sourceNodeId)) {
            VariableManager::instance().updateValue(v.id, value, quality);
            VariableManager::instance().appendHistory(v.id, value, nowMs());
            return;
        }
    }
}

// UDP and SDC parsers run inside one worker per resource.  Keeping the
// active resource thread-local preserves their existing lightweight parsers
// while ensuring equal point names on different ports/resources never share
// a VariableInfo record.
static thread_local const CommunicationResource* g_activeCollectionResource = nullptr;

static void upsertCollectedValue(const std::string& variableName,
                                 const std::string& sourceNodeId,
                                 const std::string& source,
                                 const std::string& description,
                                 double value,
                                 const std::string& quality) {
    if (!g_activeCollectionResource) return;
    std::string binding = sourceNodeId;
    if (source == "UDP" && binding.empty()) {
        binding = "udp:" + std::to_string(g_activeCollectionResource->port) + ":" + variableName;
    }
    upsertCollectedValue(*g_activeCollectionResource, variableName, binding, source,
                         description, value, quality);
}

// ==================== OPC UA 工作线程 ====================

static UA_NodeId parseNodeId(const std::string& nodeIdStr) {
    UA_UInt16 ns = 1;
    std::string remain = trimCopy(nodeIdStr);
    if (remain.size() >= 4 && remain[0] == 'n' && remain[1] == 's' && remain[2] == '=') {
        auto semi = remain.find(';', 3);
        if (semi != std::string::npos) {
            try {
                const long long parsedNamespace = std::stoll(remain.substr(3, semi - 3));
                if (parsedNamespace < 0 || parsedNamespace > std::numeric_limits<UA_UInt16>::max()) {
                    return UA_NODEID_NULL;
                }
                ns = static_cast<UA_UInt16>(parsedNamespace);
                remain = remain.substr(semi + 1);
            } catch (...) {
                return UA_NODEID_NULL;
            }
        }
    }
    if (remain.size() >= 2 && remain[0] == 'i' && remain[1] == '=') {
        try {
            const unsigned long long id = std::stoull(remain.substr(2));
            if (id > std::numeric_limits<UA_UInt32>::max()) return UA_NODEID_NULL;
            return UA_NODEID_NUMERIC(ns, static_cast<UA_UInt32>(id));
        } catch (...) {
            return UA_NODEID_NULL;
        }
    }
    if (remain.size() >= 2 && remain[0] == 's' && remain[1] == '=') {
        if (remain.size() == 2) return UA_NODEID_NULL;
        return UA_NODEID_STRING_ALLOC(ns, remain.substr(2).c_str());
    }
    if (remain.empty()) return UA_NODEID_NULL;
    return UA_NODEID_STRING_ALLOC(ns, remain.c_str());
}

static bool variantToDouble(const UA_Variant& variant, double& value) {
    if (variant.type == &UA_TYPES[UA_TYPES_DOUBLE]) value = *(UA_Double*)variant.data;
    else if (variant.type == &UA_TYPES[UA_TYPES_FLOAT]) value = *(UA_Float*)variant.data;
    else if (variant.type == &UA_TYPES[UA_TYPES_INT32]) value = *(UA_Int32*)variant.data;
    else if (variant.type == &UA_TYPES[UA_TYPES_INT64]) value = static_cast<double>(*(UA_Int64*)variant.data);
    else if (variant.type == &UA_TYPES[UA_TYPES_UINT32]) value = *(UA_UInt32*)variant.data;
    else if (variant.type == &UA_TYPES[UA_TYPES_UINT64]) value = static_cast<double>(*(UA_UInt64*)variant.data);
    else if (variant.type == &UA_TYPES[UA_TYPES_INT16]) value = *(UA_Int16*)variant.data;
    else if (variant.type == &UA_TYPES[UA_TYPES_UINT16]) value = *(UA_UInt16*)variant.data;
    else if (variant.type == &UA_TYPES[UA_TYPES_SBYTE]) value = *(UA_SByte*)variant.data;
    else if (variant.type == &UA_TYPES[UA_TYPES_BYTE]) value = *(UA_Byte*)variant.data;
    else if (variant.type == &UA_TYPES[UA_TYPES_BOOLEAN]) value = (*(UA_Boolean*)variant.data) ? 1.0 : 0.0;
    else return false;
    return true;
}

static std::vector<std::pair<std::string, double>> parseNumericDictionary(const std::string& text) {
    std::vector<std::pair<std::string, double>> values;
    const std::regex pairRegex(R"(["']([^"']+)["']\s*:\s*(-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?))");
    auto begin = std::sregex_iterator(text.begin(), text.end(), pairRegex);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        std::string key = (*it)[1].str();
        if (key == "timestamp" || key == "time") continue;
        try { values.emplace_back(key, std::stod((*it)[2].str())); } catch (...) {}
    }
    return values;
}

void CommunicationManager::opcuaWorker(std::shared_ptr<TaskContext> ctx, CommunicationResource res) {
    UA_Client* client = UA_Client_new();
    if (!client) {
        ctx->running = false;
        Log(LogLevel::ERR, "OPC UA client allocation failed for task " + res.id);
        return;
    }
    UA_ClientConfig_setDefault(UA_Client_getConfig(client));
    UA_ClientConfig* config = UA_Client_getConfig(client);
    config->timeout = 3000;
    config->requestedSessionTimeout = 10000;

    UA_StatusCode ret = UA_Client_connect(client, res.endpoint.c_str());
    if (ret != UA_STATUSCODE_GOOD) {
        Log(LogLevel::ERR, "OPC UA task " + res.name + " connect failed: " +
            std::string(UA_StatusCode_name(ret)));
        UA_Client_delete(client);
        ctx->running = false;
        return;
    }
    ctx->clientHandle = client;
    Log(LogLevel::INFO, "OPC UA task started: " + res.name + " -> " + res.endpoint +
        " (" + std::to_string(res.nodes.size()) + " nodes)");

    int sleepMs = res.pollIntervalMs > 0 ? res.pollIntervalMs : 1000;
    while (ctx->running) {
        for (const auto& node : res.nodes) {
            if (!ctx->running) break;

            UA_NodeId uaNodeId = parseNodeId(node.nodeId);
            UA_Variant variant;
            UA_Variant_init(&variant);
            UA_StatusCode readRet = UA_Client_readValueAttribute(client, uaNodeId, &variant);
            UA_NodeId_clear(&uaNodeId);

            if (readRet != UA_STATUSCODE_GOOD || !UA_Variant_isScalar(&variant)) {
                UA_Variant_clear(&variant);
                continue;
            }

            if (variant.type == &UA_TYPES[UA_TYPES_STRING]) {
                std::string text = uaStringToStd(*(UA_String*)variant.data);
                auto fields = parseNumericDictionary(text);
                if (fields.empty()) {
                    UA_Variant_clear(&variant);
                    continue;
                }
                for (const auto& field : fields) {
                    upsertCollectedValue(res, field.first, node.nodeId + "#" + field.first, "OPCUA",
                                         "从 " + res.name + " 自动采集", field.second, "GOOD");
                }
            } else {
                double value = 0.0;
                if (variantToDouble(variant, value)) {
                    upsertCollectedValue(res, node.name, node.nodeId, "OPCUA",
                                         "从 " + res.name + " 自动采集", value, "GOOD");
                }
            }
            UA_Variant_clear(&variant);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    }

    UA_Client_disconnect(client);
    UA_Client_delete(client);
    ctx->clientHandle = nullptr;
    ctx->running = false;
    Log(LogLevel::INFO, "OPC UA task stopped: " + res.name);
}

// ==================== UDP 工作线程 ====================

static bool parseJsonUdpData(const std::string& data, const std::string& taskName) {
    auto braceOpen = data.find('{');
    auto braceClose = data.find('}');
    if (braceOpen == std::string::npos || braceClose == std::string::npos) return false;

    std::string inner = data.substr(braceOpen + 1, braceClose - braceOpen - 1);
    size_t pos = 0;
    bool handled = false;

    while (pos < inner.size()) {
        auto ks = inner.find('"', pos);
        if (ks == std::string::npos) break;
        auto ke = inner.find('"', ks + 1);
        if (ke == std::string::npos) break;
        std::string key = inner.substr(ks + 1, ke - ks - 1);

        auto col = inner.find(':', ke + 1);
        if (col == std::string::npos) break;
        auto vs = inner.find_first_not_of(" \t\r\n", col + 1);
        if (vs == std::string::npos) break;

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
        if (inner[vs] == '"') {
            auto strEnd = inner.find('"', vs + 1);
            pos = (strEnd != std::string::npos) ? strEnd + 1 : inner.size();
            continue;
        }
        if (inner.substr(vs, 4) == "true" || inner.substr(vs, 5) == "false" || inner.substr(vs, 4) == "null") {
            auto ve = inner.find_first_of(",}", vs);
            pos = (ve != std::string::npos) ? ve + 1 : inner.size();
            continue;
        }

        auto ve = inner.find_first_of(",}", vs);
        if (ve == std::string::npos) ve = inner.size();
        std::string valStr = trimCopy(inner.substr(vs, ve - vs));
        if (!key.empty() && !valStr.empty()) {
            try {
                double value = std::stod(valStr);
                upsertCollectedValue(key, "", "UDP", "从 " + taskName + " 自动采集", value, "GOOD");
                handled = true;
            } catch (...) {}
        }
        pos = ve + 1;
    }
    return handled;
}

static void parseUdpKeyValue(const std::string& data, const std::string& taskName) {
    auto eqPos = data.find('=');
    if (eqPos == std::string::npos) return;
    std::string name = trimCopy(data.substr(0, eqPos));
    if (name.empty()) return;
    try {
        double value = std::stod(trimCopy(data.substr(eqPos + 1)));
        upsertCollectedValue(name, "", "UDP", "从 " + taskName + " 自动采集", value, "GOOD");
    } catch (...) {}
}

static bool g_commWinsockInitialized = false;

static bool ensureWinsock() {
    if (g_commWinsockInitialized) return true;
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
    g_commWinsockInitialized = true;
    return true;
}

void CommunicationManager::udpWorker(std::shared_ptr<TaskContext> ctx, CommunicationResource res) {
    g_activeCollectionResource = &res;
    if (!ensureWinsock()) {
        ctx->running = false;
        Log(LogLevel::ERR, "UDP task " + res.name + " WSAStartup failed");
        return;
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        ctx->running = false;
        Log(LogLevel::ERR, "UDP task " + res.name + " socket creation failed");
        return;
    }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    DWORD timeout = 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<u_short>(res.port));
    std::string bindAddr = res.address.empty() ? "0.0.0.0" : res.address;
    if (bindAddr == "0.0.0.0") addr.sin_addr.s_addr = INADDR_ANY;
    else addr.sin_addr.s_addr = inet_addr(bindAddr.c_str());

    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        Log(LogLevel::ERR, "UDP task " + res.name + " bind failed on " + bindAddr +
            ":" + std::to_string(res.port) + " error=" + std::to_string(WSAGetLastError()));
        closesocket(sock);
        ctx->running = false;
        return;
    }

    ctx->clientHandle = reinterpret_cast<void*>(sock);
    Log(LogLevel::INFO, "UDP task started: " + res.name + " on " + bindAddr +
        ":" + std::to_string(res.port));

    char buffer[65536];
    sockaddr_in senderAddr;
    int addrLen = sizeof(senderAddr);

    while (ctx->running) {
        int bytes = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, (sockaddr*)&senderAddr, &addrLen);
        if (bytes <= 0) continue;
        buffer[bytes] = '\0';
        std::string data(buffer);

        if (!data.empty() && (data[0] == '{' || data[0] == '[')) {
            parseJsonUdpData(data, res.name);
        } else if (data.find('=') != std::string::npos) {
            parseUdpKeyValue(data, res.name);
        }
    }

    closesocket(sock);
    ctx->clientHandle = nullptr;
    ctx->running = false;
    g_activeCollectionResource = nullptr;
    Log(LogLevel::INFO, "UDP task stopped: " + res.name);
}

// ==================== Modbus TCP ====================

static bool modbusSendAll(SOCKET socket, const unsigned char* data, int length) {
    int sent = 0;
    while (sent < length) {
        const int result = send(socket, reinterpret_cast<const char*>(data + sent), length - sent, 0);
        if (result <= 0) return false;
        sent += result;
    }
    return true;
}

static bool modbusReceiveAll(SOCKET socket, unsigned char* data, int length) {
    int received = 0;
    while (received < length) {
        const int result = recv(socket, reinterpret_cast<char*>(data + received), length - received, 0);
        if (result <= 0) return false;
        received += result;
    }
    return true;
}

static SOCKET openModbusSocket(const std::string& host, int port, int timeoutMs, std::string& error) {
    if (!ensureWinsock()) { error = "Winsock initialization failed"; return INVALID_SOCKET; }
    if (host.empty() || port < 1 || port > 65535) { error = "Invalid Modbus host or port"; return INVALID_SOCKET; }
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    addrinfo* addresses = nullptr;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &addresses) != 0) {
        error = "Unable to resolve Modbus host: " + host;
        return INVALID_SOCKET;
    }
    const int effectiveTimeout = std::clamp(timeoutMs, 300, 15000);
    SOCKET connected = INVALID_SOCKET;
    for (auto* address = addresses; address; address = address->ai_next) {
        SOCKET socket = ::socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (socket == INVALID_SOCKET) continue;
        u_long nonBlocking = 1;
        ioctlsocket(socket, FIONBIO, &nonBlocking);
        const int result = connect(socket, address->ai_addr, static_cast<int>(address->ai_addrlen));
        bool ok = result == 0;
        if (!ok && WSAGetLastError() == WSAEWOULDBLOCK) {
            fd_set writeSet;
            FD_ZERO(&writeSet);
            FD_SET(socket, &writeSet);
            timeval timeout{};
            timeout.tv_sec = effectiveTimeout / 1000;
            timeout.tv_usec = (effectiveTimeout % 1000) * 1000;
            if (select(0, nullptr, &writeSet, nullptr, &timeout) > 0) {
                int socketError = 0;
                int socketErrorLength = sizeof(socketError);
                getsockopt(socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&socketError), &socketErrorLength);
                ok = socketError == 0;
            }
        }
        if (!ok) { closesocket(socket); continue; }
        nonBlocking = 0;
        ioctlsocket(socket, FIONBIO, &nonBlocking);
        DWORD timeout = static_cast<DWORD>(effectiveTimeout);
        setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        connected = socket;
        break;
    }
    freeaddrinfo(addresses);
    if (connected == INVALID_SOCKET) error = "Unable to connect to Modbus TCP device " + host + ":" + std::to_string(port);
    return connected;
}

bool CommunicationManager::testModbusEndpoint(const std::string& host, int port, int timeoutMs, std::string& error) const {
    SOCKET socket = openModbusSocket(host, port, timeoutMs, error);
    if (socket == INVALID_SOCKET) return false;
    closesocket(socket);
    return true;
}

static int modbusFunction(const std::string& code) {
    if (code == "coil") return 1;
    if (code == "discrete_input") return 2;
    if (code == "holding_register") return 3;
    if (code == "input_register") return 4;
    return 0;
}

static bool readModbusPoint(void*& clientHandle, const CommunicationResource& resource, const ModbusPoint& point,
                            uint16_t transactionId, double& value, std::string& error) {
    const int function = modbusFunction(point.functionCode);
    const int quantity = (function == 1 || function == 2) ? 1 :
        ((point.dataType == "uint32" || point.dataType == "int32" || point.dataType == "float32" || point.dataType == "float32_swap") ? 2 : 1);
    if (function == 0 || point.address < 0 || point.address + quantity > 65536) { error = "Invalid Modbus point configuration"; return false; }
    SOCKET socket = openModbusSocket(resource.modbusHost, resource.port, resource.modbusTimeoutMs, error);
    if (socket == INVALID_SOCKET) return false;
    clientHandle = reinterpret_cast<void*>(socket);
    unsigned char request[12] = {
        static_cast<unsigned char>((transactionId >> 8) & 0xff), static_cast<unsigned char>(transactionId & 0xff),
        0, 0, 0, 6, static_cast<unsigned char>(resource.modbusUnitId), static_cast<unsigned char>(function),
        static_cast<unsigned char>((point.address >> 8) & 0xff), static_cast<unsigned char>(point.address & 0xff),
        static_cast<unsigned char>((quantity >> 8) & 0xff), static_cast<unsigned char>(quantity & 0xff)
    };
    unsigned char header[7]{};
    bool ok = modbusSendAll(socket, request, sizeof(request)) && modbusReceiveAll(socket, header, sizeof(header));
    const int responseLength = ok ? ((static_cast<int>(header[4]) << 8) | header[5]) : 0;
    unsigned char body[260]{};
    if (ok && (responseLength < 2 || responseLength > 260 || header[0] != request[0] || header[1] != request[1] || header[2] != 0 || header[3] != 0 || header[6] != request[6])) ok = false;
    if (ok) ok = modbusReceiveAll(socket, body, responseLength - 1);
    closesocket(socket);
    clientHandle = nullptr;
    if (!ok) { error = "Invalid or incomplete Modbus TCP response"; return false; }
    if (body[0] == static_cast<unsigned char>(function | 0x80)) { error = "Modbus exception " + std::to_string(body[1]); return false; }
    if (body[0] != function || responseLength < 3) { error = "Unexpected Modbus response function"; return false; }
    const int byteCount = body[1];
    if (byteCount != responseLength - 3) { error = "Invalid Modbus response length"; return false; }
    if (function == 1 || function == 2) {
        if (byteCount < 1) { error = "Missing Modbus bit value"; return false; }
        value = (body[2] & 0x01) ? 1.0 : 0.0;
    } else {
        const int required = quantity * 2;
        if (byteCount != required) { error = "Unexpected Modbus register byte count"; return false; }
        const uint16_t first = static_cast<uint16_t>((static_cast<uint16_t>(body[2]) << 8) | body[3]);
        if (point.dataType == "uint16") value = first;
        else if (point.dataType == "bool") value = first != 0 ? 1.0 : 0.0;
        else if (point.dataType == "int16") value = static_cast<int16_t>(first);
        else {
            const uint32_t bits = (static_cast<uint32_t>(body[2]) << 24) | (static_cast<uint32_t>(body[3]) << 16) |
                                  (static_cast<uint32_t>(body[4]) << 8) | static_cast<uint32_t>(body[5]);
            if (point.dataType == "uint32") value = static_cast<double>(bits);
            else if (point.dataType == "int32") { int32_t signedValue; std::memcpy(&signedValue, &bits, sizeof(signedValue)); value = static_cast<double>(signedValue); }
            else { uint32_t floatBits = bits; if (point.dataType == "float32_swap") floatBits = (bits << 16) | (bits >> 16); float floatValue; std::memcpy(&floatValue, &floatBits, sizeof(floatValue)); value = floatValue; }
        }
    }
    value *= point.scale;
    if (!std::isfinite(value)) { error = "Modbus value is not finite"; return false; }
    return true;
}

static void markModbusResourceBad(const CommunicationResource& resource) {
    for (const auto& variable : VariableManager::instance().getAllDefinitions()) {
        if (variable.source == "MODBUS" && variable.resourceId == resource.id) VariableManager::instance().updateValue(variable.id, 0.0, "BAD");
    }
}

void CommunicationManager::modbusWorker(std::shared_ptr<TaskContext> ctx, CommunicationResource res) {
    Log(LogLevel::INFO, "Modbus TCP task started: " + res.name + " -> " + res.modbusHost + ":" + std::to_string(res.port));
    uint16_t transactionId = 1;
    int failures = 0;
    while (ctx->running) {
        bool cycleFailed = false;
        for (const auto& point : res.modbusPoints) {
            if (!ctx->running) break;
            double value = 0.0;
            std::string error;
            if (readModbusPoint(ctx->clientHandle, res, point, transactionId++, value, error)) {
                const std::string binding = "modbus:" + point.functionCode + ":" + std::to_string(point.address) + ":" + point.dataType;
                upsertCollectedValue(res, point.name, binding, "MODBUS", "Auto-collected from " + res.name, value, "GOOD");
            } else {
                cycleFailed = true;
                Log(LogLevel::WARN, "Modbus task " + res.name + " point " + point.name + " failed: " + error);
            }
        }
        failures = cycleFailed ? failures + 1 : 0;
        if (failures >= 3) markModbusResourceBad(res);
        const int interval = std::clamp(res.pollIntervalMs, 50, 600000);
        for (int waited = 0; ctx->running && waited < interval; waited += 50) {
            std::this_thread::sleep_for(std::chrono::milliseconds(std::min(50, interval - waited)));
        }
    }
    ctx->clientHandle = nullptr;
    ctx->running = false;
    Log(LogLevel::INFO, "Modbus TCP task stopped: " + res.name);
}

static bool parseModbusBinding(const std::string& binding, std::string& functionCode,
                               int& address, std::string& dataType) {
    if (binding.compare(0, 7, "modbus:") != 0) return false;
    const auto first = binding.find(':', 7);
    const auto second = first == std::string::npos ? std::string::npos : binding.find(':', first + 1);
    if (first == std::string::npos || second == std::string::npos) return false;
    functionCode = binding.substr(7, first - 7);
    dataType = binding.substr(second + 1);
    try {
        size_t converted = 0;
        address = std::stoi(binding.substr(first + 1, second - first - 1), &converted);
        if (converted != second - first - 1) return false;
    } catch (...) {
        return false;
    }
    return address >= 0 && address <= 65535 && !functionCode.empty() && !dataType.empty();
}

bool CommunicationManager::readModbusValue(const std::string& resourceId, const std::string& binding,
                                            double& value, std::string& error) const {
    const CommunicationResource resource = getResource(resourceId);
    if (resource.id.empty() || resource.type != "MODBUS") {
        error = "Modbus communication resource not found";
        return false;
    }
    if (!resource.enabled) {
        error = "Modbus communication resource is disabled";
        return false;
    }
    std::string functionCode;
    std::string dataType;
    int address = 0;
    if (!parseModbusBinding(binding, functionCode, address, dataType)) {
        error = "Invalid Modbus variable binding";
        return false;
    }
    const auto pointIt = std::find_if(resource.modbusPoints.begin(), resource.modbusPoints.end(),
        [&](const ModbusPoint& point) {
            return point.address == address && point.functionCode == functionCode && point.dataType == dataType;
        });
    if (pointIt == resource.modbusPoints.end()) {
        error = "Modbus point is no longer present in the communication resource";
        return false;
    }
    static std::atomic<uint16_t> nextTransaction{0x4000};
    void* clientHandle = nullptr;
    return readModbusPoint(clientHandle, resource, *pointIt, nextTransaction.fetch_add(1), value, error);
}

static void appendModbusU16(std::vector<unsigned char>& out, uint16_t value) {
    out.push_back(static_cast<unsigned char>((value >> 8) & 0xff));
    out.push_back(static_cast<unsigned char>(value & 0xff));
}

bool CommunicationManager::writeModbusValue(const std::string& resourceId, const std::string& binding,
                                             double value, std::string& error) const {
    const CommunicationResource resource = getResource(resourceId);
    if (resource.id.empty() || resource.type != "MODBUS") {
        error = "Modbus communication resource not found";
        return false;
    }
    if (!resource.enabled) {
        error = "Modbus communication resource is disabled";
        return false;
    }
    std::string functionCode;
    std::string dataType;
    int address = 0;
    if (!parseModbusBinding(binding, functionCode, address, dataType)) {
        error = "Invalid Modbus variable binding";
        return false;
    }
    const auto pointIt = std::find_if(resource.modbusPoints.begin(), resource.modbusPoints.end(),
        [&](const ModbusPoint& point) {
            return point.address == address && point.functionCode == functionCode && point.dataType == dataType;
        });
    if (pointIt == resource.modbusPoints.end()) {
        error = "Modbus point is no longer present in the communication resource";
        return false;
    }
    const ModbusPoint& point = *pointIt;
    if (point.functionCode == "discrete_input" || point.functionCode == "input_register") {
        error = "This Modbus point belongs to a read-only input area";
        return false;
    }
    if (point.functionCode != "coil" && point.functionCode != "holding_register") {
        error = "Unsupported Modbus write area";
        return false;
    }
    if (!std::isfinite(value)) {
        error = "Modbus write value must be finite";
        return false;
    }

    std::vector<unsigned char> values;
    if (!std::isfinite(point.scale) || std::abs(point.scale) < 1e-12) {
        error = "Modbus point scale is invalid";
        return false;
    }
    const double scale = point.scale;
    const double rawValue = value / scale;
    if (!std::isfinite(rawValue)) {
        error = "Modbus write value is out of range after applying the point scale";
        return false;
    }
    bool multipleRegisters = false;
    if (point.functionCode == "coil") {
        appendModbusU16(values, rawValue != 0.0 ? 0xff00 : 0x0000);
    } else if (point.dataType == "bool") {
        appendModbusU16(values, rawValue != 0.0 ? 1 : 0);
    } else if (point.dataType == "uint16") {
        const double rounded = std::round(rawValue);
        if (rounded < 0.0 || rounded > 65535.0) { error = "Modbus uint16 value out of range"; return false; }
        appendModbusU16(values, static_cast<uint16_t>(rounded));
    } else if (point.dataType == "int16") {
        const double rounded = std::round(rawValue);
        if (rounded < -32768.0 || rounded > 32767.0) { error = "Modbus int16 value out of range"; return false; }
        appendModbusU16(values, static_cast<uint16_t>(static_cast<int16_t>(rounded)));
    } else {
        uint32_t bits = 0;
        if (point.dataType == "uint32") {
            const double rounded = std::round(rawValue);
            if (rounded < 0.0 || rounded > 4294967295.0) { error = "Modbus uint32 value out of range"; return false; }
            bits = static_cast<uint32_t>(rounded);
        } else if (point.dataType == "int32") {
            const double rounded = std::round(rawValue);
            if (rounded < -2147483648.0 || rounded > 2147483647.0) { error = "Modbus int32 value out of range"; return false; }
            bits = static_cast<uint32_t>(static_cast<int32_t>(rounded));
        } else if (point.dataType == "float32" || point.dataType == "float32_swap") {
            const float rawFloat = static_cast<float>(rawValue);
            if (!std::isfinite(rawFloat)) { error = "Modbus float32 value out of range"; return false; }
            std::memcpy(&bits, &rawFloat, sizeof(bits));
            if (point.dataType == "float32_swap") bits = (bits << 16) | (bits >> 16);
        } else {
            error = "Unsupported Modbus data type";
            return false;
        }
        appendModbusU16(values, static_cast<uint16_t>((bits >> 16) & 0xffff));
        appendModbusU16(values, static_cast<uint16_t>(bits & 0xffff));
        multipleRegisters = true;
    }

    static std::atomic<uint16_t> nextTransaction{1};
    const uint16_t transaction = nextTransaction.fetch_add(1);
    const unsigned char function = point.functionCode == "coil" ? 5 : (multipleRegisters ? 16 : 6);
    std::vector<unsigned char> request;
    request.reserve(17);
    appendModbusU16(request, transaction);
    appendModbusU16(request, 0);
    const uint16_t pduLength = multipleRegisters ? 10 : 5;
    appendModbusU16(request, static_cast<uint16_t>(pduLength + 1));
    request.push_back(static_cast<unsigned char>(resource.modbusUnitId));
    request.push_back(function);
    appendModbusU16(request, static_cast<uint16_t>(point.address));
    if (multipleRegisters) {
        appendModbusU16(request, 2);
        request.push_back(4);
        request.insert(request.end(), values.begin(), values.end());
    } else {
        request.insert(request.end(), values.begin(), values.end());
    }

    SOCKET socket = openModbusSocket(resource.modbusHost, resource.port, resource.modbusTimeoutMs, error);
    if (socket == INVALID_SOCKET) return false;
    unsigned char header[7]{};
    bool ok = modbusSendAll(socket, request.data(), static_cast<int>(request.size())) &&
              modbusReceiveAll(socket, header, sizeof(header));
    const int responseLength = ok ? ((static_cast<int>(header[4]) << 8) | header[5]) : 0;
    unsigned char response[260]{};
    if (ok && (responseLength < 2 || responseLength > 260 ||
               header[0] != request[0] || header[1] != request[1] || header[2] != 0 || header[3] != 0 ||
               header[6] != request[6])) ok = false;
    if (ok) ok = modbusReceiveAll(socket, response, responseLength - 1);
    closesocket(socket);
    if (!ok) { error = "Invalid or incomplete Modbus write response"; return false; }
    if (response[0] == static_cast<unsigned char>(function | 0x80)) {
        error = "Modbus write exception " + std::to_string(response[1]);
        return false;
    }
    if (response[0] != function || responseLength != 6) { error = "Unexpected Modbus write response"; return false; }
    const uint16_t echoedAddress = static_cast<uint16_t>((static_cast<uint16_t>(response[1]) << 8) | response[2]);
    const uint16_t echoedValueOrCount = static_cast<uint16_t>((static_cast<uint16_t>(response[3]) << 8) | response[4]);
    if (echoedAddress != static_cast<uint16_t>(point.address)) {
        error = "Modbus write response address does not match the requested point";
        return false;
    }
    const uint16_t expectedValueOrCount = multipleRegisters ? 2 :
        static_cast<uint16_t>((static_cast<uint16_t>(values[0]) << 8) | values[1]);
    if (echoedValueOrCount != expectedValueOrCount) {
        error = "Modbus write response does not confirm the requested value";
        return false;
    }
    return true;
}

static std::string lowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

static bool isPositiveUdpAck(const std::string& payload) {
    const std::string ack = lowerCopy(trimCopy(payload));
    if (ack.find("\"ok\":false") != std::string::npos ||
        ack.find("\"success\":false") != std::string::npos ||
        ack.find("\"accepted\":false") != std::string::npos ||
        ack.find("\"status\":\"error\"") != std::string::npos ||
        ack.find("\"status\":\"failed\"") != std::string::npos) return false;
    return ack == "ok" || ack == "1" || ack.find("\"ok\":true") != std::string::npos ||
        ack.find("\"success\":true") != std::string::npos || ack.find("\"status\":\"ok\"") != std::string::npos;
}

bool CommunicationManager::writeUdpValue(const std::string& resourceId, const std::string& variableName,
                                          double value, std::string& error) const {
    const CommunicationResource resource = getResource(resourceId);
    if (resource.id.empty() || resource.type != "UDP") {
        error = "UDP communication resource not found";
        return false;
    }
    if (!resource.enabled) {
        error = "UDP communication resource is disabled";
        return false;
    }
    if (resource.udpCommandHost.empty() || resource.udpCommandPort <= 0 || resource.udpCommandPort > 65535) {
        error = "Configure the UDP command host and command port before writing";
        return false;
    }
    if (!ensureWinsock()) { error = "Unable to initialize UDP socket"; return false; }
    if (!std::isfinite(value)) { error = "UDP write value must be finite"; return false; }
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    addrinfo* addresses = nullptr;
    const std::string portText = std::to_string(resource.udpCommandPort);
    if (getaddrinfo(resource.udpCommandHost.c_str(), portText.c_str(), &hints, &addresses) != 0 || !addresses) {
        error = "Unable to resolve UDP command host";
        return false;
    }
    std::string commandName = variableName;
    if (commandName.compare(0, 4, "udp:") == 0) {
        const auto nameStart = commandName.find(':', 4);
        if (nameStart != std::string::npos && nameStart + 1 < commandName.size()) commandName = commandName.substr(nameStart + 1);
    }
    commandName = trimCopy(commandName);
    if (commandName.empty() || commandName.size() > 256) { error = "UDP variable binding is invalid"; return false; }
    std::ostringstream commandStream;
    commandStream << "{\"name\":\"" << escapeJson(commandName) << "\",\"value\":"
                  << std::setprecision(15) << value << "}";
    const std::string command = commandStream.str();
    if (command.size() > 1024) { error = "UDP command payload is too large"; return false; }
    bool sent = false;
    for (addrinfo* address = addresses; address; address = address->ai_next) {
        SOCKET socket = ::socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (socket == INVALID_SOCKET) continue;
        // A connected UDP socket only receives datagrams from this configured
        // controller.  Without it, any host could forge a positive ACK.
        if (::connect(socket, address->ai_addr, static_cast<int>(address->ai_addrlen)) != 0) {
            closesocket(socket);
            continue;
        }
        const int result = send(socket, command.data(), static_cast<int>(command.size()), 0);
        if (result != static_cast<int>(command.size())) {
            closesocket(socket);
            continue;
        }
        sent = true;
        if (resource.udpRequireAck) {
            const DWORD timeout = static_cast<DWORD>(std::clamp(resource.udpAckTimeoutMs, 100, 15000));
            setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
            char ackBuffer[1024]{};
            const int ackLength = recv(socket, ackBuffer, sizeof(ackBuffer) - 1, 0);
            closesocket(socket);
            if (ackLength <= 0) {
                error = "UDP command was sent but controller ACK timed out";
                sent = false;
            } else {
                const std::string ack(ackBuffer, ackLength);
                if (!isPositiveUdpAck(ack)) {
                    error = "UDP controller returned an unrecognized ACK: " + ack.substr(0, 180);
                    sent = false;
                }
            }
        } else {
            closesocket(socket);
        }
        break;
    }
    freeaddrinfo(addresses);
    if (!sent) {
        if (error.empty()) error = "UDP command datagram could not be sent";
        return false;
    }
    return true;
}

// ==================== Software Defined Communication (SDC) ====================

static std::wstring utf8ToWide(const std::string& text) {
    if (text.empty()) return {};
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                            static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) return {};
    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
                        result.data(), length);
    return result;
}

static bool startsWithIgnoreCase(const std::string& value, const std::string& prefix) {
    if (value.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(value[i])) !=
            std::tolower(static_cast<unsigned char>(prefix[i]))) return false;
    }
    return true;
}

static std::string normalizeSdcEndpoint(std::string endpoint) {
    endpoint = trimCopy(endpoint);
    if (endpoint.empty()) return {};
    if (!startsWithIgnoreCase(endpoint, "http://") && !startsWithIgnoreCase(endpoint, "https://")) {
        endpoint = "http://" + endpoint;
    }
    const auto query = endpoint.find_first_of("?#");
    if (query != std::string::npos) endpoint.resize(query);
    while (endpoint.size() > 8 && endpoint.back() == '/') endpoint.pop_back();
    static const std::string streamSuffix = "/api/runtime/stream";
    if (endpoint.size() >= streamSuffix.size() &&
        endpoint.compare(endpoint.size() - streamSuffix.size(), streamSuffix.size(), streamSuffix) == 0) {
        endpoint.resize(endpoint.size() - streamSuffix.size());
    }
    while (endpoint.size() > 8 && endpoint.back() == '/') endpoint.pop_back();
    return endpoint;
}

static std::string urlEncodePathSegment(const std::string& value) {
    static const char hex[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(value.size() * 3);
    for (unsigned char ch : value) {
        if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            encoded.push_back(static_cast<char>(ch));
        } else {
            encoded.push_back('%');
            encoded.push_back(hex[(ch >> 4) & 0x0F]);
            encoded.push_back(hex[ch & 0x0F]);
        }
    }
    return encoded;
}

static bool performSdcHttpRequest(const std::string& url, const wchar_t* method,
                                  const std::string& requestBody, std::string& response,
                                  unsigned long& statusCode, std::string& error) {
    response.clear();
    statusCode = 0;
    const std::wstring wideUrl = utf8ToWide(url);
    if (wideUrl.empty()) {
        error = "SDC 地址不是有效的 UTF-8 URL";
        return false;
    }

    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwSchemeLength = static_cast<DWORD>(-1);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(wideUrl.c_str(), static_cast<DWORD>(wideUrl.size()), 0, &parts) ||
        (parts.nScheme != INTERNET_SCHEME_HTTP && parts.nScheme != INTERNET_SCHEME_HTTPS) ||
        !parts.lpszHostName || parts.dwHostNameLength == 0) {
        error = "SDC 地址仅支持有效的 http:// 或 https:// URL";
        return false;
    }

    std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    std::wstring objectName;
    if (parts.lpszUrlPath && parts.dwUrlPathLength) objectName.assign(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (parts.lpszExtraInfo && parts.dwExtraInfoLength) objectName.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    if (objectName.empty()) objectName = L"/";

    HINTERNET session = WinHttpOpen(L"Monitoring-T/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        error = "无法创建 SDC HTTP 客户端，错误=" + std::to_string(GetLastError());
        return false;
    }
    WinHttpSetTimeouts(session, 3000, 3000, 5000, 5000);

    HINTERNET connection = WinHttpConnect(session, host.c_str(), parts.nPort, 0);
    if (!connection) {
        error = "无法连接软件定义通信服务，错误=" + std::to_string(GetLastError());
        WinHttpCloseHandle(session);
        return false;
    }

    const DWORD requestFlags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connection, method, objectName.c_str(), nullptr,
                                            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, requestFlags);
    if (!request) {
        error = "无法创建 SDC 请求，错误=" + std::to_string(GetLastError());
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    const wchar_t* headers = requestBody.empty()
        ? L"Accept: application/json\r\nCache-Control: no-cache\r\n"
        : L"Accept: application/json\r\nContent-Type: application/json; charset=utf-8\r\nCache-Control: no-cache\r\n";
    const BOOL sent = WinHttpSendRequest(request, headers, static_cast<DWORD>(-1),
                                         requestBody.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(requestBody.data()),
                                         static_cast<DWORD>(requestBody.size()), static_cast<DWORD>(requestBody.size()), 0);
    if (!sent || !WinHttpReceiveResponse(request, nullptr)) {
        error = "SDC 请求失败，错误=" + std::to_string(GetLastError());
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    DWORD size = sizeof(statusCode);
    if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &size, WINHTTP_NO_HEADER_INDEX)) {
        error = "无法读取 SDC 响应状态，错误=" + std::to_string(GetLastError());
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    constexpr DWORD maxResponseBytes = 1024 * 1024;
    while (true) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available)) {
            error = "读取 SDC 响应失败，错误=" + std::to_string(GetLastError());
            break;
        }
        if (available == 0) break;
        if (response.size() + available > maxResponseBytes) {
            error = "SDC 响应超过 1 MiB 限制";
            break;
        }
        std::string chunk(available, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(request, chunk.data(), available, &read)) {
            error = "读取 SDC 数据失败，错误=" + std::to_string(GetLastError());
            break;
        }
        response.append(chunk.data(), read);
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    if (!error.empty()) return false;
    if (statusCode < 200 || statusCode >= 300) {
        error = "SDC HTTP " + std::to_string(statusCode) + (response.empty() ? "" : ": " + response.substr(0, 180));
        return false;
    }
    return true;
}

static bool parseJsonStringToken(const std::string& input, size_t& position, std::string& value) {
    if (position >= input.size() || input[position] != '"') return false;
    ++position;
    value.clear();
    while (position < input.size()) {
        const char ch = input[position++];
        if (ch == '"') return true;
        if (ch != '\\' || position >= input.size()) {
            value.push_back(ch);
            continue;
        }
        const char escaped = input[position++];
        switch (escaped) {
            case '"': value.push_back('"'); break;
            case '\\': value.push_back('\\'); break;
            case '/': value.push_back('/'); break;
            case 'b': value.push_back('\b'); break;
            case 'f': value.push_back('\f'); break;
            case 'n': value.push_back('\n'); break;
            case 'r': value.push_back('\r'); break;
            case 't': value.push_back('\t'); break;
            default: return false; // do not silently reinterpret \u escapes in variable names
        }
    }
    return false;
}

static void skipJsonWhitespace(const std::string& input, size_t& position) {
    while (position < input.size() && std::isspace(static_cast<unsigned char>(input[position]))) ++position;
}

static bool findMatchingJsonObject(const std::string& input, size_t start, size_t& end) {
    if (start >= input.size() || input[start] != '{') return false;
    bool inString = false;
    bool escaped = false;
    int depth = 0;
    for (size_t i = start; i < input.size(); ++i) {
        const char ch = input[i];
        if (inString) {
            if (escaped) escaped = false;
            else if (ch == '\\') escaped = true;
            else if (ch == '"') inString = false;
            continue;
        }
        if (ch == '"') inString = true;
        else if (ch == '{') ++depth;
        else if (ch == '}' && --depth == 0) {
            end = i;
            return true;
        }
    }
    return false;
}

static bool parseSdcVars(const std::string& payload, std::vector<std::pair<std::string, double>>& values,
                         std::string& error) {
    values.clear();
    const auto varsKey = payload.find("\"vars\"");
    if (varsKey == std::string::npos) {
        error = "SDC 响应缺少 vars 对象";
        return false;
    }
    auto position = payload.find(':', varsKey + 6);
    if (position == std::string::npos) {
        error = "SDC vars 格式无效";
        return false;
    }
    ++position;
    skipJsonWhitespace(payload, position);
    size_t varsEnd = 0;
    if (!findMatchingJsonObject(payload, position, varsEnd)) {
        error = "SDC vars 必须是 JSON 对象";
        return false;
    }
    ++position;
    while (position < varsEnd) {
        skipJsonWhitespace(payload, position);
        if (position < varsEnd && payload[position] == ',') {
            ++position;
            continue;
        }
        std::string key;
        if (!parseJsonStringToken(payload, position, key)) {
            error = "SDC vars 的变量名无效";
            return false;
        }
        skipJsonWhitespace(payload, position);
        if (position >= varsEnd || payload[position++] != ':') {
            error = "SDC vars 缺少冒号";
            return false;
        }
        skipJsonWhitespace(payload, position);
        if (position >= varsEnd) {
            error = "SDC vars 的变量值缺失";
            return false;
        }

        bool supported = false;
        double value = 0.0;
        if (payload.compare(position, 4, "true") == 0) {
            value = 1.0;
            position += 4;
            supported = true;
        } else if (payload.compare(position, 5, "false") == 0) {
            value = 0.0;
            position += 5;
            supported = true;
        } else if (payload[position] == '"') {
            std::string textValue;
            if (!parseJsonStringToken(payload, position, textValue)) {
                error = "SDC 字符串变量格式无效";
                return false;
            }
            try {
                size_t converted = 0;
                value = std::stod(textValue, &converted);
                supported = converted == textValue.size();
            } catch (...) {}
        } else if (payload.compare(position, 4, "null") == 0) {
            position += 4;
        } else {
            const auto valueEnd = payload.find_first_of(",}", position);
            const std::string numeric = trimCopy(payload.substr(position, valueEnd - position));
            try {
                size_t converted = 0;
                value = std::stod(numeric, &converted);
                supported = converted == numeric.size();
            } catch (...) {}
            position = valueEnd == std::string::npos ? varsEnd : valueEnd;
        }
        if (supported && !key.empty() && std::isfinite(value)) values.emplace_back(key, value);
        skipJsonWhitespace(payload, position);
        if (position < varsEnd && payload[position] == ',') ++position;
    }
    return true;
}

static bool fetchSdcValues(const std::string& endpoint, std::vector<std::pair<std::string, double>>& values,
                           std::string& error) {
    const std::string base = normalizeSdcEndpoint(endpoint);
    if (base.empty()) {
        error = "软件定义通信地址不能为空";
        return false;
    }
    std::string response;
    unsigned long status = 0;
    if (!performSdcHttpRequest(base + "/api/runtime/stream", L"GET", "", response, status, error)) return false;
    return parseSdcVars(response, values, error);
}

bool CommunicationManager::testSdcEndpoint(const std::string& endpoint, int& variableCount, std::string& error) const {
    std::vector<std::pair<std::string, double>> values;
    const bool ok = fetchSdcValues(endpoint, values, error);
    variableCount = static_cast<int>(values.size());
    return ok;
}

static bool parseSdcBinding(const std::string& binding, std::string& resourceId, std::string& variableName) {
    if (binding.compare(0, 4, "sdc:") != 0) return false;
    const auto lengthEnd = binding.find(':', 4);
    if (lengthEnd == std::string::npos) return false;
    size_t length = 0;
    try { length = std::stoul(binding.substr(4, lengthEnd - 4)); } catch (...) { return false; }
    const size_t idStart = lengthEnd + 1;
    if (length == 0 || idStart + length >= binding.size()) return false;
    resourceId = binding.substr(idStart, length);
    variableName = binding.substr(idStart + length);
    return !variableName.empty();
}

bool CommunicationManager::readSdcValue(const std::string& binding, double& value, std::string& error) const {
    std::string resourceId;
    std::string variableName;
    if (!parseSdcBinding(binding, resourceId, variableName)) {
        error = "SDC variable binding is invalid";
        return false;
    }
    const CommunicationResource resource = getResource(resourceId);
    if (resource.id.empty() || resource.type != "SDC") {
        error = "SDC communication resource not found";
        return false;
    }
    if (!resource.enabled) {
        error = "SDC communication resource is disabled";
        return false;
    }
    std::vector<std::pair<std::string, double>> values;
    if (!fetchSdcValues(resource.sdcUrl, values, error)) return false;
    const auto valueIt = std::find_if(values.begin(), values.end(), [&](const auto& item) {
        return item.first == variableName;
    });
    if (valueIt == values.end()) {
        error = "SDC readback variable was not returned by the controller";
        return false;
    }
    value = valueIt->second;
    return std::isfinite(value);
}

static bool sdcWriteWasRejected(const std::string& response, std::string& error) {
    const std::string normalized = lowerCopy(trimCopy(response));
    if (normalized.empty()) return false; // HTTP 204 is a valid accepted command.
    const bool explicitFailure = normalized.find("\"ok\":false") != std::string::npos ||
        normalized.find("\"success\":false") != std::string::npos ||
        normalized.find("\"accepted\":false") != std::string::npos ||
        normalized.find("\"status\":\"error\"") != std::string::npos ||
        normalized.find("\"status\":\"failed\"") != std::string::npos ||
        normalized.find("\"status\":\"rejected\"") != std::string::npos ||
        normalized.find("\"result\":\"error\"") != std::string::npos ||
        normalized.find("\"result\":\"failed\"") != std::string::npos ||
        normalized.find("\"error\":\"") != std::string::npos ||
        normalized.find("\"error\":{") != std::string::npos;
    if (!explicitFailure) return false;
    error = "SDC controller rejected the write";
    const auto message = normalized.find("\"message\"");
    if (message != std::string::npos) error += ": " + response.substr(message, 180);
    return true;
}

bool CommunicationManager::writeSdcValue(const std::string& binding, double value, std::string& error) const {
    if (!std::isfinite(value)) {
        error = "SDC write value must be finite";
        return false;
    }
    std::string resourceId;
    std::string variableName;
    if (!parseSdcBinding(binding, resourceId, variableName)) {
        error = "SDC 变量绑定无效";
        return false;
    }
    const CommunicationResource resource = getResource(resourceId);
    if (resource.id.empty() || resource.type != "SDC") {
        error = "SDC 通信资源不存在";
        return false;
    }
    if (!resource.enabled) {
        error = "SDC communication resource is disabled";
        return false;
    }
    std::ostringstream request;
    request << "{\"value\":" << std::setprecision(15) << value << "}";
    std::string response;
    unsigned long status = 0;
    const std::string base = normalizeSdcEndpoint(resource.sdcUrl);
    if (base.empty()) {
        error = "SDC 通信资源地址无效";
        return false;
    }
    if (!performSdcHttpRequest(base + "/api/runtime/stream/" + urlEncodePathSegment(variableName), L"POST",
                               request.str(), response, status, error)) return false;
    return !sdcWriteWasRejected(response, error);
}

void CommunicationManager::sdcWorker(std::shared_ptr<TaskContext> ctx, CommunicationResource res) {
    g_activeCollectionResource = &res;
    Log(LogLevel::INFO, "SDC task started: " + res.name + " -> " + normalizeSdcEndpoint(res.sdcUrl));
    int consecutiveFailures = 0;
    while (ctx->running) {
        std::vector<std::pair<std::string, double>> values;
        std::string error;
        if (fetchSdcValues(res.sdcUrl, values, error)) {
            consecutiveFailures = 0;
            for (const auto& item : values) {
                const std::string binding = makeSdcBinding(res.id, item.first);
                upsertCollectedValue(item.first, binding, "SDC", "从 " + res.name + " 自动采集", item.second, "GOOD");
            }
        } else {
            ++consecutiveFailures;
            if (consecutiveFailures == 1 || consecutiveFailures % 10 == 0) {
                Log(LogLevel::WARN, "SDC task " + res.name + " poll failed: " + error);
            }
        }
        const int interval = std::clamp(res.pollIntervalMs, 50, 600000);
        for (int waited = 0; ctx->running && waited < interval; waited += 50) {
            std::this_thread::sleep_for(std::chrono::milliseconds(std::min(50, interval - waited)));
        }
    }
    ctx->running = false;
    g_activeCollectionResource = nullptr;
    Log(LogLevel::INFO, "SDC task stopped: " + res.name);
}
