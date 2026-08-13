#ifndef MONITORING_HTTP_SERVER_H
#define MONITORING_HTTP_SERVER_H

#include "common.h"
#include <thread>
#include <atomic>
#include <map>
#include <winsock2.h>
#include <windows.h>

/**
 * HttpServer
 * 轻量级 HTTP REST API 服务器（无第三方依赖）
 * 为 Vue 前端提供 RESTful API + WebSocket 实时推送
 *
 * API 路由:
 *   GET  /api/variables          -> 获取所有变量定义
 *   GET  /api/variables/:id      -> 获取单个变量
 *   POST /api/variables          -> 添加变量
 *   PUT  /api/variables/:id      -> 更新变量
 *   DELETE /api/variables/:id    -> 删除变量
 *   GET  /api/realtime           -> 获取所有实时数据
 *   GET  /api/history/:id?from=&to=&max=      -> 获取缓存历史数据
 *   GET  /api/historydb/:id?from=&to=&max=    -> 获取数据库长表历史数据
 *   GET  /api/historydb/wide?from=&to=&max=   -> 获取数据库宽表历史数据
 *   POST /api/write/:id          -> 写入变量值
 *   GET  /api/alarms             -> 获取报警列表
 *   POST /api/alarms/:id/ack     -> 确认报警
 *   GET  /api/logs               -> 获取操作日志
 *   GET  /api/config/:key        -> 获取配置
 *   POST /api/config/:key        -> 设置配置
 *   WS   /ws/realtime            -> WebSocket 实时推送
 */
class HttpServer {
public:
    static HttpServer& instance();

    bool start(uint16_t port = 8081);
    void stop();
    bool isRunning() const;

    // 广播实时数据到所有 WebSocket 客户端
    void broadcastRealtime(const std::string& jsonData);

private:
    HttpServer() = default;
    ~HttpServer() { stop(); }
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    void serverLoop();

    // 处理 HTTP 请求
    std::string handleRequest(const std::string& method, const std::string& path,
                               const std::string& body, const std::string& contentType,
                               const std::map<std::string, std::string>& queryParams,
                               const std::string& authToken = "");

    // 解析路径
    std::vector<std::string> splitPath(const std::string& path);

    // WebSocket 管理
    struct WSClient {
        SOCKET sock;
        bool handshakeDone;
    };
    std::vector<WSClient> wsClients_;
    std::mutex wsMutex_;
    bool performWebSocketHandshake(SOCKET clientSock, const std::string& request);
    void handleWSClient(SOCKET clientSock);
    void broadcastToWS(const std::string& data);

    // 监听套接字
    SOCKET listenSock_ = INVALID_SOCKET;
    std::atomic<bool> running_{false};
    std::thread serverThread_;
    uint16_t port_ = 8080;
};

#endif // MONITORING_HTTP_SERVER_H
