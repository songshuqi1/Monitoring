cmake_minimum_required(VERSION 3.20)
project(MonitoringPlatform VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# 作者信息
# ============================================================
set(AUTHOR "Monitoring Platform Team")
set(DESCRIPTION "WinCC-like Monitoring Platform with OPC UA and UDP")

# ============================================================
# open62541 配置
# ============================================================
set(OPEN62541_DIR "${CMAKE_SOURCE_DIR}/../open62541-1.0")
if(NOT EXISTS "${OPEN62541_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "open62541 not found at ${OPEN62541_DIR}")
endif()
set(UA_BUILD_STATIC_LIBRARY ON CACHE BOOL "")
set(UA_ENABLE_AMALGAMATION ON CACHE BOOL "")
set(UA_ENABLE_DISCOVERY OFF CACHE BOOL "")
set(UA_ENABLE_DISCOVERY_MULTICAST OFF CACHE BOOL "")
set(UA_ENABLE_SUBSCRIPTIONS ON CACHE BOOL "")
set(UA_ENABLE_HISTORIZING OFF CACHE BOOL "")
set(UA_ENABLE_JSON_ENCODING OFF CACHE BOOL "")
set(UA_ENABLE_PUBSUB OFF CACHE BOOL "")
set(UA_LOGLEVEL 300 CACHE STRING "")
add_subdirectory("${OPEN62541_DIR}" "${CMAKE_BINARY_DIR}/open62541" EXCLUDE_FROM_ALL)

# ============================================================
# 项目源文件
# ============================================================
set(FRONTEND_ASSETS_SOURCE "${CMAKE_SOURCE_DIR}/generated/frontend_assets.cpp")
if(NOT EXISTS "${FRONTEND_ASSETS_SOURCE}")
    file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/generated")
    file(WRITE "${FRONTEND_ASSETS_SOURCE}"
        "#include \"frontend_assets.h\"\n\n"
        "bool embeddedFrontendAvailable() { return false; }\n"
        "const EmbeddedFrontendAsset* findEmbeddedFrontendAsset(const std::string&) { return nullptr; }\n"
    )
endif()

set(MONITOR_SOURCES
    src/main.cpp
    src/variable_manager.cpp
    src/database.cpp
    src/opcua_manager.cpp
    src/udp_manager.cpp
    src/communication_manager.cpp
    src/auth_manager.cpp
    src/http_server.cpp
    "${FRONTEND_ASSETS_SOURCE}"
)

set(MONITOR_INCLUDES
    include
    "${OPEN62541_DIR}/include"
    "${OPEN62541_DIR}/plugins/include"
    "${OPEN62541_DIR}/deps"
    "${CMAKE_BINARY_DIR}/open62541"
    "${CMAKE_BINARY_DIR}/open62541/src_generated"
)

# ============================================================
# 编译选项
# ============================================================
if(MSVC)
    # Visual Studio 编译器选项
    add_compile_options(
        /W3
        /utf-8
        /bigobj
        /MP          # 多进程编译
        /Oy-         # 禁用帧指针优化（便于调试）
        $<$<CONFIG:Debug>:/Od>
        $<$<CONFIG:Debug>:/Zi>
        $<$<CONFIG:Debug>:/DEBUG:FULL>
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/GL>
    )

    add_link_options(
        $<$<CONFIG:Debug>:/DEBUG:FULL>
        $<$<CONFIG:Release>:/LTCG>
    )
else()
    add_compile_options(-Wall -Wextra -O2)
endif()

# ============================================================
# 链接库
# ============================================================
set(LIBS open62541 ws2_32 crypt32 winhttp)
option(MONITOR_DISABLE_MYSQL "Build without MySQL support" OFF)
find_library(MYSQL_LIB
    NAMES libmysql mysqlclient
    PATHS
        "C:/Program Files/MySQL/MySQL Server 9.7/lib"
        "C:/Program Files/MySQL/MySQL Server 8.0/lib"
        "C:/Program Files/MySQL/MySQL Server 5.7/lib"
        "C:/ProgramData/Chocolatey/lib/mysql/lib"
        "C:/MySQL/lib"
        "C:/Program Files (x86)/MySQL/MySQL Connector C 6.1/lib"
        "$ENV{MYSQL_CONNECTOR_DIR}/lib"
)
if(MONITOR_DISABLE_MYSQL OR NOT MYSQL_LIB)
    message(STATUS "MySQL support disabled for this build.")
    add_compile_definitions(MONITOR_NO_MYSQL)
else()
    list(APPEND LIBS "${MYSQL_LIB}")
    message(STATUS "Found MySQL library: ${MYSQL_LIB}")
    find_path(MYSQL_INCLUDE_DIR
        NAMES mysql.h
        PATHS
            "C:/Program Files/MySQL/MySQL Server 9.7/include"
            "C:/Program Files/MySQL/MySQL Server 8.0/include"
            "C:/Program Files/MySQL/MySQL Server 5.7/include"
            "C:/MySQL/include"
            "C:/Program Files (x86)/MySQL/MySQL Connector C 6.1/include"
            "$ENV{MYSQL_CONNECTOR_DIR}/include"
    )
    if(MYSQL_INCLUDE_DIR)
        list(APPEND MONITOR_INCLUDES "${MYSQL_INCLUDE_DIR}")
        message(STATUS "Found MySQL include: ${MYSQL_INCLUDE_DIR}")
    endif()
endif()

# ============================================================
# 可执行文件
# ============================================================
add_executable(${PROJECT_NAME} ${MONITOR_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE ${MONITOR_INCLUDES})
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBS})

# ============================================================
# 生成 Visual Studio 解决方案文件
# ============================================================
# 在构建前运行 init.sql 的说明
add_custom_target(init_db COMMAND
    ${CMAKE_COMMAND} -E echo
    "Please execute backend/sql/init.sql in MySQL to initialize the database."
)

# ============================================================
# 安装规则
# ============================================================
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(FILES sql/init.sql DESTINATION sql)

message(STATUS "========================================")
message(STATUS "  Monitoring Platform Configuration")
message(STATUS "  open62541: ${OPEN62541_DIR}")
message(STATUS "  MySQL lib: ${MYSQL_LIB}")
message(STATUS "  MySQL inc: ${MYSQL_INCLUDE_DIR}")
message(STATUS "========================================")
