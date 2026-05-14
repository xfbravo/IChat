-- ============================================
-- IM 即时通讯系统数据库初始化脚本
-- 执行方式: mysql -u root -p < init_database.sql
-- ============================================

-- 创建数据库
DROP DATABASE IF EXISTS `im_server`;
CREATE DATABASE `im_server`
    DEFAULT CHARACTER SET utf8mb4
    DEFAULT COLLATE utf8mb4_unicode_ci;

USE `im_server`;

-- ============================================
-- 1. 用户模块
-- ============================================

CREATE TABLE `im_user` (
    `id`                BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '自增ID',
    `user_id`           VARCHAR(32) NOT NULL COMMENT '用户ID（唯一）',
    `phone`             VARCHAR(20) DEFAULT NULL COMMENT '手机号（唯一）',
    `email`             VARCHAR(128) DEFAULT NULL COMMENT '邮箱（唯一）',
    `nickname`          VARCHAR(64) NOT NULL DEFAULT '' COMMENT '昵称',
    `avatar_url`        MEDIUMTEXT COMMENT '头像URL或data URL',
    `gender`            VARCHAR(8) NOT NULL DEFAULT '' COMMENT '性别',
    `region`            VARCHAR(128) NOT NULL DEFAULT '' COMMENT '地区',
    `signature`         VARCHAR(255) NOT NULL DEFAULT '' COMMENT '个性签名',
    `password_hash`     VARCHAR(128) NOT NULL COMMENT '密码哈希',
    `salt`              VARCHAR(32) NOT NULL COMMENT '盐值',
    `status`            TINYINT NOT NULL DEFAULT 1 COMMENT '状态：0禁用 1正常 2被举报',
    `user_type`         TINYINT NOT NULL DEFAULT 1 COMMENT '类型：1普通用户 2官方账号 3机器人',
    `last_login_time`   DATETIME DEFAULT NULL COMMENT '最后登录时间',
    `last_login_ip`     VARCHAR(45) DEFAULT NULL COMMENT '最后登录IP',
    `create_time`       DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    `update_time`       DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
    `delete_time`       DATETIME DEFAULT NULL COMMENT '删除时间（软删除）',

    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_user_id` (`user_id`),
    UNIQUE KEY `uk_phone` (`phone`),
    UNIQUE KEY `uk_email` (`email`),
    KEY `idx_status` (`status`),
    KEY `idx_create_time` (`create_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='用户表';

CREATE TABLE `im_user_device` (
    `id`                BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '自增ID',
    `user_id`           VARCHAR(32) NOT NULL COMMENT '用户ID',
    `device_id`         VARCHAR(64) NOT NULL COMMENT '设备ID',
    `device_type`       VARCHAR(16) NOT NULL COMMENT '设备类型：ios/android/windows/mac/web',
    `device_name`       VARCHAR(128) DEFAULT '' COMMENT '设备名称',
    `os_version`        VARCHAR(32) DEFAULT '' COMMENT '系统版本',
    `app_version`       VARCHAR(16) DEFAULT '' COMMENT 'APP版本',
    `push_token`        VARCHAR(256) DEFAULT '' COMMENT '推送Token',
    `login_time`        DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '登录时间',
    `logout_time`       DATETIME DEFAULT NULL COMMENT '登出时间',
    `is_active`         TINYINT NOT NULL DEFAULT 1 COMMENT '是否在线：0否 1是',
    `create_time`       DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',

    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_user_device` (`user_id`, `device_id`),
    UNIQUE KEY `uk_device_id` (`device_id`),
    KEY `idx_user_id` (`user_id`),
    KEY `idx_is_active` (`is_active`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='用户设备表';

CREATE TABLE `im_user_session` (
    `id`                BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '自增ID',
    `user_id`           VARCHAR(32) NOT NULL COMMENT '用户ID',
    `session_type`      TINYINT NOT NULL COMMENT '会话类型：1单聊 2群聊',
    `peer_id`           VARCHAR(32) NOT NULL COMMENT '对方ID（单聊为用户ID，群聊为群ID）',
    `session_name`      VARCHAR(128) DEFAULT '' COMMENT '会话名称（群名或备注名）',
    `session_avatar`    VARCHAR(512) DEFAULT '' COMMENT '会话头像URL',
    `unread_count`      INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '未读消息数',
    `last_msg_id`       BIGINT UNSIGNED DEFAULT NULL COMMENT '最后消息ID',
    `last_msg_time`     DATETIME DEFAULT NULL COMMENT '最后消息时间',
    `last_msg_type`     VARCHAR(32) DEFAULT '' COMMENT '最后消息类型',
    `last_msg_content`  VARCHAR(512) DEFAULT '' COMMENT '最后消息内容（摘要）',
    `is_disturb`        TINYINT NOT NULL DEFAULT 0 COMMENT '是否免打扰：0否 1是',
    `is_top`            TINYINT NOT NULL DEFAULT 0 COMMENT '是否置顶：0否 1是',
    `update_time`       DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
    `create_time`       DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',

    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_user_session` (`user_id`, `session_type`, `peer_id`),
    KEY `idx_user_id` (`user_id`),
    KEY `idx_last_msg_time` (`user_id`, `last_msg_time`),
    KEY `idx_unread` (`user_id`, `unread_count`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='用户会话表';

CREATE TABLE `im_user_stats` (
    `id`                BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '自增ID',
    `user_id`           VARCHAR(32) NOT NULL COMMENT '用户ID',
    `total_friend_count`    INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '好友数量',
    `total_group_count`     INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '加入群组数量',
    `total_message_count`   BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '发送消息总数',
    `total_file_count`      INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '发送文件数',
    `total_file_size`       BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '发送文件总大小（字节）',
    `update_time`       DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',

    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_user_id` (`user_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='用户统计表';

-- ============================================
-- 2. 好友关系模块
-- ============================================

CREATE TABLE `im_friend` (
    `id`                BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '自增ID',
    `user_id`          VARCHAR(32) NOT NULL COMMENT '用户ID',
    `friend_id`        VARCHAR(32) NOT NULL COMMENT '好友ID',
    `remark`           VARCHAR(128) DEFAULT '' COMMENT '好友备注',
    `friend_nickname`  VARCHAR(64) DEFAULT '' COMMENT '好友昵称',
    `friend_avatar`    MEDIUMTEXT COMMENT '好友头像',
    `friend_type`      TINYINT NOT NULL DEFAULT 1 COMMENT '好友类型：1正常好友',
    `status`           TINYINT NOT NULL DEFAULT 1 COMMENT '状态：0删除 1正常',
    `create_time`      DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '添加时间',
    `update_time`      DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',

    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_friend_relation` (`user_id`, `friend_id`),
    KEY `idx_user_id` (`user_id`),
    KEY `idx_friend_id` (`friend_id`),
    KEY `idx_create_time` (`create_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='好友关系表';

CREATE TABLE `im_friend_request` (
    `id`                BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '自增ID',
    `request_id`       VARCHAR(32) NOT NULL COMMENT '申请ID',
    `from_user_id`     VARCHAR(32) NOT NULL COMMENT '申请人ID',
    `from_nickname`    VARCHAR(64) NOT NULL DEFAULT '' COMMENT '申请人昵称',
    `from_avatar`      VARCHAR(512) DEFAULT '' COMMENT '申请人头像',
    `to_user_id`       VARCHAR(32) NOT NULL COMMENT '被申请人ID',
    `to_nickname`      VARCHAR(64) NOT NULL DEFAULT '' COMMENT '被申请人昵称',
    `remark`           VARCHAR(256) DEFAULT '' COMMENT '申请留言',
    `status`           TINYINT NOT NULL DEFAULT 0 COMMENT '状态：0待处理 1已同意 2已拒绝 3已过期',
    `handle_time`      DATETIME DEFAULT NULL COMMENT '处理时间',
    `expire_time`      DATETIME NOT NULL COMMENT '过期时间',
    `create_time`      DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '申请时间',

    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_request_id` (`request_id`),
    KEY `idx_to_user_status` (`to_user_id`, `status`),
    KEY `idx_from_user_id` (`from_user_id`),
    KEY `idx_create_time` (`create_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='好友申请表';

CREATE TABLE `im_blacklist` (
    `id`                BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '自增ID',
    `user_id`          VARCHAR(32) NOT NULL COMMENT '用户ID',
    `block_user_id`    VARCHAR(32) NOT NULL COMMENT '被拉黑用户ID',
    `remark`           VARCHAR(256) DEFAULT '' COMMENT '拉黑原因',
    `create_time`      DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '拉黑时间',

    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_user_block` (`user_id`, `block_user_id`),
    KEY `idx_user_id` (`user_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='黑名单表';

-- ============================================
-- 3. 群组模块
-- ============================================

CREATE TABLE `im_group` (
    `id`                BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '自增ID',
    `group_id`          VARCHAR(32) NOT NULL COMMENT '群ID',
    `group_name`       VARCHAR(128) NOT NULL DEFAULT '' COMMENT '群名称',
    `group_avatar`     VARCHAR(512) DEFAULT '' COMMENT '群头像URL',
    `owner_id`         VARCHAR(32) NOT NULL COMMENT '群主ID',
    `group_type`       TINYINT NOT NULL DEFAULT 1 COMMENT '群类型：1普通群 2超级群 3广播群',
    `member_count`     INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '成员数量',
    `max_member_count` INT UNSIGNED NOT NULL DEFAULT 500 COMMENT '最大成员数',
    `status`           TINYINT NOT NULL DEFAULT 1 COMMENT '状态：0解散 1正常',
    `apply_type`       TINYINT NOT NULL DEFAULT 1 COMMENT '加群方式：0禁止加入 1允许直接加入 2需要验证',
    `introduction`     VARCHAR(512) DEFAULT '' COMMENT '群介绍',
    `create_time`      DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    `update_time`      DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',

    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_group_id` (`group_id`),
    KEY `idx_owner_id` (`owner_id`),
    KEY `idx_status` (`status`),
    KEY `idx_create_time` (`create_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='群组表';

CREATE TABLE `im_group_member` (
    `id`                BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '自增ID',
    `group_id`         VARCHAR(32) NOT NULL COMMENT '群ID',
    `user_id`          VARCHAR(32) NOT NULL COMMENT '用户ID',
    `nickname`         VARCHAR(64) NOT NULL DEFAULT '' COMMENT '群内昵称',
    `avatar_url`       VARCHAR(512) DEFAULT '' COMMENT '头像URL',
    `role`             TINYINT NOT NULL DEFAULT 1 COMMENT '角色：1普通成员 2管理员 3群主',
    `join_type`        TINYINT NOT NULL DEFAULT 1 COMMENT '加入方式：1邀请 2主动加入',
    `inviter_id`       VARCHAR(32) DEFAULT NULL COMMENT '邀请人ID',
    `is_notification`  TINYINT NOT NULL DEFAULT 1 COMMENT '是否接收通知：0否 1是',
    `is_shield`        TINYINT NOT NULL DEFAULT 0 COMMENT '是否屏蔽群消息：0否 1是',
    `join_time`        DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '加入时间',
    `last_msg_time`    DATETIME DEFAULT NULL COMMENT '最后消息时间',
    `status`           TINYINT NOT NULL DEFAULT 1 COMMENT '状态：0已退出 1正常',

    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_group_user` (`group_id`, `user_id`),
    KEY `idx_group_id` (`group_id`),
    KEY `idx_user_id` (`user_id`),
    KEY `idx_role` (`group_id`, `role`),
    KEY `idx_last_msg_time` (`group_id`, `last_msg_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='群成员表';

CREATE TABLE `im_group_request` (
    `id`                BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '自增ID',
    `request_id`        VARCHAR(32) NOT NULL COMMENT '申请ID',
    `group_id`         VARCHAR(32) NOT NULL COMMENT '群ID',
    `group_name`       VARCHAR(128) NOT NULL DEFAULT '' COMMENT '群名称',
    `user_id`          VARCHAR(32) NOT NULL COMMENT '申请人ID',
    `user_nickname`    VARCHAR(64) NOT NULL DEFAULT '' COMMENT '申请人昵称',
    `user_avatar`      VARCHAR(512) DEFAULT '' COMMENT '申请人头像',
    `remark`           VARCHAR(256) DEFAULT '' COMMENT '申请留言',
    `inviter_id`       VARCHAR(32) DEFAULT NULL COMMENT '邀请人ID',
    `status`           TINYINT NOT NULL DEFAULT 0 COMMENT '状态：0待处理 1已同意 2已拒绝 3已过期',
    `handle_user_id`   VARCHAR(32) DEFAULT NULL COMMENT '处理人ID',
    `handle_time`      DATETIME DEFAULT NULL COMMENT '处理时间',
    `expire_time`      DATETIME NOT NULL COMMENT '过期时间',
    `create_time`      DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '申请时间',

    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_request_id` (`request_id`),
    KEY `idx_group_status` (`group_id`, `status`),
    KEY `idx_user_id` (`user_id`),
    KEY `idx_create_time` (`create_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='加群申请表';

-- ============================================
-- 4. 消息模块
-- ============================================

CREATE TABLE `im_message` (
    `id`                BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '自增ID',
    `msg_id`            VARCHAR(32) NOT NULL COMMENT '消息ID（全局唯一）',
    `msg_type`          TINYINT NOT NULL COMMENT '消息类型：1文本 2图片 3语音 4视频 5文件 6位置 7卡片',
    `chat_type`         TINYINT NOT NULL COMMENT '聊天类型：1单聊 2群聊',
    `from_user_id`      VARCHAR(32) NOT NULL COMMENT '发送者ID',
    `to_user_id`        VARCHAR(32) NOT NULL COMMENT '接收者ID',
    `content_type`      VARCHAR(32) NOT NULL COMMENT '内容类型：text/image/voice/video/file/location/card',
    `content`          TEXT COMMENT '消息内容',
    `client_seq`        BIGINT UNSIGNED DEFAULT NULL COMMENT '客户端消息序号（去重）',
    `client_time`       DATETIME NOT NULL COMMENT '客户端发送时间',
    `server_time`       DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '服务器接收时间',
    `status`            TINYINT NOT NULL DEFAULT 1 COMMENT '状态：1已发送 2已投递 3已读 4已撤回 5已删除',
    `is_revoke`         TINYINT NOT NULL DEFAULT 0 COMMENT '是否已撤回：0否 1是',
    `revoke_time`       DATETIME DEFAULT NULL COMMENT '撤回时间',

    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_msg_id` (`msg_id`),
    KEY `idx_chat` (`chat_type`, `to_user_id`, `server_time`),
    KEY `idx_from_user` (`from_user_id`, `server_time`),
    KEY `idx_to_user` (`to_user_id`, `server_time`),
    KEY `idx_client_seq` (`from_user_id`, `client_seq`),
    KEY `idx_status` (`status`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='消息表';

CREATE TABLE `im_message_read` (
    `id`                BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '自增ID',
    `msg_id`            VARCHAR(32) NOT NULL COMMENT '消息ID',
    `group_id`          VARCHAR(32) NOT NULL COMMENT '群ID',
    `user_id`           VARCHAR(32) NOT NULL COMMENT '用户ID',
    `read_time`         DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '已读时间',

    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_msg_user` (`msg_id`, `user_id`),
    KEY `idx_group_msg` (`group_id`, `msg_id`),
    KEY `idx_user_id` (`user_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='消息已读状态表';

CREATE TABLE `im_offline_message` (
    `id`                BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '自增ID',
    `user_id`           VARCHAR(32) NOT NULL COMMENT '接收用户ID',
    `msg_id`            VARCHAR(32) NOT NULL COMMENT '消息ID',
    `msg_type`          TINYINT NOT NULL COMMENT '消息类型',
    `chat_type`         TINYINT NOT NULL COMMENT '聊天类型：1单聊 2群聊',
    `from_user_id`      VARCHAR(32) NOT NULL COMMENT '发送者ID',
    `to_user_id`        VARCHAR(32) NOT NULL COMMENT '接收者ID',
    `content`           TEXT COMMENT '消息内容',
    `client_time`       DATETIME NOT NULL COMMENT '客户端发送时间',
    `server_time`       DATETIME NOT NULL COMMENT '服务器接收时间',
    `is_pushed`         TINYINT NOT NULL DEFAULT 0 COMMENT '是否已推送：0否 1是',
    `push_time`         DATETIME DEFAULT NULL COMMENT '推送时间',
    `create_time`       DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',

    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_user_msg` (`user_id`, `msg_id`),
    KEY `idx_user_unpushed` (`user_id`, `is_pushed`),
    KEY `idx_create_time` (`create_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='离线消息表';

CREATE TABLE `im_moment` (
    `id`                BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '自增ID',
    `moment_id`         VARCHAR(48) NOT NULL COMMENT '朋友圈ID',
    `user_id`           VARCHAR(32) NOT NULL COMMENT '发布者ID',
    `content`           TEXT COMMENT '文字内容',
    `media_type`        VARCHAR(16) NOT NULL DEFAULT 'text' COMMENT 'text/image',
    `media_json`        MEDIUMTEXT COMMENT '图片data URL JSON',
    `status`            TINYINT NOT NULL DEFAULT 1 COMMENT '状态：1正常 5删除',
    `create_time`       DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '发布时间',
    `update_time`       DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',

    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_moment_id` (`moment_id`),
    KEY `idx_user_time` (`user_id`, `create_time`),
    KEY `idx_create_time` (`create_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='朋友圈动态表';

-- ============================================
-- 5. 文件模块
-- ============================================

CREATE TABLE `im_file` (
    `id`                BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '自增ID',
    `file_id`          VARCHAR(32) NOT NULL COMMENT '文件ID',
    `transfer_id`      VARCHAR(32) NOT NULL COMMENT '传输任务ID',
    `from_user_id`     VARCHAR(32) NOT NULL COMMENT '发送者ID',
    `to_user_id`       VARCHAR(32) NOT NULL COMMENT '接收者ID',
    `file_name`        VARCHAR(256) NOT NULL COMMENT '文件名',
    `file_size`        BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '文件大小（字节）',
    `file_hash`        VARCHAR(64) NOT NULL COMMENT '文件SHA256哈希',
    `mime_type`        VARCHAR(64) NOT NULL COMMENT 'MIME类型',
    `file_path`        VARCHAR(512) DEFAULT '' COMMENT '服务器存储路径',
    `cdn_url`          VARCHAR(512) DEFAULT '' COMMENT 'CDN URL',
    `thumbnail_url`    VARCHAR(512) DEFAULT '' COMMENT '缩略图URL',
    `width`            INT UNSIGNED DEFAULT NULL COMMENT '宽度',
    `height`           INT UNSIGNED DEFAULT NULL COMMENT '高度',
    `duration`         INT UNSIGNED DEFAULT NULL COMMENT '时长（毫秒）',
    `status`           TINYINT NOT NULL DEFAULT 0 COMMENT '状态：0上传中 1完成 2失败',
    `expire_time`      DATETIME DEFAULT NULL COMMENT '过期时间',
    `create_time`       DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',

    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_file_id` (`file_id`),
    UNIQUE KEY `uk_transfer_id` (`transfer_id`),
    KEY `idx_from_user` (`from_user_id`),
    KEY `idx_to_user` (`to_user_id`),
    KEY `idx_file_hash` (`file_hash`),
    KEY `idx_create_time` (`create_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='文件记录表';

CREATE TABLE `im_file_transfer` (
    `id`                BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '自增ID',
    `transfer_id`      VARCHAR(32) NOT NULL COMMENT '传输任务ID',
    `file_id`          VARCHAR(32) NOT NULL COMMENT '文件ID',
    `from_user_id`     VARCHAR(32) NOT NULL COMMENT '发送者ID',
    `to_user_id`       VARCHAR(32) NOT NULL COMMENT '接收者ID',
    `chat_type`        TINYINT NOT NULL COMMENT '聊天类型：1单聊 2群聊',
    `transfer_type`    TINYINT NOT NULL COMMENT '传输方式：1直传 2CDN 3中转',
    `file_size`        BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '文件大小',
    `transferred_size` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '已传输大小',
    `total_chunks`     INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '总分片数',
    `chunk_size`       INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '分片大小',
    `status`           TINYINT NOT NULL DEFAULT 0 COMMENT '状态：0准备 1传输中 2完成 3失败 4取消',
    `error_code`       VARCHAR(32) DEFAULT '' COMMENT '错误码',
    `error_message`    VARCHAR(256) DEFAULT '' COMMENT '错误信息',
    `start_time`       DATETIME DEFAULT NULL COMMENT '开始时间',
    `end_time`         DATETIME DEFAULT NULL COMMENT '结束时间',
    `create_time`       DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',

    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_transfer_id` (`transfer_id`),
    KEY `idx_file_id` (`file_id`),
    KEY `idx_from_user` (`from_user_id`),
    KEY `idx_to_user` (`to_user_id`),
    KEY `idx_status` (`status`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='文件传输记录表';

-- ============================================
-- 6. 初始化测试数据
-- ============================================

-- 插入测试用户（密码都是 123456 的 SHA256 哈希）
INSERT INTO `im_user` (`user_id`, `phone`, `nickname`, `password_hash`, `salt`, `status`) VALUES
('user_001', '+86-13800001001', '张三', 'e10adc3949ba59abbe56e057f20f883e', '123456', 1),
('user_002', '+86-13800001002', '李四', 'e10adc3949ba59abbe56e057f20f883e', '123456', 1),
('user_003', '+86-13800001003', '王五', 'e10adc3949ba59abbe56e057f20f883e', '123456', 1);

-- 初始化用户统计
INSERT INTO `im_user_stats` (`user_id`) VALUES
('user_001'), ('user_002'), ('user_003');

-- 添加好友关系（user_001 和 user_002 互为好友）
INSERT INTO `im_friend` (`user_id`, `friend_id`, `remark`) VALUES
('user_001', 'user_002', '老李'),
('user_002', 'user_001', '小张');

-- 创建测试群
INSERT INTO `im_group` (`group_id`, `group_name`, `owner_id`, `member_count`) VALUES
('group_001', '测试群', 'user_001', 3);

-- 添加群成员
INSERT INTO `im_group_member` (`group_id`, `user_id`, `role`) VALUES
('group_001', 'user_001', 3),  -- 群主
('group_001', 'user_002', 1),  -- 成员
('group_001', 'user_003', 1);  -- 成员

-- 更新群成员数量
UPDATE `im_group` SET member_count = 3 WHERE group_id = 'group_001';

-- ============================================
-- 7. 创建存储过程
-- ============================================

-- 消息分页查询存储过程
DELIMITER //
CREATE PROCEDURE `sp_get_chat_messages`(
    IN p_chat_type INT,
    IN p_to_user_id VARCHAR(32),
    IN p_before_time DATETIME,
    IN p_limit INT
)
BEGIN
    SELECT msg_id, msg_type, chat_type, from_user_id, to_user_id,
           content_type, content, client_time, server_time, status
    FROM im_message
    WHERE chat_type = p_chat_type
      AND to_user_id = p_to_user_id
      AND server_time < p_before_time
    ORDER BY server_time DESC
    LIMIT p_limit;
END //
DELIMITER ;

-- 获取用户会话列表
DELIMITER //
CREATE PROCEDURE `sp_get_user_sessions`(
    IN p_user_id VARCHAR(32)
)
BEGIN
    SELECT s.session_type, s.peer_id, s.session_name, s.session_avatar,
           s.unread_count, s.last_msg_time, s.last_msg_content,
           s.is_disturb, s.is_top,
           u.nickname AS peer_nickname, u.avatar_url AS peer_avatar
    FROM im_user_session s
    LEFT JOIN im_user u ON s.peer_id = u.user_id
    WHERE s.user_id = p_user_id
    ORDER BY s.is_top DESC, s.last_msg_time DESC;
END //
DELIMITER ;

-- ============================================
-- 8. 创建事件（清理过期数据）
-- ============================================

-- 每天凌晨3点清理7天前的离线消息
DELIMITER //
CREATE EVENT `ev_cleanup_offline_messages`
ON SCHEDULE EVERY 1 DAY STARTS '2024-01-01 03:00:00'
DO
BEGIN
    DELETE FROM im_offline_message
    WHERE create_time < DATE_SUB(NOW(), INTERVAL 7 DAY)
      AND is_pushed = 1;
END //
DELIMITER ;

-- ============================================
-- 完成
-- ============================================
SELECT '数据库初始化完成！' AS Result;
