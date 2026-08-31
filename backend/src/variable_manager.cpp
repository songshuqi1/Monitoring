#include "variable_manager.h"
#include "database.h"
#include "communication_manager.h"
#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <unordered_set>

namespace {
std::string uppercaseCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return value;
}

bool isCollectedProtocol(const std::string& source) {
    return source == "OPCUA" || source == "MODBUS" || source == "UDP" || source == "SDC";
}

int64_t staleAfterMs(const CommunicationResource& resource, const std::string& source) {
    const int64_t interval = std::clamp<int64_t>(resource.pollIntervalMs, 50, 600000);
    // Allow for an entire collection cycle and normal transport jitter, while
    // still making a stopped source visible quickly in the client.
    const int64_t cycles = source == "UDP" ? 3 : 4;
    return std::max<int64_t>(5000, interval * cycles + 1000);
}
}

VariableManager& VariableManager::instance() {
    static VariableManager inst;
    return inst;
}

bool VariableManager::loadDefinitionsFromDB() {
    auto vars = DatabaseManager::instance().loadVariableDefinitions();
    std::unique_lock lock(defMutex_);
    definitions_.clear();
    nameToId_.clear();
    for (const auto& v : vars) {
        definitions_[v.id] = v;
        nameToId_[v.name] = v.id;
    }
    Log(LogLevel::INFO, "Loaded " + std::to_string(vars.size()) + " variable definitions from DB");
    return true;
}

bool VariableManager::addOrUpdateDefinition(const VariableInfo& info) {
    std::unique_lock lock(defMutex_);

    // 先尝试保存到数据库
    bool dbOk = DatabaseManager::instance().saveVariableDefinition(info);
    if (dbOk) {
        // 从DB重新加载得到正确ID
        auto vars = DatabaseManager::instance().loadVariableDefinitions();
        definitions_.clear();
        nameToId_.clear();
        for (const auto& v : vars) {
            definitions_[v.id] = v;
            nameToId_[v.name] = v.id;
        }
        Log(LogLevel::DEBUG, "addOrUpdateDefinition saved to DB: " + info.name);
    } else {
        // DB 不可用时，在内存中分配一个负 ID
        static int fallbackId = -1;
        VariableInfo memInfo = info;
        if (memInfo.id <= 0) memInfo.id = fallbackId--;
        definitions_[memInfo.id] = memInfo;
        nameToId_[memInfo.name] = memInfo.id;
        Log(LogLevel::DEBUG, "addOrUpdateDefinition saved to memory: " + info.name + " id=" + std::to_string(memInfo.id));
    }
    return true;
}

bool VariableManager::removeDefinition(int varId) {
    return !removeDefinitions({varId}).empty();
}

std::vector<int> VariableManager::removeDefinitions(const std::vector<int>& varIds) {
    std::unordered_set<int> targets;
    for (int id : varIds) {
        if (id != 0) targets.insert(id);
    }
    if (targets.empty()) return {};

    std::vector<int> existing;
    {
        std::shared_lock lock(defMutex_);
        existing.reserve(targets.size());
        for (int id : targets) {
            if (definitions_.count(id)) existing.push_back(id);
        }
    }
    if (existing.empty()) return {};

    auto& db = DatabaseManager::instance();
    const bool persisted = !db.isConnected() || db.deleteVariableDefinitions(existing);
    if (!persisted) return {};

    {
        std::unique_lock lock(defMutex_);
        for (int id : existing) {
            auto it = definitions_.find(id);
            if (it == definitions_.end()) continue;
            nameToId_.erase(it->second.name);
            definitions_.erase(it);
        }
    }
    {
        std::unique_lock lock(rtMutex_);
        for (int id : existing) realtimeData_.erase(id);
    }
    {
        std::unique_lock lock(histMutex_);
        for (int id : existing) historyCache_.erase(id);
    }
    return existing;
}

std::vector<int> VariableManager::removeOfflineDefinitions() {
    std::vector<int> offlineIds;
    auto definitions = getAllDefinitions();
    const auto realtime = getAllLatestData();
    std::unordered_map<int, DataPoint> latestById;
    latestById.reserve(realtime.size());
    for (const auto& point : realtime) latestById[point.varId] = point;
    for (const auto& definition : definitions) {
        const auto pointIt = latestById.find(definition.id);
        const DataPoint point = pointIt == latestById.end() ? DataPoint{} : pointIt->second;
        std::string quality = point.quality;
        std::transform(quality.begin(), quality.end(), quality.begin(), [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });
        if (point.varId == 0 || quality == "OFFLINE" || quality == "DISCONNECTED") {
            offlineIds.push_back(definition.id);
        }
    }
    return removeDefinitions(offlineIds);
}

std::vector<VariableInfo> VariableManager::getAllDefinitions() const {
    std::shared_lock lock(defMutex_);
    std::vector<VariableInfo> result;
    result.reserve(definitions_.size());
    for (const auto& [id, info] : definitions_) {
        result.push_back(info);
    }
    return result;
}

VariableInfo VariableManager::getDefinition(int varId) const {
    std::shared_lock lock(defMutex_);
    auto it = definitions_.find(varId);
    if (it != definitions_.end()) return it->second;
    return VariableInfo{};
}

void VariableManager::updateValue(int varId, double value, const std::string& quality) {
    {
        std::unique_lock lock(rtMutex_);
        auto& dp = realtimeData_[varId];
        dp.varId = varId;
        dp.value = value;
        dp.quality = quality;
        dp.timestampMs = nowMs();

        // 补全 varName
        if (dp.varName.empty()) {
            std::shared_lock dlock(defMutex_);
            auto it = definitions_.find(varId);
            if (it != definitions_.end()) {
                dp.varName = it->second.name;
            }
        }
    }

    // 回调通知
    if (dataCallback_) {
        std::shared_lock lock(rtMutex_);
        dataCallback_(realtimeData_[varId]);
    }
}

DataPoint VariableManager::getLatestData(int varId) const {
    std::shared_lock lock(rtMutex_);
    auto it = realtimeData_.find(varId);
    if (it != realtimeData_.end()) return it->second;
    return DataPoint{};
}

std::vector<DataPoint> VariableManager::getAllLatestData() const {
    std::vector<DataPoint> result;
    {
        std::shared_lock lock(rtMutex_);
        result.reserve(realtimeData_.size());
        for (const auto& [id, dp] : realtimeData_) {
            result.push_back(dp);
        }
    }

    // Quality has two parts: the protocol's latest result and the freshness
    // of that result.  A previously GOOD value must never remain normal after
    // its communication resource stops delivering data.
    const auto definitions = getAllDefinitions();
    std::unordered_map<int, VariableInfo> definitionsById;
    definitionsById.reserve(definitions.size());
    for (const auto& definition : definitions) definitionsById[definition.id] = definition;

    const auto resources = CommunicationManager::instance().listResources();
    std::unordered_map<std::string, CommunicationResource> resourcesById;
    resourcesById.reserve(resources.size());
    for (const auto& resource : resources) resourcesById[resource.id] = resource;

    const int64_t currentMs = nowMs();
    for (auto& point : result) {
        const auto definitionIt = definitionsById.find(point.varId);
        if (definitionIt == definitionsById.end()) continue;
        const auto& definition = definitionIt->second;
        const std::string source = uppercaseCopy(definition.source);
        if (!isCollectedProtocol(source) || definition.resourceId.empty()) continue;

        const auto resourceIt = resourcesById.find(definition.resourceId);
        const bool resourceMissing = resourceIt == resourcesById.end();
        const bool resourceDisabled = !resourceMissing && !resourceIt->second.enabled;
        const int64_t timeoutMs = resourceMissing ? 5000 : staleAfterMs(resourceIt->second, source);
        const bool stale = point.timestampMs <= 0 || currentMs - point.timestampMs > timeoutMs;
        if (resourceMissing || resourceDisabled || stale) point.quality = "OFFLINE";
    }
    return result;
}

void VariableManager::appendHistory(int varId, double value, int64_t timestampMs) {
    std::string varName;
    {
        std::shared_lock dlock(defMutex_);
        auto it = definitions_.find(varId);
        if (it != definitions_.end()) {
            varName = it->second.name;
        }
    }

    HistoryPoint hp;
    hp.varId = varId;
    hp.varName = varName;
    hp.value = value;
    hp.quality = "GOOD";
    hp.timestampMs = timestampMs;

    std::unique_lock lock(histMutex_);
    auto& deque = historyCache_[varId];
    deque.push_back(hp);
    if (deque.size() > MAX_HISTORY_POINTS) {
        deque.pop_front();
    }
}

std::vector<HistoryPoint> VariableManager::getHistory(int varId, int64_t fromMs, int64_t toMs, int maxPoints) const {
    std::shared_lock lock(histMutex_);
    auto it = historyCache_.find(varId);
    if (it == historyCache_.end()) return {};

    const auto& deque = it->second;
    std::vector<HistoryPoint> result;
    result.reserve(std::min(deque.size(), static_cast<size_t>(maxPoints)));

    bool filterByTime = (fromMs > 0 || toMs > 0);
    int64_t effectiveTo = (toMs > 0) ? toMs : INT64_MAX;

    for (const auto& hp : deque) {
        if (filterByTime) {
            if (hp.timestampMs < fromMs) continue;
            if (hp.timestampMs > effectiveTo) continue;
        }
        result.push_back(hp);
    }

    // 如果结果超过 maxPoints，进行抽稀
    if (result.size() > static_cast<size_t>(maxPoints)) {
        std::vector<HistoryPoint> sampled;
        sampled.reserve(maxPoints);
        double step = static_cast<double>(result.size()) / maxPoints;
        for (int i = 0; i < maxPoints; ++i) {
            size_t idx = static_cast<size_t>(i * step);
            if (idx >= result.size()) idx = result.size() - 1;
            sampled.push_back(result[idx]);
        }
        return sampled;
    }

    return result;
}

void VariableManager::clearHistory(int varId) {
    std::unique_lock lock(histMutex_);
    historyCache_.erase(varId);
}

bool VariableManager::flushToDatabase() {
    auto& db = DatabaseManager::instance();
    std::vector<DataPoint> snapshot;

    {
        std::shared_lock lock(rtMutex_);
        snapshot.reserve(realtimeData_.size());
        for (const auto& [id, dp] : realtimeData_) {
            snapshot.push_back(dp);
        }
    }

    std::unordered_map<int, std::string> definitionNames;
    {
        std::shared_lock lock(defMutex_);
        for (const auto& [id, info] : definitions_) {
            definitionNames[id] = info.name;
        }
    }

    for (auto& dp : snapshot) {
        if (!definitionNames.count(dp.varId)) continue;
        if (dp.varName.empty()) {
            auto it = definitionNames.find(dp.varId);
            if (it != definitionNames.end()) dp.varName = it->second;
        }
        if (dp.quality.empty()) dp.quality = "GOOD";
        db.upsertRealtimeData(dp.varId, dp.varName, dp.value, dp.quality, dp.timestampMs);
    }

    if (!snapshot.empty()) {
        db.insertHistoricalSnapshot(snapshot, nowMs());
    }
    return true;
}
