USE software_plc_monitoring;

DROP PROCEDURE IF EXISTS migrate_realtime_data_to_wide;

DELIMITER $$

CREATE PROCEDURE migrate_realtime_data_to_wide()
BEGIN
    DECLARE done INT DEFAULT 0;
    DECLARE long_columns INT DEFAULT 0;
    DECLARE col_name VARCHAR(128);
    DECLARE row_name VARCHAR(128);
    DECLARE row_value DOUBLE;
    DECLARE snapshot_time DATETIME(3);
    DECLARE tmp_table VARCHAR(96);
    DECLARE backup_table VARCHAR(96);

    DECLARE name_cur CURSOR FOR
        SELECT DISTINCT COALESCE(NULLIF(r.var_name, ''), NULLIF(v.name, ''), CONCAT('变量', r.var_id)) AS var_name
        FROM realtime_data r
        LEFT JOIN variable_definitions v ON v.id = r.var_id
        ORDER BY var_name;

    DECLARE row_cur CURSOR FOR
        SELECT
            COALESCE(NULLIF(r.var_name, ''), NULLIF(v.name, ''), CONCAT('变量', r.var_id)) AS var_name,
            r.value
        FROM realtime_data r
        LEFT JOIN variable_definitions v ON v.id = r.var_id
        ORDER BY r.id ASC;

    DECLARE CONTINUE HANDLER FOR NOT FOUND SET done = 1;

    SELECT COUNT(*)
    INTO long_columns
    FROM INFORMATION_SCHEMA.COLUMNS
    WHERE TABLE_SCHEMA = DATABASE()
      AND TABLE_NAME = 'realtime_data'
      AND COLUMN_NAME IN ('var_id', 'var_name', 'value', 'timestamp');

    IF long_columns = 4 THEN
        SELECT COALESCE(MAX(timestamp), NOW(3)) INTO snapshot_time FROM realtime_data;

        SET tmp_table = CONCAT('realtime_data_wide_tmp_', DATE_FORMAT(NOW(3), '%Y%m%d%H%i%s%f'));
        SET backup_table = CONCAT('realtime_data_long_backup_', DATE_FORMAT(NOW(3), '%Y%m%d%H%i%s%f'));

        SET @safe_tmp = REPLACE(tmp_table, '`', '``');
        SET @sql = CONCAT(
            'CREATE TABLE `', @safe_tmp, '` (',
            'id BIGINT AUTO_INCREMENT PRIMARY KEY,',
            '`time` DATETIME(3) NOT NULL,',
            'UNIQUE KEY uk_realtime_time (`time`),',
            'INDEX idx_realtime_time (`time`)',
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
            FETCH row_cur INTO row_name, row_value;
            IF done = 1 THEN
                LEAVE row_loop;
            END IF;

            SET @safe_col = REPLACE(row_name, '`', '``');
            SET @snapshot_time = snapshot_time;
            SET @row_value = row_value;
            SET @sql = CONCAT(
                'INSERT INTO `', @safe_tmp, '` (id, `time`, `', @safe_col, '`) VALUES (1, ?, ?) ',
                'ON DUPLICATE KEY UPDATE `time` = VALUES(`time`), `', @safe_col, '` = VALUES(`', @safe_col, '`)'
            );
            PREPARE stmt FROM @sql;
            EXECUTE stmt USING @snapshot_time, @row_value;
            DEALLOCATE PREPARE stmt;
        END LOOP;
        CLOSE row_cur;

        SET @safe_backup = REPLACE(backup_table, '`', '``');
        SET @sql = CONCAT(
            'RENAME TABLE realtime_data TO `', @safe_backup, '`, ',
            '`', @safe_tmp, '` TO realtime_data'
        );
        PREPARE stmt FROM @sql;
        EXECUTE stmt;
        DEALLOCATE PREPARE stmt;
    END IF;
END$$

DELIMITER ;

CALL migrate_realtime_data_to_wide();

DROP PROCEDURE IF EXISTS migrate_realtime_data_to_wide;
