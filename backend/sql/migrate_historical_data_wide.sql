USE software_plc_monitoring;

DROP PROCEDURE IF EXISTS migrate_historical_data_to_wide;

DELIMITER $$

CREATE PROCEDURE migrate_historical_data_to_wide()
BEGIN
    DECLARE done INT DEFAULT 0;
    DECLARE long_columns INT DEFAULT 0;
    DECLARE col_name VARCHAR(128);
    DECLARE row_name VARCHAR(128);
    DECLARE row_value DOUBLE;
    DECLARE row_time DATETIME(3);
    DECLARE tmp_table VARCHAR(96);
    DECLARE backup_table VARCHAR(96);

    DECLARE name_cur CURSOR FOR
        SELECT DISTINCT COALESCE(NULLIF(h.var_name, ''), NULLIF(v.name, ''), NULLIF(r.var_name, ''), CONCAT('变量', h.var_id)) AS var_name
        FROM historical_data h
        LEFT JOIN variable_definitions v ON v.id = h.var_id
        LEFT JOIN realtime_data r ON r.var_id = h.var_id
        ORDER BY var_name;

    DECLARE row_cur CURSOR FOR
        SELECT
            COALESCE(NULLIF(h.var_name, ''), NULLIF(v.name, ''), NULLIF(r.var_name, ''), CONCAT('变量', h.var_id)) AS var_name,
            h.value,
            h.timestamp
        FROM historical_data h
        LEFT JOIN variable_definitions v ON v.id = h.var_id
        LEFT JOIN realtime_data r ON r.var_id = h.var_id
        ORDER BY h.timestamp ASC, h.id ASC;

    DECLARE CONTINUE HANDLER FOR NOT FOUND SET done = 1;

    SELECT COUNT(*)
    INTO long_columns
    FROM INFORMATION_SCHEMA.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'historical_data'
      AND COLUMN_NAME IN ('var_id', 'var_name', 'value', 'timestamp');

    IF long_columns = 4 THEN
        SET tmp_table = CONCAT('historical_data_wide_tmp_', DATE_FORMAT(NOW(3), '%Y%m%d%H%i%s%f'));
        SET backup_table = CONCAT('historical_data_long_backup_', DATE_FORMAT(NOW(3), '%Y%m%d%H%i%s%f'));

        SET @safe_tmp = REPLACE(tmp_table, '`', '``');
        SET @sql = CONCAT(
            'CREATE TABLE `', @safe_tmp, '` (',
            'id BIGINT AUTO_INCREMENT PRIMARY KEY,',
            '`time` DATETIME(3) NOT NULL,',
            'UNIQUE KEY uk_history_time (`time`),',
            'INDEX idx_history_time (`time`)',
            ') ENGINE=InnoDB DEFAULT CHARSET=utf8mb4'
        );
        PREPARE stmt FROM @sql;
        EXECUTE stmt;
        DEALLOCATE PREPARE stmt;

        SET done = 0;
        OPEN name_cur;
        name_loop: LOOP
            FETCH name_cur INTO col_name;
            IF done = 1 THEN
                LEAVE name_loop;
            END IF;

            SET @safe_col = REPLACE(col_name, '`', '``');
            SET @sql = CONCAT('ALTER TABLE `', @safe_tmp, '` ADD COLUMN `', @safe_col, '` DOUBLE NULL');
            PREPARE stmt FROM @sql;
            EXECUTE stmt;
            DEALLOCATE PREPARE stmt;
        END LOOP;
        CLOSE name_cur;

        SET done = 0;
        OPEN row_cur;
        row_loop: LOOP
            FETCH row_cur INTO row_name, row_value, row_time;
            IF done = 1 THEN
                LEAVE row_loop;
            END IF;

            SET @safe_col = REPLACE(row_name, '`', '``');
            SET @row_time = row_time;
            SET @row_value = row_value;
            SET @sql = CONCAT(
                'INSERT INTO `', @safe_tmp, '` (`time`, `', @safe_col, '`) VALUES (?, ?) ',
                'ON DUPLICATE KEY UPDATE `', @safe_col, '` = VALUES(`', @safe_col, '`)'
            );
            PREPARE stmt FROM @sql;
            EXECUTE stmt USING @row_time, @row_value;
            DEALLOCATE PREPARE stmt;
        END LOOP;
        CLOSE row_cur;

        SET @safe_backup = REPLACE(backup_table, '`', '``');
        SET @sql = CONCAT(
            'RENAME TABLE historical_data TO `', @safe_backup, '`, ',
            '`', @safe_tmp, '` TO historical_data'
        );
        PREPARE stmt FROM @sql;
        EXECUTE stmt;
        DEALLOCATE PREPARE stmt;
    END IF;
END$$

DELIMITER ;

CALL migrate_historical_data_to_wide();

DROP PROCEDURE IF EXISTS migrate_historical_data_to_wide;
