-- ============================================================
-- 西门子 WinCC 风格监控平台 - 数据库初始化脚本
-- 数据库名称: software_plc_monitoring
-- ============================================================

CREATE DATABASE IF NOT EXISTS software_plc_monitoring
    DEFAULT CHARACTER SET utf8mb4
    DEFAULT COLLATE utf8mb4_unicode_ci;

USE software_plc_monitoring;

-- -------------------------------------------
-- 变量定义表：定义需要监控的所有变量
-- -------------------------------------------
CREATE TABLE IF NOT EXISTS variable_definitions (
    id              INT AUTO_INCREMENT PRIMARY KEY,
    name            VARCHAR(128)    NOT NULL UNIQUE COMMENT '变量名称（标记名）',
    description     VARCHAR(256)    DEFAULT '' COMMENT '变量描述',
    data_type       VARCHAR(32)     NOT NULL DEFAULT 'FLOAT' COMMENT '数据类型: FLOAT, INT, BOOL, STRING',
    source          VARCHAR(32)     NOT NULL DEFAULT 'OPCUA' COMMENT '数据来源: OPCUA, UDP, SIMULATED',
    opcua_node_id   VARCHAR(256)    DEFAULT '' COMMENT 'OPC UA 节点ID (如 ns=1;s=temperature)',
    udp_port        INT             DEFAULT 0 COMMENT 'UDP 端口（仅UDP来源时有效）',
    min_value       DOUBLE          DEFAULT NULL COMMENT '量程最小值',
    max_value       DOUBLE          DEFAULT NULL COMMENT '量程最大值',
    unit            VARCHAR(32)     DEFAULT '' COMMENT '工程单位',
    color           VARCHAR(16)     DEFAULT '#2196F3' COMMENT '曲线颜色',
    enabled         TINYINT(1)      DEFAULT 1 COMMENT '是否启用',
    created_at      DATETIME        DEFAULT CURRENT_TIMESTAMP,
    updated_at      DATETIME        DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_source (source),
    INDEX idx_enabled (enabled)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- -------------------------------------------
-- 实时数据表：最新变量值宽表（快照表）
-- 基础列固定为 id、time；变量列由后端按变量名动态添加。
-- -------------------------------------------
CREATE TABLE IF NOT EXISTS realtime_data (
    id              BIGINT AUTO_INCREMENT PRIMARY KEY,
    `time`          DATETIME(3)     NOT NULL COMMENT '最新采集时间戳',
    UNIQUE KEY uk_realtime_time (`time`),
    INDEX idx_realtime_time (`time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- -------------------------------------------
-- 历史数据表：宽表/交叉表
-- 基础列固定为 id、time；变量列由后端按变量名动态添加。
-- -------------------------------------------
CREATE TABLE IF NOT EXISTS historical_data (
    id              BIGINT AUTO_INCREMENT PRIMARY KEY,
    `time`          DATETIME(3)     NOT NULL COMMENT '采集时间戳',
    UNIQUE KEY uk_history_time (`time`),
    INDEX idx_history_time (`time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- -------------------------------------------
-- 报警记录表
-- -------------------------------------------
CREATE TABLE IF NOT EXISTS alarm_records (
    id              BIGINT AUTO_INCREMENT PRIMARY KEY,
    var_id          INT             NOT NULL COMMENT '关联变量ID',
    var_name        VARCHAR(128)    NOT NULL COMMENT '变量名',
    alarm_type      VARCHAR(32)     NOT NULL COMMENT '报警类型: HIGH, LOW, HH, LL, CHANGE',
    alarm_value     DOUBLE          NOT NULL COMMENT '触发值',
    limit_value     DOUBLE          NOT NULL COMMENT '限值',
    severity        INT             DEFAULT 1 COMMENT '严重级别 1-5',
    message         VARCHAR(256)    DEFAULT '' COMMENT '报警信息',
    acknowledged    TINYINT(1)      DEFAULT 0 COMMENT '是否确认',
    occurred_at     DATETIME(3)     NOT NULL COMMENT '发生时间',
    acknowledged_at DATETIME(3)     DEFAULT NULL COMMENT '确认时间',
    INDEX idx_var_alarm (var_id, occurred_at),
    INDEX idx_ack (acknowledged)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- -------------------------------------------
-- 操作记录表（用户操作日志）
-- -------------------------------------------
CREATE TABLE IF NOT EXISTS operation_log (
    id              BIGINT AUTO_INCREMENT PRIMARY KEY,
    var_id          INT             DEFAULT NULL COMMENT '关联变量ID',
    operation_type  VARCHAR(32)     NOT NULL COMMENT '操作类型: SET_VALUE, WRITE_DB, ALARM_ACK',
    var_name        VARCHAR(128)    DEFAULT '' COMMENT '变量名',
    old_value       DOUBLE          DEFAULT NULL COMMENT '旧值',
    new_value       DOUBLE          DEFAULT NULL COMMENT '新值',
    operator        VARCHAR(64)     DEFAULT 'SYSTEM' COMMENT '操作人',
    detail          VARCHAR(512)    DEFAULT '' COMMENT '详情',
    operated_at     DATETIME(3)     DEFAULT CURRENT_TIMESTAMP(3) COMMENT '操作时间',
    INDEX idx_type (operation_type),
    INDEX idx_time (operated_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- -------------------------------------------
-- 下发记录表：物理宽表，变量列按下发变量名动态追加
-- -------------------------------------------
CREATE TABLE IF NOT EXISTS write_records (
    id              BIGINT AUTO_INCREMENT PRIMARY KEY,
    `time`          DATETIME(3)     NOT NULL COMMENT '下发时间',
    UNIQUE KEY uk_write_time (`time`),
    INDEX idx_write_time (`time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- -------------------------------------------
-- 用户配置表
-- -------------------------------------------
CREATE TABLE IF NOT EXISTS user_config (
    id              INT AUTO_INCREMENT PRIMARY KEY,
    `key`           VARCHAR(128)    NOT NULL UNIQUE COMMENT '配置键',
    `value`         TEXT            COMMENT '配置值',
    description     VARCHAR(256)    DEFAULT '' COMMENT '说明',
    updated_at      DATETIME        DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- -------------------------------------------
-- 插入默认变量（示例）
-- -------------------------------------------
INSERT INTO variable_definitions (name, description, data_type, source, opcua_node_id, min_value, max_value, unit, color) VALUES
    ('温度_1',     '1号反应器温度',         'FLOAT',  'OPCUA', 'ns=1;s=temperature_1',   0,   200, '℃',   '#FF5722'),
    ('压力_1',     '1号反应器压力',         'FLOAT',  'OPCUA', 'ns=1;s=pressure_1',      0,   10,  'MPa',  '#2196F3'),
    ('液位_1',     '1号反应器液位',         'FLOAT',  'OPCUA', 'ns=1;s=level_1',         0,   100, '%',    '#4CAF50'),
    ('流量_1',     '1号管道流量',           'FLOAT',  'UDP',   '',                       0,   500, 'm³/h', '#FF9800'),
    ('电机转速_1', '1号电机转速',           'INT',    'OPCUA', 'ns=1;s=motor_speed_1',   0,   3000,'RPM',  '#9C27B0'),
    ('温度_2',     '2号反应器温度',         'FLOAT',  'OPCUA', 'ns=1;s=temperature_2',   0,   200, '℃',   '#E91E63'),
    ('压力_2',     '2号反应器压力',         'FLOAT',  'OPCUA', 'ns=1;s=pressure_2',      0,   10,  'MPa',  '#00BCD4'),
    ('液位_2',     '2号反应器液位',         'FLOAT',  'UDP',   '',                       0,   100, '%',    '#8BC34A'),
    ('阀门开度_1', '1号调节阀开度',         'INT',    'OPCUA', 'ns=1;s=valve_1',         0,   100, '%',    '#FFC107'),
    ('振动_1',     '1号压缩机振动',         'FLOAT',  'OPCUA', 'ns=1;s=vibration_1',     0,   50,  'mm/s', '#795548')
ON DUPLICATE KEY UPDATE description = VALUES(description);

-- 插入默认配置
INSERT INTO user_config (`key`, `value`, description) VALUES
    ('opcua_server_url', 'opc.tcp://localhost:4840', 'OPC UA 服务器地址'),
    ('udp_listen_port', '8888', 'UDP 监听端口'),
    ('data_poll_interval_ms', '1000', '数据轮询间隔(毫秒)'),
    ('history_retention_days', '30', '历史数据保留天数'),
    ('chart_refresh_interval_ms', '500', '图表刷新间隔(毫秒)')
ON DUPLICATE KEY UPDATE description = VALUES(description);

-- 创建存储过程：归档历史数据
DELIMITER //
CREATE PROCEDURE IF NOT EXISTS archive_old_data(IN retention_days INT)
BEGIN
    DELETE FROM historical_data
    WHERE `time` < DATE_SUB(NOW(), INTERVAL retention_days DAY);
END//
DELIMITER ;

-- 创建事件调度器（每天自动清理过期历史数据）
DELIMITER //
CREATE EVENT IF NOT EXISTS auto_archive_event
ON SCHEDULE EVERY 1 DAY
STARTS CURRENT_TIMESTAMP
DO
BEGIN
    CALL archive_old_data(30);
END//
DELIMITER ;

SET GLOBAL event_scheduler = ON;
