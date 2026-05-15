# IM 即时通讯系统数据库设计

## 1. 数据库整体架构

```
im_server (数据库)
├── 用户模块
│   ├── im_user                 # 用户表
│   ├── im_user_device          # 用户设备表
│   └── im_user_session         # 用户会话表
│
├── 好友关系模块
│   ├── im_friend                # 好友关系表
│   └── im_friend_request       # 好友申请表
│
├── 群组模块
│   ├── im_group                # 群组表
│   ├── im_group_member         # 群成员表
│   └── im_group_request        # 加群申请表
│
├── 消息模块
│   ├── im_message              # 消息表（单聊/群聊）
│   ├── im_message_content      # 消息内容表（分离存储）
│   ├── im_offline_message      # 离线消息表
│   └── im_message_read         # 消息已读状态表
│
├── 文件模块
│   ├── im_file                 # 文件记录表
│   └── im_file_transfer        # 文件传输记录表
│
└── 系统模块
    ├── im_blacklist            # 黑名单表
    └── im_user_stats           # 用户统计表
```

---

## 2. 建表 SQL

### 2.1 用户模块

```sql
-- ============================================
-- 用户表
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

-- ============================================
-- 用户设备表
-- ============================================
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

-- ============================================
-- 用户会话表（用于快速查询用户的会话列表）
-- ============================================
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

-- ============================================
-- 用户统计表
-- ============================================
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
```

---

### 2.2 好友关系模块

```sql
-- ============================================
-- 好友关系表
-- ============================================
CREATE TABLE `im_friend` (
    `id`                BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '自增ID',
    `user_id`          VARCHAR(32) NOT NULL COMMENT '用户ID',
    `friend_id`        VARCHAR(32) NOT NULL COMMENT '好友ID',
    `remark`           VARCHAR(128) DEFAULT '' COMMENT '好友备注',
    `friend_nickname`  VARCHAR(64) DEFAULT '' COMMENT '好友昵称（冗余存储避免查询）',
    `friend_avatar`    MEDIUMTEXT COMMENT '好友头像（冗余）',
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

-- 注意：互为好友需要两条记录
-- user_id=A, friend_id=B 和 user_id=B, friend_id=A

-- ============================================
-- 好友申请表
-- ============================================
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
    `expire_time`      DATETIME NOT NULL COMMENT '过期时间（默认7天）',
    `create_time`      DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '申请时间',

    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_request_id` (`request_id`),
    KEY `idx_to_user_status` (`to_user_id`, `status`),
    KEY `idx_from_user_id` (`from_user_id`),
    KEY `idx_create_time` (`create_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='好友申请表';

-- ============================================
-- 黑名单表
-- ============================================
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
```

---

### 2.3 群组模块

```sql
-- ============================================
-- 群组表
-- ============================================
CREATE TABLE `im_group` (
    `id`                BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '自增ID',
    `group_id`          VARCHAR(32) NOT NULL COMMENT '群ID',
    `group_name`       VARCHAR(128) NOT NULL DEFAULT '' COMMENT '群名称',
    `group_avatar`     MEDIUMTEXT COMMENT '群头像URL或data URL',
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

-- ============================================
-- 群成员表
-- ============================================
CREATE TABLE `im_group_member` (
    `id`                BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '自增ID',
    `group_id`         VARCHAR(32) NOT NULL COMMENT '群ID',
    `user_id`          VARCHAR(32) NOT NULL COMMENT '用户ID',
    `nickname`         VARCHAR(64) NOT NULL DEFAULT '' COMMENT '群内昵称',
    `avatar_url`       MEDIUMTEXT COMMENT '头像URL或data URL（冗余）',
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

-- ============================================
-- 加群申请表
-- ============================================
CREATE TABLE `im_group_request` (
    `id`                BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '自增ID',
    `request_id`        VARCHAR(32) NOT NULL COMMENT '申请ID',
    `group_id`         VARCHAR(32) NOT NULL COMMENT '群ID',
    `group_name`       VARCHAR(128) NOT NULL DEFAULT '' COMMENT '群名称（冗余）',
    `user_id`          VARCHAR(32) NOT NULL COMMENT '申请人ID',
    `user_nickname`    VARCHAR(64) NOT NULL DEFAULT '' COMMENT '申请人昵称',
    `user_avatar`      VARCHAR(512) DEFAULT '' COMMENT '申请人头像',
    `remark`           VARCHAR(256) DEFAULT '' COMMENT '申请留言',
    `inviter_id`       VARCHAR(32) DEFAULT NULL COMMENT '邀请人ID',
    `status`           TINYINT NOT NULL DEFAULT 0 COMMENT '状态：0待处理 1已同意 2已拒绝 3已过期',
    `handle_user_id`   VARCHAR(32) DEFAULT NULL COMMENT '处理人ID（群主或管理员）',
    `handle_time`      DATETIME DEFAULT NULL COMMENT '处理时间',
    `expire_time`      DATETIME NOT NULL COMMENT '过期时间',
    `create_time`      DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '申请时间',

    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_request_id` (`request_id`),
    KEY `idx_group_status` (`group_id`, `status`),
    KEY `idx_user_id` (`user_id`),
    KEY `idx_create_time` (`create_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='加群申请表';
```

---

### 2.4 消息模块

```sql
-- ============================================
-- 消息表（核心表，按月分表）
-- ============================================
-- 注意：生产环境建议按月分表（im_message_202404）
CREATE TABLE `im_message` (
    `id`                BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '自增ID',
    `msg_id`            VARCHAR(32) NOT NULL COMMENT '消息ID（全局唯一）',
    `msg_type`          TINYINT NOT NULL COMMENT '消息类型：1文本 2图片 3语音 4视频 5文件 6位置 7卡片',
    `chat_type`         TINYINT NOT NULL COMMENT '聊天类型：1单聊 2群聊',
    `from_user_id`      VARCHAR(32) NOT NULL COMMENT '发送者ID',
    `to_user_id`        VARCHAR(32) NOT NULL COMMENT '接收者ID（单聊为用户ID，群聊为群ID）',
    `content_type`      VARCHAR(32) NOT NULL COMMENT '内容类型：text/image/voice/video/file/location/card',
    `content`          TEXT COMMENT '消息内容（JSON或纯文本）',
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

-- ============================================
-- 消息已读状态表（群聊用）
-- ============================================
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

-- ============================================
-- 离线消息表
-- ============================================
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
```

---

### 2.5 文件模块

```sql
-- ============================================
-- 文件记录表
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
    `thumbnail_url`    VARCHAR(512) DEFAULT '' COMMENT '缩略图URL（图片/视频）',
    `width`            INT UNSIGNED DEFAULT NULL COMMENT '宽度（图片/视频）',
    `height`           INT UNSIGNED DEFAULT NULL COMMENT '高度（图片/视频）',
    `duration`         INT UNSIGNED DEFAULT NULL COMMENT '时长毫秒（语音/视频）',
    `status`           TINYINT NOT NULL DEFAULT 0 COMMENT '状态：0上传中 1上传完成 2上传失败',
    `expire_time`      DATETIME DEFAULT NULL COMMENT '过期时间（临时文件）',
    `create_time`       DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',

    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_file_id` (`file_id`),
    UNIQUE KEY `uk_transfer_id` (`transfer_id`),
    KEY `idx_from_user` (`from_user_id`),
    KEY `idx_to_user` (`to_user_id`),
    KEY `idx_file_hash` (`file_hash`),
    KEY `idx_create_time` (`create_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='文件记录表';

-- ============================================
-- 文件传输记录表
-- ============================================
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
    `status`           TINYINT NOT NULL DEFAULT 0 COMMENT '状态：0准备传输 1传输中 2传输完成 3传输失败 4已取消',
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
```

---

## 3. 索引设计说明

### 3.1 主键与唯一索引

| 表名 | 主键 | 唯一索引 |
|------|------|---------|
| im_user | id | user_id, phone, email |
| im_user_device | id | device_id, (user_id, device_id) |
| im_friend | id | (user_id, friend_id) |
| im_group | id | group_id |
| im_group_member | id | (group_id, user_id) |
| im_message | id | msg_id |
| im_offline_message | id | (user_id, msg_id) |

### 3.2 普通索引

| 表名 | 索引 | 用途 |
|------|------|------|
| im_user | status, create_time | 状态查询、时间排序 |
| im_user_session | user_id, (user_id, last_msg_time) | 会话列表排序 |
| im_friend | user_id, friend_id | 好友查询 |
| im_group_member | group_id, user_id, (group_id, role) | 群成员查询、管理员查询 |
| im_message | (chat_type, to_user_id, server_time) | 聊天消息分页 |
| im_message | (from_user_id, client_seq) | 消息去重 |
| im_offline_message | (user_id, is_pushed) | 离线消息拉取 |

### 3.3 联合索引设计原则

**最左前缀原则**：
```sql
-- im_message 表的 idx_chat 索引
KEY `idx_chat` (`chat_type`, `to_user_id`, `server_time`)

-- 可支持的查询：
1. WHERE chat_type = 1 AND to_user_id = 'user_002' ORDER BY server_time
2. WHERE chat_type = 1 AND to_user_id = 'user_002'
3. WHERE chat_type = 2 AND to_user_id = 'group_001' ORDER BY server_time

-- 不可使用索引：
1. WHERE to_user_id = 'user_002'  -- 缺少 chat_type
2. ORDER BY server_time           -- 缺少前面的列
```

---

## 4. 分表策略

### 4.1 按月分表（消息表）

```sql
-- 生产环境建议使用 MySQL 分区或中间件（如 ShardingSphere）

-- 按月分表脚本
CREATE TABLE `im_message_202404` LIKE `im_message`;
CREATE TABLE `im_message_202405` LIKE `im_message`;

-- 或使用分区
CREATE TABLE `im_message` (
    -- ... 字段 ...
) PARTITION BY RANGE (TO_DAYS(server_time)) (
    PARTITION p202404 VALUES LESS THAN (TO_DAYS('2024-05-01')),
    PARTITION p202405 VALUES LESS THAN (TO_DAYS('2024-06-01')),
    PARTITION p_future VALUES LESS THAN MAXVALUE
);
```

### 4.2 分片策略

```sql
-- 用户表按 user_id 哈希分片（8个分片）
SHARDING_KEY = hash(user_id) % 8

-- 群消息表按 group_id 哈希分片
SHARDING_KEY = hash(group_id) % 16

-- 单聊消息表按 (from_user_id, to_user_id) 组合分片
SHARDING_KEY = hash(from_user_id + to_user_id) % 32
```

---

## 5. 高并发优化建议

### 5.1 读写分离

```sql
-- 主库：写操作
-- 从库：读操作（消息拉取、好友列表查询）

-- 读写分离配置
-- MySQL: log_slave_updates = ON
-- 中间件: ProxySQL / MySQL Router
```

### 5.2 缓存策略

```sql
-- Redis 缓存热点数据
-- 用户信息缓存（5分钟TTL）
GET user:user_001:info -> {"nickname": "张三", "avatar": "..."}

-- 会话列表缓存（按需更新）
GET user:user_001:sessions -> [session_list]

-- 未读数缓存
GET user:user_001:unread -> {"p2p:user_002": 5, "group:group_001": 10}

-- 用户在线状态
SET user:user_001:online 1 EX 300  -- 5分钟过期
```

### 5.3 消息写入优化

```sql
-- 批量写入（消息队列）
-- 客户端 -> Kafka -> 批量消费 -> 写入 MySQL

-- 异步写入
1. 消息先写入 Redis List
2. 异步线程批量读取并写入 MySQL
3. 保证最终一致性
```

### 5.4 核心查询优化

```sql
-- 1. 消息分页查询（高效）
SELECT * FROM im_message
WHERE chat_type = 1 AND to_user_id = 'user_002'
  AND server_time < '2024-04-24 12:00:00'
ORDER BY server_time DESC
LIMIT 20;

-- 2. 未读消息数（缓存）
SELECT COUNT(*) FROM im_offline_message
WHERE user_id = 'user_002' AND is_pushed = 0;

-- 3. 会话列表（JOIN优化）
SELECT s.*, u.nickname, u.avatar_url
FROM im_user_session s
LEFT JOIN im_user u ON s.peer_id = u.user_id
WHERE s.user_id = 'user_001'
ORDER BY s.last_msg_time DESC
LIMIT 50;
```

---

## 6. 容量评估

### 6.1 存储估算

| 数据类型 | 单条大小 | 日增量 | 1年存储 |
|---------|---------|-------|---------|
| 用户表 | 1KB | 1万用户注册 | 3.6GB |
| 消息表 | 0.5KB | 1亿条消息 | 180GB |
| 文件记录 | 2KB | 100万文件 | 720GB |
| 离线消息 | 0.3KB | 1000万条 | 10GB |

### 6.2 预估 QPS

| 场景 | QPS | 说明 |
|------|-----|------|
| 登录 | 1000 | 峰值 |
| 发送消息 | 50000 | 峰值 |
| 拉取消息 | 20000 | 峰值 |
| 文件上传 | 5000 | 峰值 |

---

## 7. 安全考虑

```sql
-- 1. 敏感数据加密
-- 密码使用 SHA-256 + salt 哈希存储

-- 2. SQL 注入防护
-- 使用预处理语句（PreparedStatement）

-- 3. 数据脱敏
-- 手机号：138****8888
-- 邮箱：a***@example.com

-- 4. 权限控制
-- 用户只能查询自己的消息、会话
WHERE from_user_id = ? OR to_user_id = ?
```

---

## 8. 扩展：消息内容分离

```sql
-- 大消息体分离存储
CREATE TABLE `im_message_content` (
    `id`            BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `msg_id`        VARCHAR(32) NOT NULL COMMENT '消息ID',
    `content`       MEDIUMTEXT NOT NULL COMMENT '消息内容（可存储大文本）',
    `content_size`  INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '内容大小',

    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_msg_id` (`msg_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 消息表只保留引用
ALTER TABLE im_message ADD COLUMN content_id BIGINT UNSIGNED;

-- content_size < 1000 时保留在 im_message.content
-- content_size >= 1000 时存储到 im_message_content
```
