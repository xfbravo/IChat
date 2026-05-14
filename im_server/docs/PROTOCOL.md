# IM 通信协议设计文档

## 1. 概述

### 1.1 协议分层

```
┌─────────────────────────────────────────────────────────┐
│                    应用层 (Application)                    │
│   登录 | 注册 | 单聊 | 群聊 | 文件 | 心跳 | 音视频        │
├─────────────────────────────────────────────────────────┤
│                    消息体 (Message Body)                   │
│              JSON 格式（UTF-8 编码）                      │
├─────────────────────────────────────────────────────────┤
│                    协议层 (Protocol)                      │
│              包头 (Header) + 包体 (Body)                   │
├─────────────────────────────────────────────────────────┤
│                    传输层 (Transport)                     │
│                     TCP Stream                            │
└─────────────────────────────────────────────────────────┘
```

### 1.2 传输方式

| 消息类型 | 传输方式 | 说明 |
|---------|---------|------|
| 控制信令 | TCP | 登录、聊天元数据 |
| 文件/图片 | TCP + 分片 | 大文件分片传输 |
| 语音/视频 | UDP 或 TCP | 实时性要求高时用 UDP |

---

## 2. 包头设计

### 2.1 固定包头（6字节）

```
+----------------+----------------+----------------+
|   Type (2B)    |   Length (4B)  |   Body (N)     |
+----------------+----------------+----------------+
       2                 4                N
```

| 字段 | 长度 | 类型 | 说明 |
|------|------|------|------|
| Type | 2 | uint16 | 消息类型（大端序） |
| Length | 4 | uint32 | 包体长度（大端序），不包含包头 |

当前正式协议版本为 **v1**。v1 的版本不写入包头，而由实现和文档共同约定；后续如需升级包头，应新增协议版本并提供迁移说明，不能静默改变这 6 字节格式。

### 2.2 消息类型定义

```cpp
enum class MsgType : uint16_t {
    HEARTBEAT            = 0x0001,  // 心跳
    LOGIN                = 0x0002,  // 登录请求
    REGISTER_REQ         = 0x0003,  // 注册请求
    LOGOUT               = 0x0004,  // 登出
    CHAT_MESSAGE         = 0x0005,  // 统一聊天消息，content_type 区分 text/image/file/voice
    TEXT                 = 0x0005,  // 旧名称，仅兼容
    IMAGE                = 0x0006,  // 旧媒体类型，仅兼容；新实现使用 CHAT_MESSAGE + content_type=image
    FILE                 = 0x0007,  // 旧媒体类型，仅兼容；新实现使用 CHAT_MESSAGE + content_type=file
    VOICE                = 0x0008,  // 旧媒体类型，仅兼容；新实现使用 CHAT_MESSAGE + content_type=voice
    ACK                  = 0x0009,  // 消息确认
    FRIEND_REQUEST       = 0x000A,  // 发送好友请求
    GET_FRIEND_LIST      = 0x000B,  // 获取好友列表
    GET_FRIEND_REQUESTS  = 0x000C,  // 获取好友请求列表
    FRIEND_REQUEST_RSP   = 0x000D,  // 响应好友请求
    DELETE_FRIEND        = 0x000E,  // 删除好友
    GET_CHAT_HISTORY     = 0x000F,  // 获取聊天记录
    GET_OFFLINE_MESSAGES = 0x0010,  // 获取离线消息
    OFFLINE_MESSAGE_ACK  = 0x0011,  // 离线消息确认
    UPDATE_FRIEND_REMARK = 0x0012,  // 修改好友备注
    UPDATE_AVATAR        = 0x0013,  // 更新头像
    CHANGE_PASSWORD      = 0x0014,  // 修改密码
    UPDATE_PROFILE       = 0x0015,  // 更新个人信息
    GET_USER_PROFILE     = 0x0016,  // 获取用户个人信息
    CREATE_MOMENT        = 0x0017,  // 发布朋友圈
    GET_MOMENTS          = 0x0018,  // 获取朋友圈时间流
    FILE_UPLOAD_START    = 0x0019,  // 开始文件上传
    FILE_UPLOAD_CHUNK    = 0x001A,  // 上传文件分片
    FILE_DOWNLOAD_REQ    = 0x001B,  // 下载文件请求

    LOGIN_RSP            = 0x8002,  // 登录响应
    REGISTER_RSP         = 0x8003,  // 注册响应
    FRIEND_LIST_RSP      = 0x800A,  // 好友列表响应
    FRIEND_REQUEST_NEW   = 0x800B,  // 新好友请求通知
    FRIEND_LIST_UPDATE   = 0x800C,  // 好友列表更新通知
    ERROR                = 0x800F,  // 错误响应
    CHAT_HISTORY_RSP     = 0x8010,  // 聊天记录响应
    OFFLINE_MESSAGE      = 0x8011,  // 离线消息推送
    UPDATE_FRIEND_REMARK_RSP = 0x8012, // 修改好友备注响应
    UPDATE_AVATAR_RSP    = 0x8013,  // 更新头像响应
    CHANGE_PASSWORD_RSP  = 0x8014,  // 修改密码响应
    UPDATE_PROFILE_RSP   = 0x8015,  // 更新个人信息响应
    USER_PROFILE_RSP     = 0x8016,  // 用户个人信息响应
    CREATE_MOMENT_RSP    = 0x8017,  // 发布朋友圈响应
    MOMENTS_RSP          = 0x8018,  // 朋友圈时间流响应
    FILE_UPLOAD_RSP      = 0x8019,  // 文件上传响应
    FILE_DOWNLOAD_RSP    = 0x801A,  // 文件下载响应
    FILE_DOWNLOAD_CHUNK  = 0x801B,  // 文件下载分片
};
```

---

## 3. 消息格式

### 3.1 心跳消息

**请求**:
```json
{
    "user_id": "user_001",
    "timestamp": 1713900000,
    "client_time": 1713900000
}
```

**响应**:
```json
{
    "server_time": 1713900001,
    "interval": 30
}
```

---

### 3.2 登录消息

**请求 (LOGIN_REQ)**:
```json
{
    "user_id": "user_001",
    "password_hash": "sha256_hash_of_password",
    "device_type": "ios",           // ios | android | windows | mac | web
    "device_id": "uuid-device-id",
    "app_version": "1.0.0",
    "os_version": "17.0",
    "client_time": 1713900000
}
```

**响应 (LOGIN_RSP)**:
```json
{
    "code": 0,
    "message": "登录成功",
    "user_id": "user_001",
    "nickname": "张三",
    "avatar_url": "https://cdn.example.com/avatar/user_001.jpg",
    "gender": "男",
    "region": "北京",
    "signature": "Somebody feels the rain，somebody just gets wet",
    "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",  // JWT Token
    "refresh_token": "refresh_token_string",
    "expires_in": 604800,              // token 有效期（秒）
    "server_time": 1713900001
}
```

---

### 3.3 注册消息

**请求 (REGISTER_REQ)**:
```json
{
    "user_id": "user_002",
    "nickname": "李四",
    "password_hash": "sha256_hash_of_password",
    "phone": "+86-138****8888",
    "email": "user002@example.com",
    "verification_code": "123456",     // 验证码
    "client_time": 1713900000
}
```

**响应 (REGISTER_RSP)**:
```json
{
    "code": 0,
    "message": "注册成功",
    "user_id": "user_002"
}
```

### 3.4 更新头像

客户端在“我”页选择本地图片后，将图片裁剪压缩为 JPEG data URL，再同步到服务端。服务端会保存到用户资料中，并在后续 `LOGIN_RSP` 的 `avatar_url` 中返回。

**请求 (UPDATE_AVATAR / 0x0013)**:
```json
{
    "avatar_url": "data:image/jpeg;base64,..."
}
```

**响应 (UPDATE_AVATAR_RSP / 0x8013)**:
```json
{
    "code": 0,
    "message": "头像已同步",
    "avatar_url": "data:image/jpeg;base64,..."
}
```

---

### 3.5 更新个人信息

**请求 (UPDATE_PROFILE / 0x0015)**:
```json
{
    "nickname": "张三",
    "gender": "男",
    "region": "北京",
    "signature": "Somebody feels the rain，somebody just gets wet"
}
```

**响应 (UPDATE_PROFILE_RSP / 0x8015)**:
```json
{
    "code": 0,
    "message": "资料已保存",
    "nickname": "张三",
    "gender": "男",
    "region": "北京",
    "signature": "Somebody feels the rain，somebody just gets wet"
}
```

---

### 3.6 获取用户个人信息

头像点击、联系人资料查看等场景使用该接口按 `user_id` 拉取只读资料。响应不包含手机号、邮箱等私密字段。

**请求 (GET_USER_PROFILE / 0x0016)**:
```json
{
    "user_id": "user_002"
}
```

当前用户进入“我”导航栏或个人信息页时，客户端会带上本地缓存快照。服务端只会在 `client_user_id`、`user_id` 与当前登录会话完全一致时比较本地快照；一致则只返回 ACK，不下发资料字段。
```json
{
    "user_id": "17275536202",
    "client_user_id": "17275536202",
    "local_profile": {
        "nickname": "XFNotFound",
        "gender": "男",
        "region": "北京",
        "signature": "BIT rocks"
    }
}
```

**响应 (USER_PROFILE_RSP / 0x8016)**:
```json
{
    "code": 0,
    "message": "获取成功",
    "sync_status": "full",
    "user_id": "user_002",
    "nickname": "李四",
    "avatar_url": "data:image/jpeg;base64,...",
    "gender": "女",
    "region": "上海",
    "signature": "保持热爱"
}
```

当前用户本地缓存与服务端一致时，服务端仅返回 ACK：
```json
{
    "code": 0,
    "message": "资料已同步",
    "sync_status": "same",
    "user_id": "17275536202"
}
```

---

### 3.7 统一聊天消息

**发送请求 (CHAT_MESSAGE / 0x0005)**:
```json
{
    "msg_id": "uuid-msg-001",
    "from_user_id": "user_001",
    "to_user_id": "user_002",
    "content_type": "text",            // text | image | voice | video | file
    "content": "你好，这是一条测试消息",
    "client_time": 1713900000,
    "client_seq": 1001                 // 客户端消息序号（用于去重）
}
```

**已收到确认 (ACK / 0x0009)**:
```json
{
    "msg_id": "uuid-msg-001",
    "from_user_id": "user_001",
    "to_user_id": "user_002",
    "code": 0,
    "server_time": 1713900001
}
```

**完整接收方消息格式**:
```json
{
    "msg_id": "uuid-msg-001",
    "from_user_id": "user_001",
    "from_nickname": "张三",
    "from_avatar": "https://cdn.example.com/avatar/user_001.jpg",
    "to_user_id": "user_002",
    "content_type": "text",
    "content": "你好，这是一条测试消息",
    "client_time": 1713900000,
    "server_time": 1713900001
}
```

---

### 3.8 群聊消息（规划）

当前代码尚未实现群聊专用消息类型。后续实现时仍应复用 `CHAT_MESSAGE` 的正文结构，通过 `chat_type` 和 `group_id` 区分群聊，而不是新增与媒体类型绑定的包头类型。

**建议请求体 (CHAT_MESSAGE / 0x0005)**:
```json
{
    "msg_id": "uuid-msg-002",
    "from_user_id": "user_001",
    "group_id": "group_001",
    "content_type": "text",
    "content": "大家好！",
    "client_time": 1713900000,
    "client_seq": 1002
}
```

**群消息接收格式**:
```json
{
    "msg_id": "uuid-msg-002",
    "from_user_id": "user_001",
    "from_nickname": "张三",
    "from_avatar": "https://cdn.example.com/avatar/user_001.jpg",
    "group_id": "group_001",
    "group_name": "测试群",
    "content_type": "text",
    "content": "大家好！",
    "client_time": 1713900000,
    "server_time": 1713900001,
    "member_count": 50                  // 群成员数
}
```

---

### 3.9 文件/媒体消息

图片、文件、语音、视频都通过 `CHAT_MESSAGE / 0x0005` 发送，`content_type` 决定内容类型。旧的 `IMAGE(0x0006)`、`FILE(0x0007)`、`VOICE(0x0008)` 仅用于兼容旧客户端，服务端会按统一聊天消息处理并以 `CHAT_MESSAGE` 转发。

#### 3.9.1 文件上传开始

**请求 (FILE_UPLOAD_START / 0x0019)**:
```json
{
    "transfer_id": "uuid-transfer-001",
    "to_user_id": "user_002",
    "file_name": "document.pdf",
    "file_size": 1048576,
    "mime_type": "application/pdf",
    "total_chunks": 4
}
```

**响应 (FILE_UPLOAD_RSP / 0x8019)**:
```json
{
    "code": 0,
    "status": "ready",
    "message": "可以上传",
    "transfer_id": "uuid-transfer-001",
    "file_id": "server-file-id",
    "next_chunk_index": 0
}
```

服务端限制单个文件最大 200MB。客户端可一次选择多个文件，但每个文件独立创建上传任务。

#### 3.9.2 文件上传分片

**请求 (FILE_UPLOAD_CHUNK / 0x001A)**:
```json
{
    "transfer_id": "uuid-transfer-001",
    "chunk_index": 0,
    "data": "base64_encoded_data"
}
```

**响应 (FILE_UPLOAD_RSP / 0x8019)**:
```json
{
    "code": 0,
    "status": "chunk",
    "message": "分片已接收",
    "transfer_id": "uuid-transfer-001",
    "next_chunk_index": 1,
    "received_size": 262144
}
```

最后一个分片完成后，服务端返回：

```json
{
    "code": 0,
    "status": "complete",
    "message": "上传完成",
    "transfer_id": "uuid-transfer-001",
    "file_id": "server-file-id",
    "file_name": "document.pdf",
    "file_size": 1048576,
    "mime_type": "application/pdf"
}
```

客户端收到 `complete` 后，再发送一条普通聊天消息。

#### 3.9.3 文件消息

**请求 (CHAT_MESSAGE / 0x0005)**:
```json
{
    "msg_id": "uuid-msg-file-001",
    "from_user_id": "user_001",
    "to_user_id": "user_002",
    "content_type": "file",
    "content": "{\"file_id\":\"server-file-id\",\"file_name\":\"document.pdf\",\"file_size\":1048576,\"mime_type\":\"application/pdf\",\"transfer_id\":\"uuid-transfer-001\"}",
    "client_time": 1713900000
}
```

#### 3.9.4 文件下载

**请求 (FILE_DOWNLOAD_REQ / 0x001B)**:
```json
{
    "transfer_id": "uuid-download-001",
    "file_id": "server-file-id",
    "file_name": "document.pdf"
}
```

**响应 (FILE_DOWNLOAD_RSP / 0x801A)**:
```json
{
    "code": 0,
    "status": "ready",
    "message": "开始下载",
    "transfer_id": "uuid-download-001",
    "file_id": "server-file-id",
    "file_name": "document.pdf",
    "file_size": 1048576,
    "total_chunks": 4
}
```

**下载分片 (FILE_DOWNLOAD_CHUNK / 0x801B)**:
```json
{
    "code": 0,
    "transfer_id": "uuid-download-001",
    "file_id": "server-file-id",
    "file_name": "document.pdf",
    "file_size": 1048576,
    "chunk_index": 0,
    "total_chunks": 4,
    "data": "base64_encoded_data"
}
```

下载结束后服务端再次返回 `FILE_DOWNLOAD_RSP`：

```json
{
    "code": 0,
    "status": "complete",
    "message": "下载完成",
    "transfer_id": "uuid-download-001",
    "file_id": "server-file-id",
    "file_name": "document.pdf"
}
```

---

### 3.10 图片消息

**请求 (CHAT_MESSAGE / 0x0005)**:
```json
{
    "msg_id": "uuid-msg-003",
    "from_user_id": "user_001",
    "to_user_id": "user_002",
    "content_type": "image",
    "image": {
        "original": {
            "width": 1920,
            "height": 1080,
            "size": 204800,
            "hash": "sha256_original"
        },
        "thumbnail": {
            "width": 200,
            "height": 200,
            "size": 10240,
            "hash": "sha256_thumbnail",
            "data": "base64_thumbnail_data"
        },
        "url": "https://cdn.example.com/images/uuid-msg-003.jpg"
    },
    "client_time": 1713900000
}
```

---

### 3.11 语音消息

**请求 (CHAT_MESSAGE / 0x0005)**:
```json
{
    "msg_id": "uuid-msg-004",
    "from_user_id": "user_001",
    "to_user_id": "user_002",
    "content_type": "voice",
    "voice": {
        "duration": 5000,              // 语音时长（毫秒）
        "size": 10240,                 // 文件大小
        "format": "aac",               // 编码格式
        "url": "https://cdn.example.com/voice/uuid-msg-004.aac",
        "waveform": [0.1, 0.5, 0.3...] // 波形数据（可选）
    },
    "client_time": 1713900000
}
```

---

### 3.12 音视频通话

**呼叫邀请 (CALL_INVITE)**:
```json
{
    "call_id": "uuid-call-001",
    "from_user_id": "user_001",
    "from_nickname": "张三",
    "to_user_id": "user_002",
    "call_type": "video",              // audio | video
    "room_id": "uuid-room-001",        // 房间ID
    "sdp": {                           // 初始SDP（可选）
        "type": "offer",
        "sdp": "v=0\r\no=- ..."
    },
    "timestamp": 1713900000
}
```

**接听 (CALL_ACCEPT)**:
```json
{
    "call_id": "uuid-call-001",
    "to_user_id": "user_001",
    "code": 0,
    "sdp": {
        "type": "answer",
        "sdp": "v=0\r\no=- ..."
    },
    "timestamp": 1713900001
}
```

**ICE候选 (CALL_ICE)**:
```json
{
    "call_id": "uuid-call-001",
    "from_user_id": "user_001",
    "candidate": {
        "foundation": "1",
        "component": 1,
        "protocol": "udp",
        "priority": 2130706431,
        "ip": "192.168.1.100",
        "port": 5000,
        "type": "host"
    },
    "timestamp": 1713900002
}
```

---

## 4. 通信流程

### 4.1 连接建立与心跳

```
客户端                                              服务器
   |                                                    |
   |  1. TCP 连接 (三次握手)                           |
   |-------------------------------------------------->|
   |                                                    |
   |  2. 发送 LOGIN_REQ                                |
   |-------------------------------------------------->|
   |                                                    |
   |  3. 接收 LOGIN_RSP (包含 token)                   |
   |<--------------------------------------------------|
   |                                                    |
   |  ========= 连接已建立，开始心跳 =========          |
   |                                                    |
   |  4. 发送 HEARTBEAT (每30秒)                       |
   |-------------------------------------------------->|
   |                                                    |
   |  5. 接收 HEARTBEAT_ACK                            |
   |<--------------------------------------------------|
   |                                                    |
   |  (重复 4-5 直到连接断开)                          |
   |                                                    |
```

### 4.2 登录流程

```
客户端                                              服务器
   |                                                    |
   |  发送 LOGIN_REQ:                                  |
   |  {                                                 |
   |    "user_id": "user_001",                        |
   |    "password_hash": "xxx",                       |
   |    "device_type": "windows"                       |
   |  }                                                 |
   |-------------------------------------------------->|
   |                                                    |
   |                              验证密码 & 生成 Token |
   |                              更新用户在线状态       |
   |                                                    |
   |  接收 LOGIN_RSP:                                  |
   |  {                                                 |
   |    "code": 0,                                     |
   |    "token": "eyJhbG...",                          |
   |    "expires_in": 604800                           |
   |  }                                                 |
   |<--------------------------------------------------|
   |                                                    |
   |  使用 token 发送后续请求                          |
   |-------------------------------------------------->|
   |                                                    |
```

### 4.3 单聊消息流程

```
用户A (user_001)                          用户B (user_002)
   |                                              |
   |  1. 发送 CHAT_MESSAGE                       |
   |     msg_id: "uuid-001"                      |
   |--------------------------------------------->|
   |                                              |
   |                         服务器转发消息给B    |
   |                                              |
   |  2. 接收 ACK (msg_id确认)                   |
   |     code: 0                                  |
   |<---------------------------------------------|  (A 知道服务器已收到)
   |                                              |
   |                         3. B 发送 MSG_READ   |
   |                        (表示已读)            |
   |<---------------------------------------------|  (A 收到"已读"状态)
   |                                              |
```

**消息完整流转**:

```
[客户端A] --CHAT_MESSAGE--> [服务器] --CHAT_MESSAGE--> [客户端B]
                |                    |
                v                    v
         [存储消息到DB]        [推送通知]
                |                    |
                v                    v
         [返回ACK给A]           [B发送已读]
                |                    |
                v                    v
         [A显示"已发送"] <---- [A收到已读]
```

### 4.4 群聊消息流程

```
用户A                                    服务器                           用户B, C, D
   |                                        |                                |
   |  1. 发送 CHAT_MESSAGE                  |                                |
   |     group_id: "group_001"             |                                |
   |--------------------------------------->|                                |
   |                                        |                                |
   |                   2. 存储消息到DB     |                                |
   |                   3. 查询群成员列表    |                                |
   |                   4. 转发消息给每个成员 |                                |
   |                                        |                                |
   |  5. ACK                               |                                |
   |<---------------------------------------|                                |
   |                                        |                                |
   |                              6. CHAT_MESSAGE (转发给B, C, D)           |
   |                             ------------------------------------------>|
```

### 4.5 文件传输流程

```
发送方                                              服务器
   |                                                    |
   |  1. FILE_UPLOAD_START(file_name, size, chunks)     |
   |-------------------------------------------------->|
   |  2. FILE_UPLOAD_RSP(status=ready, file_id)         |
   |<--------------------------------------------------|
   |  3. FILE_UPLOAD_CHUNK(base64 chunk 0..N)           |
   |-------------------------------------------------->|
   |  4. FILE_UPLOAD_RSP(status=complete)               |
   |<--------------------------------------------------|
   |  5. CHAT_MESSAGE(content_type=file)                |
   |     content: { file_id, file_name, file_size }      |
   |-------------------------------------------------->|
   |                                                    |
   |                              6. 保存文件消息元数据                      |
   |                              7. 按 CHAT_MESSAGE 转发                    |
   |                                                    |
   |  接收 CHAT_MESSAGE                                |
   |  { content_type: "file", content: "{...file_id...}" } |
   |<--------------------------------------------------|
```

接收方点击下载时：

```
接收方                                              服务器
   |  1. FILE_DOWNLOAD_REQ(file_id)                     |
   |-------------------------------------------------->|
   |  2. FILE_DOWNLOAD_RSP(status=ready)                |
   |<--------------------------------------------------|
   |  3. FILE_DOWNLOAD_CHUNK(base64 chunk 0..N)         |
   |<--------------------------------------------------|
   |  4. FILE_DOWNLOAD_RSP(status=complete)             |
   |<--------------------------------------------------|
```

### 4.6 音视频通话流程 (WebRTC)

```
用户A (user_001)                    用户B (user_002)          信令服务器
   |                                    |                          |
   |  1. CALL_INVITE (offer)            |                          |
   |------------------------------------>                          |
   |                                    |                          |
   |  2. CALL_RINGING (中转)            |                          |
   |<------------------------------------                          |
   |                                    |                          |
   |  3. CALL_ACCEPT (answer)           |                          |
   |------------------------------------>                          |
   |                                    |                          |
   |  4. SDP exchange (ICE candidates)   |                          |
   |<==================================>|  (直连或中继)             |
   |                                    |                          |
   |  5. 音视频流直连 (RTP/RTCP)        |                          |
   |<==================================>                          |
   |                                    |                          |
   |  6. CALL_HANGUP                    |                          |
   |------------------------------------>                          |
```

---

## 5. 粘包处理

### 5.1 问题分析

TCP 是流式协议，可能出现：
- **粘包**：多个消息合并为一个数据包
- **拆包**：一个消息分散到多个数据包

### 5.2 解决方案

**固定包头 + 长度字段**：

```
┌──────────┬────────────┬─────────────────────────┐
│   Type   │   Length  │        Body            │
│   2字节   │   4字节    │   Length 指定的字节数  │
└──────────┴────────────┴─────────────────────────┘
```

### 5.3 处理流程

**发送端**:
```
1. 序列化消息体 (JSON)
2. 计算长度
3. 组装: [Type(2)] [Length(4)] [Body]
4. 发送
```

**接收端**:
```
1. 读取 6 字节包头
2. 解析 Type 和 Length
3. 根据 Length 读取对应字节数
4. 反序列化 Body
5. 处理消息
6. 循环直到数据处理完毕
```

### 5.4 示例

假设发送两条消息：

**消息1**: Type=0x0005 (CHAT_MESSAGE), Body=`{"content":"hi"}`
```
0x00 0x05 0x00 0x00 0x00 0x10 ...(16字节body)...
[ TYPE ][     LENGTH     ][   BODY   ]
```

**消息2**: Type=0x0005 (CHAT_MESSAGE), Body=`{"content":"hello"}`
```
0x00 0x05 0x00 0x00 0x00 0x13 ...(19字节body)...
```

**接收端缓冲区状态**:
```
[包头1][16字节body][包头2][19字节body]
   6        16            6        19   = 47 字节
```

接收端循环处理：
1. 读取6字节 → 获取 Type=0x0005, Length=16
2. 读取16字节 → 完整消息1
3. 继续处理剩余数据 → 获取 Type=0x0005, Length=19
4. 读取19字节 → 完整消息2

---

## 6. Protobuf 格式（可选）

如果对性能有更高要求，可以使用 Protobuf：

```protobuf
syntax = "proto3";
package im;

message Message {
    uint32 version = 1;
    uint32 type = 2;
    bytes body = 3;
}

message LoginReq {
    string user_id = 1;
    string password_hash = 2;
    string device_type = 3;
    string device_id = 4;
}

message LoginRsp {
    uint32 code = 1;
    string message = 2;
    string user_id = 3;
    string token = 4;
    uint64 expires_in = 5;
}

message ChatMessage {
    string msg_id = 1;
    string from_user_id = 2;
    string to_user_id = 3;
    uint32 content_type = 4;
    string content = 5;
    uint64 client_time = 6;
}
```

---

## 7. 错误码定义

| 错误码 | 说明 |
|-------|------|
| 0 | 成功 |
| 1001 | 无效的用户名或密码 |
| 1002 | Token 过期 |
| 1003 | Token 无效 |
| 1004 | 用户已登录 |
| 1005 | 用户不存在 |
| 2001 | 消息发送失败 |
| 2002 | 对方不在线 |
| 2003 | 消息被撤回 |
| 3001 | 文件传输失败 |
| 3002 | 文件过大 |
| 3003 | 不支持的文件类型 |
| 4001 | 通话邀请已过期 |
| 4002 | 通话对方正在忙 |
| 4003 | 通话对方无响应 |
| 5001 | 服务器内部错误 |
| 5002 | 服务器繁忙 |

---

## 8. 安全考虑

1. **密码传输**: 使用 HTTPS 或在客户端做 SHA-256 哈希
2. **Token**: 使用 JWT，设置合理过期时间
3. **消息加密**: 端到端加密（E2E Encryption）
4. **文件安全**: 上传后做病毒扫描，生成私有 URL

---

## 9. 协议版本演进

| 版本 | 日期 | 变更说明 |
|------|------|---------|
| 0x01 | 2024-04 | 初版设计 |
