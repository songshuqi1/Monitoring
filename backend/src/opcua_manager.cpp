#include "opcua_manager.h"
#include "variable_manager.h"
#include "database.h"
#include <algorithm>
#include <cctype>
#include <regex>

OPCUAManager& OPCUAManager::instance() {
    static OPCUAManager inst;
    return inst;
}

OPCUAManager::OPCUAManager() {
}

OPCUAManager::~OPCUAManager() {
    stopServer();
    disconnectClient();
}

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

static std::string localizedTextToStd(const UA_LocalizedText& value) {
    return uaStringToStd(value.text);
}

static std::string messageSecurityModeToStd(UA_MessageSecurityMode mode) {
    switch (mode) {
        case UA_MESSAGESECURITYMODE_INVALID:
            return "Invalid";
        case UA_MESSAGESECURITYMODE_NONE:
            return "None";
        case UA_MESSAGESECURITYMODE_SIGN:
            return "Sign";
        case UA_MESSAGESECURITYMODE_SIGNANDENCRYPT:
            return "SignAndEncrypt";
        default:
            return std::to_string(static_cast<int>(mode));
    }
}

static void configureClientDefaults(UA_Client* client) {
    if (!client) return;
    UA_ClientConfig_setDefault(UA_Client_getConfig(client));
    UA_ClientConfig* config = UA_Client_getConfig(client);
    config->timeout = 3000;
    config->requestedSessionTimeout = 10000;
}

static void logEndpointDescriptions(const std::string& endpointUrl) {
    UA_Client* discoveryClient = UA_Client_new();
    if (!discoveryClient) return;
    configureClientDefaults(discoveryClient);

    size_t endpointCount = 0;
    UA_EndpointDescription* endpoints = nullptr;
    UA_StatusCode ret = UA_Client_getEndpoints(discoveryClient, endpointUrl.c_str(),
                                               &endpointCount, &endpoints);
    if (ret != UA_STATUSCODE_GOOD) {
        Log(LogLevel::WARN, "OPC UA endpoint discovery failed for " + endpointUrl +
            ": " + std::string(UA_StatusCode_name(ret)));
        UA_Client_delete(discoveryClient);
        return;
    }

    Log(LogLevel::INFO, "--- OPC UA EndpointDescriptions (" + std::to_string(endpointCount) + ") ---");
    for (size_t i = 0; i < endpointCount; ++i) {
        const UA_EndpointDescription& ep = endpoints[i];
        Log(LogLevel::INFO,
            "  [" + std::to_string(i) + "] endpointUrl=" + uaStringToStd(ep.endpointUrl) +
            " applicationName=" + localizedTextToStd(ep.server.applicationName) +
            " applicationUri=" + uaStringToStd(ep.server.applicationUri) +
            " productUri=" + uaStringToStd(ep.server.productUri) +
            " securityMode=" + messageSecurityModeToStd(ep.securityMode));
    }
    Log(LogLevel::INFO, "--- End EndpointDescriptions ---");

    UA_Array_delete(endpoints, endpointCount, &UA_TYPES[UA_TYPES_ENDPOINTDESCRIPTION]);
    UA_Client_delete(discoveryClient);
}

static std::string nodeIdToString(const UA_NodeId& nodeId) {
    switch (nodeId.identifierType) {
        case UA_NODEIDTYPE_NUMERIC:
            return "ns=" + std::to_string(nodeId.namespaceIndex) +
                   ";i=" + std::to_string(nodeId.identifier.numeric);
        case UA_NODEIDTYPE_STRING:
            return "ns=" + std::to_string(nodeId.namespaceIndex) +
                   ";s=" + uaStringToStd(nodeId.identifier.string);
        case UA_NODEIDTYPE_GUID:
            return "ns=" + std::to_string(nodeId.namespaceIndex) + ";g=<guid>";
        case UA_NODEIDTYPE_BYTESTRING:
            return "ns=" + std::to_string(nodeId.namespaceIndex) + ";b=<bytestring>";
        default:
            return "ns=" + std::to_string(nodeId.namespaceIndex) + ";<unknown>";
    }
}

static bool variantToDouble(const UA_Variant& variant, double& value) {
    if (variant.type == &UA_TYPES[UA_TYPES_DOUBLE]) {
        value = *(UA_Double*)variant.data;
    } else if (variant.type == &UA_TYPES[UA_TYPES_FLOAT]) {
        value = *(UA_Float*)variant.data;
    } else if (variant.type == &UA_TYPES[UA_TYPES_INT32]) {
        value = *(UA_Int32*)variant.data;
    } else if (variant.type == &UA_TYPES[UA_TYPES_INT64]) {
        value = static_cast<double>(*(UA_Int64*)variant.data);
    } else if (variant.type == &UA_TYPES[UA_TYPES_UINT32]) {
        value = *(UA_UInt32*)variant.data;
    } else if (variant.type == &UA_TYPES[UA_TYPES_UINT64]) {
        value = static_cast<double>(*(UA_UInt64*)variant.data);
    } else if (variant.type == &UA_TYPES[UA_TYPES_INT16]) {
        value = *(UA_Int16*)variant.data;
    } else if (variant.type == &UA_TYPES[UA_TYPES_UINT16]) {
        value = *(UA_UInt16*)variant.data;
    } else if (variant.type == &UA_TYPES[UA_TYPES_SBYTE]) {
        value = *(UA_SByte*)variant.data;
    } else if (variant.type == &UA_TYPES[UA_TYPES_BYTE]) {
        value = *(UA_Byte*)variant.data;
    } else if (variant.type == &UA_TYPES[UA_TYPES_BOOLEAN]) {
        value = (*(UA_Boolean*)variant.data) ? 1.0 : 0.0;
    } else {
        return false;
    }
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
        try {
            values.emplace_back(key, std::stod((*it)[2].str()));
        } catch (...) {
            // Ignore malformed numeric fields.
        }
    }
    return values;
}

static bool extractNamespaceIndex(const std::string& nodeIdStr, UA_UInt16& nsIndex) {
    std::string s = trimCopy(nodeIdStr);
    if (s.size() < 4 || s[0] != 'n' || s[1] != 's' || s[2] != '=') return false;
    auto semi = s.find(';', 3);
    if (semi == std::string::npos) return false;
    try {
        nsIndex = static_cast<UA_UInt16>(std::stoi(s.substr(3, semi - 3)));
        return true;
    } catch (...) {
        return false;
    }
}

static size_t findNodeIdIdentifierStart(const std::string& s, size_t searchFrom) {
    const char* markers[] = {";i=", ";s=", ";g=", ";b="};
    size_t best = std::string::npos;
    for (const char* marker : markers) {
        auto pos = s.find(marker, searchFrom);
        if (pos != std::string::npos && (best == std::string::npos || pos < best)) best = pos;
    }
    return best;
}

// 解析节点 ID 字符串为 UA_NodeId
// 支持格式:
//   "ns=2;i=2"        → 数值型, namespace 2, id=2
//   "ns=1;s=temperature" → 字符串型, namespace 1, id="temperature"
//   "temperature"     → 字符串型, namespace 1, id="temperature"
static UA_NodeId parseNodeId(const std::string& nodeIdStr) {
    int ns = 1;
    std::string remain = trimCopy(nodeIdStr);
    std::string original = remain;

    // 提取 ns 前缀
    if (original.size() >= 4 && original[0] == 'n' && original[1] == 's' && original[2] == '=') {
        auto semi = original.find(';', 3);
        if (semi != std::string::npos) {
            try {
                ns = std::stoi(original.substr(3, semi - 3));
                remain = original.substr(semi + 1);
            } catch (...) {
                remain = original;
            }
        }
    }

    // 判断是数值型 (i=...) 还是字符串型 (s=...)
    if (remain.size() >= 2 && remain[0] == 'i' && remain[1] == '=') {
        try {
            int id = std::stoi(remain.substr(2));
            return UA_NODEID_NUMERIC(ns, id);
        } catch (...) {
            return UA_NODEID_NULL;
        }
    } else if (remain.size() >= 2 && remain[0] == 's' && remain[1] == '=') {
        return UA_NODEID_STRING_ALLOC(ns, remain.substr(2).c_str());
    } else {
        // 纯字符串名称
        return UA_NODEID_STRING_ALLOC(ns, remain.c_str());
    }
}

static bool parseNodeIdWithNamespaceUri(UA_Client* client,
                                        const std::string& nodeIdStr,
                                        UA_NodeId& out,
                                        std::string& resolvedInfo) {
    std::string s = trimCopy(nodeIdStr);
    if (s.rfind("nsu=", 0) != 0) {
        out = parseNodeId(s);
        return out.identifierType != UA_NODEIDTYPE_NUMERIC || out.identifier.numeric != 0 || out.namespaceIndex != 0;
    }

    if (!client) {
        resolvedInfo = "nsu NodeId requires an active OPC UA client";
        out = UA_NODEID_NULL;
        return false;
    }

    auto marker = findNodeIdIdentifierStart(s, 4);
    if (marker == std::string::npos) {
        resolvedInfo = "invalid nsu NodeId format, expected nsu=<uri>;i=<id> or nsu=<uri>;s=<id>";
        out = UA_NODEID_NULL;
        return false;
    }

    std::string namespaceUri = s.substr(4, marker - 4);
    std::string identifier = s.substr(marker + 1);
    UA_String uaNamespaceUri = UA_STRING_ALLOC(namespaceUri.c_str());
    UA_UInt16 namespaceIndex = 0;
    UA_StatusCode ret = UA_Client_NamespaceGetIndex(client, &uaNamespaceUri, &namespaceIndex);
    UA_String_clear(&uaNamespaceUri);

    if (ret != UA_STATUSCODE_GOOD) {
        resolvedInfo = "namespace URI not found: " + namespaceUri + " status=" + UA_StatusCode_name(ret);
        out = UA_NODEID_NULL;
        return false;
    }

    std::string resolved = "ns=" + std::to_string(namespaceIndex) + ";" + identifier;
    out = parseNodeId(resolved);
    resolvedInfo = "resolved " + s + " -> " + resolved;
    return true;
}

static void browseNodeRecursive(UA_Client* client,
                                const UA_NodeId& nodeId,
                                const std::string& path,
                                int depth,
                                int maxDepth) {
    if (!client || depth > maxDepth) return;

    UA_BrowseRequest bReq;
    UA_BrowseRequest_init(&bReq);
    bReq.requestedMaxReferencesPerNode = 0;
    bReq.nodesToBrowse = UA_BrowseDescription_new();
    bReq.nodesToBrowseSize = 1;
    bReq.nodesToBrowse[0].nodeId = nodeId;
    bReq.nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_ALL;

    UA_BrowseResponse bResp = UA_Client_Service_browse(client, bReq);
    if (bResp.responseHeader.serviceResult != UA_STATUSCODE_GOOD) {
        Log(LogLevel::WARN, "Browse failed at " + path + ": " +
            std::string(UA_StatusCode_name(bResp.responseHeader.serviceResult)));
        UA_BrowseRequest_clear(&bReq);
        UA_BrowseResponse_clear(&bResp);
        return;
    }

    for (size_t i = 0; i < bResp.resultsSize; ++i) {
        for (size_t j = 0; j < bResp.results[i].referencesSize; ++j) {
            UA_ReferenceDescription* ref = &bResp.results[i].references[j];
            if (ref->nodeId.serverIndex != 0 || ref->nodeId.namespaceUri.length > 0) continue;

            std::string name = localizedTextToStd(ref->displayName);
            std::string childId = nodeIdToString(ref->nodeId.nodeId);
            std::string childPath = path.empty() ? name : path + "/" + name;
            std::string indent(static_cast<size_t>(depth) * 2, ' ');
            Log(LogLevel::INFO, indent + childPath + " -> " + childId);

            if (depth < maxDepth && ref->nodeClass == UA_NODECLASS_OBJECT) {
                UA_NodeId child;
                UA_NodeId_init(&child);
                if (UA_NodeId_copy(&ref->nodeId.nodeId, &child) == UA_STATUSCODE_GOOD) {
                    browseNodeRecursive(client, child, childPath, depth + 1, maxDepth);
                    UA_NodeId_clear(&child);
                }
            }
        }
    }

    UA_BrowseRequest_clear(&bReq);
    UA_BrowseResponse_clear(&bResp);
}

// ============ 服务器端 ============

void OPCUAManager::serverRunLoop(UA_Server* server, UA_Boolean* running) {
    UA_Server_run(server, running);
}

bool OPCUAManager::startServer(uint16_t port) {
    if (server_) return true;

    server_ = UA_Server_new();
    if (!server_) {
        Log(LogLevel::ERR, "OPC UA Server creation failed");
        return false;
    }
    UA_ServerConfig* config = UA_Server_getConfig(server_);
    UA_StatusCode ret = UA_ServerConfig_setMinimal(config, port, NULL);
    if (ret != UA_STATUSCODE_GOOD) {
        Log(LogLevel::ERR, "OPC UA Server config failed: " + std::string(UA_StatusCode_name(ret)));
        UA_Server_delete(server_);
        server_ = nullptr;
        return false;
    }

    // 从变量管理器加载变量并创建 OPC UA 节点
    auto vars = VariableManager::instance().getAllDefinitions();
    for (const auto& v : vars) {
            UA_VariableAttributes attr = UA_VariableAttributes_default;
            double defaultVal = 0.0;
            UA_Variant_setScalarCopy(&attr.value, &defaultVal, &UA_TYPES[UA_TYPES_DOUBLE]);
            attr.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", v.name.c_str());
            attr.description = UA_LOCALIZEDTEXT_ALLOC("en-US", v.description.c_str());

            UA_NodeId nodeId = parseNodeId(v.name);
            UA_QualifiedName qname = UA_QUALIFIEDNAME_ALLOC(1, v.name.c_str());
            UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
            UA_NodeId parentRef = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);

            UA_Server_addVariableNode(server_, nodeId, parentNodeId,
                                      parentRef, qname, UA_NODEID_NULL,
                                      attr, NULL, NULL);

            UA_VariableAttributes_clear(&attr);
            UA_NodeId_clear(&nodeId);
            UA_QualifiedName_clear(&qname);
    }

    serverRunning_ = true;
    serverThread_ = std::thread(serverRunLoop, server_, &serverRunning_);
    Log(LogLevel::INFO, "OPC UA Server started on port " + std::to_string(port));
    return true;
}

void OPCUAManager::stopServer() {
    if (server_) {
        serverRunning_ = false;
        if (serverThread_.joinable()) serverThread_.join();
        UA_Server_delete(server_);
        server_ = nullptr;
        Log(LogLevel::INFO, "OPC UA Server stopped");
    }
}

bool OPCUAManager::isServerRunning() const {
    return serverRunning_;
}

bool OPCUAManager::updateServerVariable(const std::string& nodeId, double value) {
    if (!server_) return false;

    UA_NodeId uaNodeId = parseNodeId(nodeId);
    UA_Variant var;
    UA_Variant_setScalar(&var, &value, &UA_TYPES[UA_TYPES_DOUBLE]);

    UA_StatusCode ret = UA_Server_writeValue(server_, uaNodeId, var);
    UA_NodeId_clear(&uaNodeId);

    if (ret != UA_STATUSCODE_GOOD) {
        Log(LogLevel::WARN, "Failed to update OPC UA variable " + nodeId +
            " status: " + std::to_string(ret));
        return false;
    }
    return true;
}

// ============ 客户端 ============

bool OPCUAManager::connectClient(const std::string& endpointUrl) {
    if (client_) {
        UA_Client_disconnect(client_);
        UA_Client_delete(client_);
        client_ = nullptr;
    }

    logEndpointDescriptions(endpointUrl);

    client_ = UA_Client_new();
    configureClientDefaults(client_);

    UA_StatusCode ret = UA_Client_connect(client_, endpointUrl.c_str());
    if (ret != UA_STATUSCODE_GOOD) {
        Log(LogLevel::ERR, "OPC UA Client connection failed to " + endpointUrl +
            " status: " + std::to_string(ret));
        UA_Client_delete(client_);
        client_ = nullptr;
        clientConnected_ = false;
        return false;
    }

    clientConnected_ = true;
    Log(LogLevel::INFO, "OPC UA Client connected to " + endpointUrl);
    logNamespaceArray();
    // 浏览服务端 Objects 节点列表以便调试
    browseRemoteNodes();
    return true;
}

void OPCUAManager::disconnectClient() {
    if (client_) {
        UA_Client_disconnect(client_);
        UA_Client_delete(client_);
        client_ = nullptr;
        clientConnected_ = false;
        Log(LogLevel::INFO, "OPC UA Client disconnected");
    }
}

bool OPCUAManager::isClientConnected() const {
    return clientConnected_;
}

bool OPCUAManager::readNodeVariant(const std::string& nodeId, double& numericValue,
                                   std::string& stringValue, bool& isString,
                                   std::string& quality) {
    if (!client_ || !clientConnected_) return false;

    UA_NodeId uaNodeId;
    std::string resolvedInfo;
    if (!parseNodeIdWithNamespaceUri(client_, nodeId, uaNodeId, resolvedInfo)) {
        Log(LogLevel::WARN, "OPC UA read invalid nodeId=" + nodeId + " " + resolvedInfo);
        quality = "BAD";
        return false;
    }
    if (!resolvedInfo.empty()) Log(LogLevel::DEBUG, "OPC UA " + resolvedInfo);

    UA_Variant variant;
    UA_Variant_init(&variant);

    UA_StatusCode ret = UA_Client_readValueAttribute(client_, uaNodeId, &variant);
    UA_NodeId_clear(&uaNodeId);

    if (ret != UA_STATUSCODE_GOOD) {
        Log(LogLevel::WARN, "OPC UA read " + std::string(UA_StatusCode_name(ret)) + " nodeId=" + nodeId);
        UA_UInt16 nsIndex = 0;
        if (ret == UA_STATUSCODE_BADNODEIDUNKNOWN && extractNamespaceIndex(nodeId, nsIndex)) {
            std::string nsUri = namespaceUriForIndex(nsIndex);
            Log(LogLevel::WARN,
                "OPC UA namespace hint: ns=" + std::to_string(nsIndex) +
                " uri=" + (nsUri.empty() ? "<not found>" : nsUri) +
                ". If this NodeId was copied from another tool/session, prefer nsu=<namespace-uri>;i=... or verify the endpoint and browse result.");
        }
        quality = "BAD";
        UA_Variant_clear(&variant);
        return false;
    }
    if (!UA_Variant_isScalar(&variant)) {
        Log(LogLevel::WARN, "OPC UA read not scalar nodeId=" + nodeId);
        quality = "BAD";
        UA_Variant_clear(&variant);
        return false;
    }

    if (variant.type == &UA_TYPES[UA_TYPES_STRING]) {
        stringValue = uaStringToStd(*(UA_String*)variant.data);
        isString = true;
    } else if (variantToDouble(variant, numericValue)) {
        isString = false;
    } else {
        Log(LogLevel::WARN, "OPC UA read unsupported scalar type nodeId=" + nodeId);
        quality = "BAD";
        UA_Variant_clear(&variant);
        return false;
    }

    quality = "GOOD";
    UA_Variant_clear(&variant);
    return true;
}

bool OPCUAManager::readVariable(const std::string& nodeId, double& value, std::string& quality) {
    std::string stringValue;
    bool isString = false;
    if (!readNodeVariant(nodeId, value, stringValue, isString, quality)) return false;
    if (!isString) return true;

    auto fields = parseNumericDictionary(stringValue);
    if (!fields.empty()) {
        value = fields.front().second;
        quality = "GOOD";
        return true;
    }

    Log(LogLevel::WARN, "OPC UA read string value is not numeric JSON dictionary nodeId=" + nodeId);
    quality = "BAD";
    return false;
}

bool OPCUAManager::writeVariable(const std::string& nodeId, double value) {
    if (!client_ || !clientConnected_) return false;

    UA_NodeId uaNodeId;
    std::string resolvedInfo;
    if (!parseNodeIdWithNamespaceUri(client_, nodeId, uaNodeId, resolvedInfo)) {
        Log(LogLevel::WARN, "OPC UA write invalid nodeId=" + nodeId + " " + resolvedInfo);
        return false;
    }
    UA_Variant var;
    UA_Variant_setScalar(&var, &value, &UA_TYPES[UA_TYPES_DOUBLE]);

    UA_StatusCode ret = UA_Client_writeValueAttribute(client_, uaNodeId, &var);
    UA_NodeId_clear(&uaNodeId);

    if (ret != UA_STATUSCODE_GOOD) {
        Log(LogLevel::WARN, "OPC UA write failed for " + nodeId +
            " status: " + std::to_string(ret));
        return false;
    }
    return true;
}

// ============ 轮询 ============

bool OPCUAManager::startRemoteCollection(const std::string& endpoint,
                                          const std::vector<ResourceNode>& nodes,
                                          int intervalMs) {
    if (polling_) return false;
    remoteEndpoint_ = endpoint;
    remoteNodes_ = nodes;
    polling_ = true;
    pollIntervalMs_ = intervalMs;

    // 先连接远程服务器
    if (!connectClient(endpoint)) {
        polling_ = false;
        return false;
    }

    pollThread_ = std::thread([this]() {
        try {
        Log(LogLevel::INFO, "Remote collection started: " + remoteEndpoint_ +
            " interval=" + std::to_string(pollIntervalMs_) + "ms nodes=" + std::to_string(remoteNodes_.size()) + ")");

        auto updateCollectedValue = [this](const std::string& variableName,
                                           const std::string& sourceNodeId,
                                           double value,
                                           const std::string& quality) {
            if (variableName.empty()) return false;

            auto allVars = VariableManager::instance().getAllDefinitions();
            for (const auto& v : allVars) {
                if (v.name == variableName) {
                    VariableManager::instance().updateValue(v.id, value, quality);
                    VariableManager::instance().appendHistory(v.id, value, nowMs());
                    return true;
                }
            }

            VariableInfo newVar;
            newVar.name = variableName;
            newVar.description = "从 " + remoteEndpoint_ + " 自动采集";
            newVar.source = "OPCUA";
            newVar.opcuaNodeId = sourceNodeId;
            VariableManager::instance().addOrUpdateDefinition(newVar);
            Log(LogLevel::INFO, "Auto-created: " + variableName + " -> " + sourceNodeId);

            auto refreshedVars = VariableManager::instance().getAllDefinitions();
            for (const auto& v : refreshedVars) {
                if (v.name == variableName) {
                    VariableManager::instance().updateValue(v.id, value, quality);
                    VariableManager::instance().appendHistory(v.id, value, nowMs());
                    return true;
                }
            }
            return false;
        };

        while (polling_) {
            if (clientConnected_) {
                Log(LogLevel::DEBUG, "OPC UA poll reading " + std::to_string(remoteNodes_.size()) + " nodes...");
                for (const auto& node : remoteNodes_) {
                    double numericValue = 0.0;
                    std::string stringValue;
                    bool isString = false;
                    std::string quality;

                    if (readNodeVariant(node.nodeId, numericValue, stringValue, isString, quality)) {
                        if (isString) {
                            auto fields = parseNumericDictionary(stringValue);
                            if (fields.empty()) {
                                Log(LogLevel::WARN,
                                    "OPC UA read string has no numeric dictionary fields: name=" +
                                    node.name + " nodeId=" + node.nodeId + " value=" + stringValue);
                                continue;
                            }

                            for (const auto& field : fields) {
                                updateCollectedValue(field.first, node.nodeId, field.second, quality);
                                Log(LogLevel::DEBUG,
                                    "OPC UA dictionary read OK: " + field.first + "=" +
                                    std::to_string(field.second) + " from " + node.nodeId);
                            }
                        } else {
                            updateCollectedValue(node.name, node.nodeId, numericValue, quality);
                            Log(LogLevel::DEBUG,
                                "OPC UA read OK: " + node.name + "=" + std::to_string(numericValue));
                        }
                    } else {
                        Log(LogLevel::WARN, "OPC UA read FAILED: name=" + node.name + " nodeId=" + node.nodeId);
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs_));
        }
        } catch (const std::exception& e) {
            Log(LogLevel::ERR, std::string("Remote collection thread crashed: ") + e.what());
            polling_ = false;
        } catch (...) {
            Log(LogLevel::ERR, "Remote collection thread crashed: unknown exception");
            polling_ = false;
        }
    });
    return true;
}

void OPCUAManager::stopRemoteCollection() {
    polling_ = false;
    if (pollThread_.joinable()) pollThread_.join();
    disconnectClient();
    Log(LogLevel::INFO, "OPC UA polling stopped");
}

void OPCUAManager::logNamespaceArray() {
    if (!client_ || !clientConnected_) return;

    UA_Variant value;
    UA_Variant_init(&value);
    UA_NodeId namespaceArrayId = UA_NODEID_NUMERIC(0, UA_NS0ID_SERVER_NAMESPACEARRAY);
    UA_StatusCode ret = UA_Client_readValueAttribute(client_, namespaceArrayId, &value);
    if (ret != UA_STATUSCODE_GOOD) {
        Log(LogLevel::WARN, "OPC UA NamespaceArray read failed: " + std::string(UA_StatusCode_name(ret)));
        UA_Variant_clear(&value);
        return;
    }

    Log(LogLevel::INFO, "--- OPC UA NamespaceArray ---");
    if (value.type == &UA_TYPES[UA_TYPES_STRING] && value.data && value.arrayLength > 0) {
        auto* namespaces = static_cast<UA_String*>(value.data);
        for (size_t i = 0; i < value.arrayLength; ++i) {
            Log(LogLevel::INFO, "  ns=" + std::to_string(i) + " uri=" + uaStringToStd(namespaces[i]));
        }
    } else {
        Log(LogLevel::WARN, "OPC UA NamespaceArray has unexpected data type");
    }
    Log(LogLevel::INFO, "--- End NamespaceArray ---");
    UA_Variant_clear(&value);
}

std::string OPCUAManager::namespaceUriForIndex(UA_UInt16 namespaceIndex) {
    if (!client_ || !clientConnected_) return "";

    UA_Variant value;
    UA_Variant_init(&value);
    UA_NodeId namespaceArrayId = UA_NODEID_NUMERIC(0, UA_NS0ID_SERVER_NAMESPACEARRAY);
    UA_StatusCode ret = UA_Client_readValueAttribute(client_, namespaceArrayId, &value);
    if (ret != UA_STATUSCODE_GOOD) {
        UA_Variant_clear(&value);
        return "";
    }

    std::string result;
    if (value.type == &UA_TYPES[UA_TYPES_STRING] && value.data && namespaceIndex < value.arrayLength) {
        auto* namespaces = static_cast<UA_String*>(value.data);
        result = uaStringToStd(namespaces[namespaceIndex]);
    }
    UA_Variant_clear(&value);
    return result;
}

// ============ 浏览远程 OPC UA 服务端节点 ============
void OPCUAManager::browseRemoteNodes() {
    if (!client_ || !clientConnected_) return;

    Log(LogLevel::INFO, "--- OPC UA server node browser (Objects recursive) ---");
    UA_NodeId objectsNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    browseNodeRecursive(client_, objectsNodeId, "Objects", 0, 4);
    Log(LogLevel::INFO, "--- End of node browser ---");
}

// 向后兼容桥接
void OPCUAManager::startPolling(int) {
    Log(LogLevel::WARN, "startPolling() deprecated, use startRemoteCollection()");
}
void OPCUAManager::stopPolling() {
    stopRemoteCollection();
}
