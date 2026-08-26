


# MonitoringPlatform — 西门子 WinCC 风格监控平台

> 一个基于 C++ 后端 + Vue 3 前端的工业监控平台，架构参照西门子 WinCC (SCADA HMI)。
> 支持 **OPC UA** (open62541 v1.0) 与 **UDP** 两种通信方式，**MySQL** 持久化存储，Web 实时曲线图表。

---

## 系统架构图

```
┌─────────────────────────────────────────────────────────────┐
│                        前端 (Vue 3)                         │
│  ┌─────────┐ ┌────────────┐ ┌──────────┐ ┌─────────────┐  │
│  │Dashboard│ │  TrendView │ │ DataView │ │ SettingsView │  │
│  └────┬────┘ └─────┬──────┘ └────┬─────┘ └──────┬──────┘  │
│       └────────────┼─────────────┼───────────────┘          │
│               ┌────┴────┐                                   │
│               │  Pinia  │ 状态管理 + Axios API + WebSocket  │
│               └─────────┘                                   │
└──────────────────────────┬──────────────────────────────────┘
                  │ HTTP REST + WebSocket (ws://)
                  │
┌──────────────────────────┴──────────────────────────────────┐
│                  后端 (C++17 / Visual Studio 2026)           │
│  ┌──────────────┐ ┌─────────┐ ┌──────────────────────────┐  │
│  │  HttpServer   │ │UDPMgr  │ │     OPCUAManager         │  │
│  │  (8080)       │ │(8888)  │ │  Server(4840)+Client     │  │
│  │ REST+WebSocket│ │        │ │  +Polling                │  │
│  └──────┬───────┘ └────┬────┘ └───────────┬──────────────┘  │
│         └──────┬───────┴───────────────────┘                 │
│                ▼                                              │
│         ┌──────────────┐                                     │
│         │VariableManager│ (环形缓冲区 10K/变量)              │
│         └──────┬───────┘                                     │
│                ▼                                              │
│         ┌──────────────┐                                     │
│         │DatabaseMgr   │ (MySQL Connector/C)                │
│         └──────────────┘                                     │
└──────────────────────────┬──────────────────────────────────┘
                  │ TCP 3306
                  ▼
              ┌──────────────┐
              │    MySQL     │
              │ 6 张业务表   │
              └──────────────┘
```

---

## 目录结构

```
Monitoring/
├── backend/                     # C++ 后端
│   ├── CMakeLists.txt           # VS2026 CMake 构建
│   ├── sql/
│   │   └── init.sql             # MySQL 建库建表脚本
│   ├── include/
│   │   ├── common.h             # 公共类型、工具函数
│   │   ├── variable_manager.h   # 变量管理器
│   │   ├── database.h           # MySQL 数据库封装
│   │   ├── opcua_manager.h      # OPC UA 服务器+客户端
│   │   ├── udp_manager.h        # UDP 通信
│   │   └── http_server.h        # HTTP REST + WebSocket
│   └── src/
│       ├── main.cpp             # 入口、服务编排
│       ├── variable_manager.cpp
│       ├── database.cpp
│       ├── opcua_manager.cpp
│       ├── udp_manager.cpp
│       └── http_server.cpp
├── frontend/                    # Vue 3 前端
│   ├── package.json
│   ├── vite.config.js
│   ├── index.html
│   └── src/
│       ├── main.js
│       ├── App.vue
│       ├── assets/styles.css    # 全局暗色主题
│       ├── router/index.js
│       ├── store/index.js       # Pinia + WebSocket
│       ├── api/index.js         # Axios API 客户端
│       ├── components/
│       │   ├── MenuBar.vue       # 导航菜单
│       │   ├── DynamicChart.vue  # Canvas2D 实时曲线
│       │   ├── VariablePanel.vue # 变量卡片面板
│       │   └── AlarmPanel.vue    # 告警面板
│       └── views/
│           ├── Dashboard.vue     # 仪表盘首页
│           ├── TrendView.vue     # 趋势分析
│           ├── DataView.vue      # 数据管理
│           └── SettingsView.vue  # 系统设置
├── open62541-1.0/               # OPC UA 开源库 (源码)
└── README.md
```

---

## 数据库 (MySQL)

| 表名 | 用途 |
|------|------|
| `variable_definitions` | 变量定义（名称、类型、数据源、量程、颜色等） |
| `realtime_data` | 实时快照（最新值、质量戳、时间戳） |
| `historical_data` | 历史数据（时间序列） |
| `alarm_records` | 告警记录（类型、级别、确认状态） |
| `operation_log` | 操作日志（谁、什么时间、改了哪个变量） |
| `user_config` | 用户配置键值表 |

---

## 构建与运行

### 1. 安装前端依赖

```bash
cd frontend
npm install
```

### 2. 启动后端（Windows，推荐）

当前仓库中已经提供了可直接运行的打包运行时，最省事的方式是：

```bash
cd release/MonitoringRuntime
start_runtime.bat
```

或者直接运行：

```bash
cd release/MonitoringRuntime
MonitoringRuntime.exe
```

启动后默认监听：
- HTTP REST API + WebSocket → `http://127.0.0.1:8081`
- OPC UA Server → `opc.tcp://localhost:4841`
- UDP Receiver → 由前端通信配置页控制

> 如果没有配置 MySQL，后端会以“有限功能模式”运行，基础接口仍可使用，但数据库持久化功能不可用。

### 3. 启动前端

```bash
cd frontend
npm run dev -- --host 0.0.0.0 --port 5170
```

浏览器打开：
- `http://localhost:5170/`

前端会自动代理 `/api` 和 `/ws` 请求到后端的 `8081` 端口。

### 4. 可选：初始化数据库

如果你希望启用完整的数据库持久化功能，可以执行：

```bash
mysql -u root -p < backend/sql/init.sql
```

### 5. 可选：从源码重新构建后端

如果你需要重新编译后端，可在 Windows 环境下执行：

```bash
cd backend
cmake -B build-runtime -G "Visual Studio 17 2026" -A x64
cmake --build build-runtime --config Release
```

然后运行生成的可执行文件：

```bash
backend/build-runtime/Release/MonitoringPlatform.exe
```

### 6. 跨机器使用说明（更智能、可移植）

如果你要把前后端部署到另一台机器上，建议按下面思路操作：

1. 先确认目标机器上已安装 Node.js 和 Windows 运行环境。
2. 将整个项目目录复制到目标机器，至少保留以下内容：
   - `frontend/`
   - `release/MonitoringRuntime/`
   - `backend/sql/`（如需初始化数据库）
3. 在目标机器上先启动后端：

```bash
cd release/MonitoringRuntime
start_runtime.bat
```

4. 再启动前端：

```bash
cd frontend
npm install
npm run dev -- --host 0.0.0.0 --port 5170
```

5. 如果需要从其他设备访问，确保防火墙放行以下端口：
   - `8081`：后端 HTTP / WebSocket
   - `5170`：前端开发页面
   - `4841`：OPC UA 服务（如需要访问 OPC UA）

6. 访问地址可以使用：
   - 本机：`http://localhost:5170/`
   - 局域网：`http://<目标机器 IP>:5170/`

> 这套方式的好处是：不依赖当前机器的特定路径，也不需要先构建出某个固定的 Visual Studio 工程；只要目标机器满足基本依赖，就可以直接启动。

---

## 默认端口汇总

| 端口 | 用途 | 协议 |
|------|------|------|
| 8081 | REST API + WebSocket | HTTP / WS |
| 4841 | OPC UA 服务器 | OPC UA TCP |
| 5170 | Vue 前端调试 | HTTP (Vite) |

---

## 功能特性

- **实时仪表盘**：变量卡片、实时曲线、告警列表、操作日志
- **趋势分析**：多变量叠加曲线、时间范围选择、历史数据表
- **数据管理**：实时值表格、告警查询、操作日志查看
- **系统设置**：通信参数、数据库配置、显示偏好
- **OPC UA**：内嵌服务器，可连接远程 OPC UA 服务端进行读写轮询
- **UDP 通信**：接收 `varName=value` 格式的广播报文，自动更新变量
- **WebSocket 推送**：500ms 间隔广播实时数据到所有 Web 客户端
- **数据持久化**：周期性刷新实时数据与历史数据到 MySQL

---

## 技术栈

| 层 | 技术 | 说明 |
|----|------|------|
| 前端 | Vue 3 + Vite 5 + Pinia + Vue Router 4 | 组合式 API |
| 图表 | Canvas2D 自研引擎 | 20fps 渲染，梯度填充，自动缩放 |
| 后端 | C++17 (MSVC) | 原生 Windows Socket + CMake |
| OPC UA | open62541 v1.0 | 服务器 + 客户端 + 轮询 |
| 数据库 | MySQL + Connector/C | 6 张业务表 |

---

## 开发说明

- 后端使用 **CMake** 生成 Visual Studio 2026 解决方案。
- open62541 v1.0 以源码方式集成（`add_subdirectory`），编译为静态库。
- MySQL Connector/C 自动搜索常见安装路径；若未找到，编译可继续但数据库功能运行时不可用。
- 前端使用 **Vite 代理** 解决开发跨域问题，生产部署可将 Vue 构建产物放到后端 HTTP 服务器静态目录。
- 代码中预设了 `SIMULATED` 数据源变量用于演示，无需连接真实设备即可看到动态曲线。
│  OPCUAManager (open62541 服务端 + 客户端 + 轮询)             │
│  UDPManager (UDP 接收/发送)                                  │
└──────┬──────────────────────┬──────────────────┬────────────┘
       │                      │                  │
       ▼                      ▼                  ▼
    MySQL DB            OPC UA 设备          UDP 设备
```

## 技术栈

| 层       | 技术                                      |
| -------- | ----------------------------------------- |
| 前端     | Vue 3.4, Vite 5, Pinia 2.1, Vue Router 4 |
| 后端     | C++17 (MSVC), CMake 3.20+                |
| 通信     | OPC UA (open62541 v1.0), UDP, WebSocket  |
| 数据库   | MySQL 8.0+ / MariaDB 10.5+               |
| 编译器   | Visual Studio 2026 (MSVC v143+)          |

## 项目结构

```
Monitoring/
├── backend/
│   ├── CMakeLists.txt          # VS2026 构建配置
│   ├── sql/init.sql            # MySQL 数据库初始化脚本
│   ├── include/
│   │   ├── common.h            # 公共类型定义、工具函数
│   │   ├── variable_manager.h  # 变量管理器
│   │   ├── database.h          # 数据库接口
│   │   ├── opcua_manager.h     # OPC UA 管理器
│   │   ├── udp_manager.h       # UDP 通信管理器
│   │   └── http_server.h       # HTTP + WebSocket 服务器
│   └── src/
│       ├── main.cpp            # 入口、服务编排
│       ├── variable_manager.cpp
│       ├── database.cpp
│       ├── opcua_manager.cpp
│       ├── udp_manager.cpp
│       └── http_server.cpp
├── frontend/
│   ├── package.json
│   ├── vite.config.js
│   ├── index.html
│   └── src/
│       ├── main.js
│       ├── App.vue
│       ├── router/index.js
│       ├── store/index.js
│       ├── api/index.js
│       ├── assets/styles.css
│       ├── components/
│       │   ├── MenuBar.vue
│       │   ├── DynamicChart.vue
│       │   ├── VariablePanel.vue
│       │   └── AlarmPanel.vue
│       └── views/
│           ├── Dashboard.vue
│           ├── TrendView.vue
│           ├── DataView.vue
│           └── SettingsView.vue
└── README.md
```

## 快速开始

### 1. 环境要求

- **Visual Studio 2026** 或更高版本（含 C++ 桌面开发工作负载）
- **CMake** 3.20+
- **MySQL** 8.0+ 或 MariaDB 10.5+
- **Node.js** 18+ (推荐 20 LTS)
- **open62541 v1.0**（已集成至项目中）

### 2. 数据库初始化

```sql
-- 在 MySQL 中执行
mysql -u root -p < backend/sql/init.sql
```

或者登录 MySQL 后执行：

```sql
SOURCE C:/Users/NEU/Desktop/Monitoring/backend/sql/init.sql;
```

这将创建 `monitoring_platform` 数据库及所有表（variable_definitions, realtime_data, historical_data, alarm_records, operation_log, user_config）。

### 3. 构建后端

```bash
cd backend
cmake -B build -G "Visual Studio 17 2026" -A x64
cmake --build build --config Release
```

或者直接用 Visual Studio 打开 `build/MonitoringPlatform.sln`。

> **注意**：CMakeLists.txt 会自动检测 open62541 的 amalgamated 文件（`open62541.c/h`），如果找不到则尝试从源码编译。也会自动搜索 MySQL Connector/C 的安装路径（通过 `MYSQL_CONNECTOR_DIR` 环境变量或注册表）。

### 4. 配置环境变量（可选）

| 变量名                | 默认值                      | 说明                      |
| --------------------- | --------------------------- | ------------------------- |
| `MYSQL_HOST`          | `127.0.0.1`                 | MySQL 主机地址            |
| `MYSQL_PORT`          | `3306`                      | MySQL 端口                |
| `MYSQL_USER`          | `root`                      | 数据库用户                |
| `MYSQL_PASS`          | (空)                        | 数据库密码                |
| `MYSQL_DB`            | `monitoring_platform`       | 数据库名                  |
| `OPCUA_SERVER_PORT`   | `4840`                      | OPC UA 服务器端口         |
| `UDP_RECEIVE_PORT`    | `8888`                      | UDP 接收端口              |
| `HTTP_PORT`           | `8080`                      | HTTP REST 服务端口        |
| `MONITOR_DATA_DIR`    | `./data`                    | 通信资源等本地持久化数据目录；生产环境建议配置为绝对路径 |
| `MONITOR_AUTH_FILE`    | `auth_users.tsv`           | 本地账号库路径；可配置为绝对路径，必须随生产部署一并备份 |

### 5. 运行后端

```bash
cd build/Release
./MonitoringPlatform.exe
```

后端将启动：
- **HTTP REST API** → http://localhost:8080
- **WebSocket 实时推送** → ws://localhost:8080/ws/realtime
- **OPC UA Server** → opc.tcp://localhost:4840
- **UDP Receiver** → 端口 8888

### 6. 安装前端依赖

```bash
cd frontend
npm install
```

### 7. 运行前端开发服务器

```bash
npm run dev
```

前端开发服务器将运行在 **http://localhost:3000**，API 请求自动代理到后端 8080 端口。

### 8. 打开浏览器

访问 **http://localhost:3000** 即可进入监控平台界面。

## API 接口

| 方法   | 路径                      | 说明                      |
| ------ | ------------------------- | ------------------------- |
| GET    | /api/variables            | 获取所有变量定义          |
| POST   | /api/variables            | 新增变量定义              |
| PUT    | /api/variables/:id        | 修改变量定义              |
| DELETE | /api/variables/:id        | 删除变量定义              |
| GET    | /api/realtime             | 获取所有实时数据          |
| GET    | /api/history/:id          | 查询历史数据（支持时间范围）|
| POST   | /api/write/:id            | 写入变量值                |
| GET    | /api/alarms               | 获取报警记录              |
| POST   | /api/alarms/:id/ack       | 确认报警                  |
| GET    | /api/logs                 | 获取操作日志              |
| GET    | /api/config/:key          | 获取配置项                |
| POST   | /api/config/:key          | 设置配置项                |
| GET    | /api/status               | 服务状态                  |

## WebSocket 协议

- **端点**: `ws://localhost:8080/ws/realtime`
- **消息格式**: JSON 字符串
- **推送内容**: 所有变量当前值（每 500ms 广播一次）

## OPC UA 地址空间

服务器在 `opc.tcp://localhost:4840` 上运行，所有变量定义自动创建为 OPC UA 变量节点，节点 ID 格式为 `ns=1;i=<变量ID>`。

## 功能特性

- ✅ 实时数据仪表盘（动态 Canvas2D 曲线）
- ✅ 趋势分析（多变量对比、历史回溯）
- ✅ 变量管理（增删改查、写值）
- ✅ 报警管理（多级报警、确认）
- ✅ 操作日志（完整审计追踪）
- ✅ 系统配置（通信、数据库、显示设置）
- ✅ OPC UA 服务器 + 客户端
- ✅ UDP 设备数据接入
- ✅ WebSocket 实时推送
- ✅ 深色 WinCC 风格主题
- ✅ 数据持久化（MySQL）
