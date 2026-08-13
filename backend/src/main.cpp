/**
 * MonitoringPlatform - 西门子 WinCC 风格监控平台
 * 后端 C++ 主程序
 *
 * 编译: Visual Studio 2026
 * 依赖:
 *   - open62541-1.0 (OPC UA)
 *   - MySQL Connector/C (libmysql)
 *   - Windows Socket API
 */

#include "common.h"
#include "variable_manager.h"
#include "database.h"
#include "opcua_manager.h"
#include "udp_manager.h"
#include "http_server.h"
#include <csignal>
#include <iostream>
#include <cstdlib>

static std::atomic<bool> g_running{true};

void signalHandler(int) {
    Log(LogLevel::INFO, "Shutdown signal received...");
    g_running = false;
}

// 数据持久化定时器 - 定期刷新实时快照和历史数据
static void dataPersistenceLoop(int intervalMs) {
    Log(LogLevel::INFO, "Data persistence loop started");

    try {
        while (g_running) {
            VariableManager::instance().flushToDatabase();
            std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
        }
    } catch (const std::exception& e) {
        Log(LogLevel::ERR, std::string("Data persistence loop crashed: ") + e.what());
    } catch (...) {
        Log(LogLevel::ERR, "Data persistence loop crashed: unknown exception");
    }
}

// WebSocket 广播线程
static void wsBroadcastLoop(int intervalMs) {
    Log(LogLevel::INFO, "WebSocket broadcast loop started");

    try {
        while (g_running) {
            auto data = VariableManager::instance().getAllLatestData();
            if (!data.empty()) {
                std::string json = "[";
                for (size_t i = 0; i < data.size(); ++i) {
                    json += data[i].toJson();
                    if (i < data.size() - 1) json += ",";
                }
                json += "]";
                HttpServer::instance().broadcastRealtime(json);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
        }
    } catch (const std::exception& e) {
        Log(LogLevel::ERR, std::string("WebSocket broadcast loop crashed: ") + e.what());
    } catch (...) {
        Log(LogLevel::ERR, "WebSocket broadcast loop crashed: unknown exception");
    }
}

int main() {
    try {
    // 注册信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    Log(LogLevel::INFO, std::string("=== ") + APP_NAME + " v" + APP_VERSION + " ===");
    Log(LogLevel::INFO, "Starting monitoring platform...");

    // ========== 1. 连接数据库 ==========
    auto& db = DatabaseManager::instance();
    std::string dbHost = "localhost";
    int dbPort = 3306;
    std::string dbUser = "root";
    std::string dbPass = "123456";
    std::string dbName = "software_plc_monitoring";

    // 从环境变量读取数据库配置
    char* envHost = getenv("DB_HOST");
    if (envHost) dbHost = envHost;
    char* envPort = getenv("DB_PORT");
    if (envPort) {
        try { dbPort = std::stoi(envPort); }
        catch (...) { Log(LogLevel::WARN, "Invalid DB_PORT env value, using default"); }
    }
    char* envUser = getenv("DB_USER");
    if (envUser) dbUser = envUser;
    char* envPass = getenv("DB_PASS");
    if (envPass) dbPass = envPass;
    char* envDb = getenv("DB_NAME");
    if (envDb) dbName = envDb;

    if (!db.connect(dbHost, dbPort, dbUser, dbPass, dbName)) {
        Log(LogLevel::WARN, "Database connection failed. Running in limited mode.");
        Log(LogLevel::WARN, "Please ensure MySQL is running and execute backend/sql/init.sql");
    } else {
        // 加载变量定义
        VariableManager::instance().loadDefinitionsFromDB();
    }

    // ========== 2. 启动 OPC UA 服务器 ==========
    int opcuaServerPort = 4841;
    char* envOpcuaPort = getenv("OPCUA_SERVER_PORT");
    if (envOpcuaPort) {
        try { opcuaServerPort = std::stoi(envOpcuaPort); }
        catch (...) { Log(LogLevel::WARN, "Invalid OPCUA_SERVER_PORT env value, using default"); }
    }
    OPCUAManager::instance().startServer(static_cast<uint16_t>(opcuaServerPort));

    // ========== 3. 启动 UDP 接收器 ==========
    // UDP 接收器由前端通信资源页面控制，不再自动启动
    Log(LogLevel::INFO, "UDP Receiver: waiting for frontend to start (port configured in Communication Resources)");

    // ========== 4. 启动 HTTP 服务器 ==========
    int httpServerPort = 8081;
    char* envHttpPort = getenv("HTTP_SERVER_PORT");
    if (envHttpPort) {
        try { httpServerPort = std::stoi(envHttpPort); }
        catch (...) { Log(LogLevel::WARN, "Invalid HTTP_SERVER_PORT env value, using default"); }
    }
    HttpServer::instance().start(static_cast<uint16_t>(httpServerPort));

    // ========== 5. 启动数据持久化 ==========
    std::thread persistenceThread(dataPersistenceLoop, 10000);

    // ========== 6. 启动 WebSocket 广播 ==========
    std::thread wsThread(wsBroadcastLoop, 500);

    // ========== 主循环 ==========
    Log(LogLevel::INFO, "=== All services started ===");
    Log(LogLevel::INFO, "OPC UA Server: opc.tcp://localhost:" + std::to_string(opcuaServerPort));
    Log(LogLevel::INFO, "UDP Receiver:  waiting (start from frontend Communication Resources)");
    Log(LogLevel::INFO, "HTTP Server:   http://localhost:" + std::to_string(httpServerPort));
    Log(LogLevel::INFO, "Press Ctrl+C to shutdown");

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // ========== 清理 ==========
    Log(LogLevel::INFO, "Shutting down...");

    db.disconnect();
    if (persistenceThread.joinable()) persistenceThread.join();
    if (wsThread.joinable()) wsThread.join();
    OPCUAManager::instance().stopPolling();
    OPCUAManager::instance().stopServer();
    UDPManager::instance().stopReceiver();
    HttpServer::instance().stop();

    Log(LogLevel::INFO, "Shutdown complete.");
    return 0;

    } catch (const std::exception& e) {
        Log(LogLevel::ERR, std::string("FATAL: main() exception: ") + e.what());
        return 1;
    } catch (...) {
        Log(LogLevel::ERR, "FATAL: main() unknown exception");
        return 1;
    }
}
