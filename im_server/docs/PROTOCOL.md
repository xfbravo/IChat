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

### 2.1 固定包头（12字节）

```
+----------------+----------------+----------------+----------------+
|  Version (1B) |   Type (2B)   |   Length (4B)  |  Reserved (5B) |
+----------------+----------------+----------------+----------------+
      1                2                4                  5
```

| 字段 | 长度 | 类型 | 说明 |
|------|------|------|------|
| Version | 1 | uint8 | 协议版本，当前为 0x01 |
| Type | 2 | uint16 | 消息类型（大端序） |
| Length | 4 | uint32 | 包体长度（大端序），不包含包头 |
| Reserved | 5 | bytes | 保留字段，用于扩展或校验 |

### 2.2 消息类型定义

```cpp
enum class MsgType : uint16_t {
    // ===== 认证消息 (0x0001 - 0x00FF) =====
    HEARTBEAT         = 0x0001,   // 心跳
    LOGIN_REQ         = 0x0002,   // 登录请求
    LOGIN_RSP         = 0x8002,   // 登录响应
    REGISTER_REQ      = 0x0003,   // 注册请求
    REGISTER_RSP      = 0x8003,   // 注册响应
    LOGOUT            = 0x0004,   // 登出
    TOKEN_REFRESH     = 0x0005,   // Token刷新

    // ===== 好友/群组消息 (0x0100 - 0x01FF) =====
    FRIEND_LIST       = 0x0101,   // 好友列表请求
    FRIEND_LIST_RSP   = 0x8101,   // 好友列表响应
    GROUP_LIST        = 0x0102,   // 群组列表请求
    GROUP_LIST_RSP    = 0x8102,   // 群组列表响应
    ADD_FRIEND        = 0x0103,   // 添加好友
    ADD_FRIEND_RSP    = 0x8103,   // 添加好友响应
    CREATE_GROUP      = 0x0104,   // 创建群组
    CREATE_GROUP_RSP   = 0x8104,   // 创建群组响应
    JOIN_GROUP        = 0x0105,   // 加入群组
    JOIN_GROUP_RSP    = 0x8105,   // 加入群组响应

    // ===== 聊天消息 (0x0200 - 0x02FF) =====
    P2P_MSG           = 0x0201,   // 单聊消息
    P2P_MSG_ACK      = 0x8201,   // 单聊消息已收到确认
    GROUP_MSG         = 0x0202,   // 群聊消息
    GROUP_MSG_ACK    = 0x8202,   // 群聊消息已收到确认
    MSG_RECALL        = 0x0203,   // 消息撤回
    MSG_READ          = 0x0204,   // 消息已读

    // ===== 文件/媒体消息 (0x0300 - 0x03FF) =====
    FILE_TRANS_REQ    = 0x0301,   // 文件传输请求
    FILE_TRANS_RSP    = 0x8301,   // 文件传输响应
    FILE_TRANS_DATA   = 0x0302,   // 文件数据（分片）
    FILE_TRANS_END    = 0x0303,   // 文件传输结束
    IMAGE_MSG         = 0x0304,   // 图片消息（缩略图）
    VOICE_MSG         = 0x0305,   // 语音消息
    VIDEO_MSG         = 0x0306,   // 视频消息（metadata）

    // ===== 音视频 (0x0400 - 0x04FF) =====
    CALL_INVITE       = 0x0401,   // 呼叫邀请
    CALL_RINGING      = 0x0402,   // 响铃
    CALL_ACCEPT       = 0x0403,   // 接听
    CALL_REJECT       = 0x0404,   // 拒绝
    CALL_HANGUP       = 0x0405,   // 挂断
    CALL_SDP          = 0x0406,   // SDP 交换（WebRTC）
    CALL_ICE          = 0x0407,   // ICE 候选交换

    // ===== 错误响应 (0x8000 - 0x8FFF) =====
    ERROR             = 0x8000,   // 通用错误
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

---

### 3.4 单聊消息

**发送请求 (P2P_MSG)**:
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

**已收到确认 (P2P_MSG_ACK)**:
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

### 3.5 群聊消息

**发送请求 (GROUP_MSG)**:
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

### 3.6 文件传输

#### 3.6.1 文件传输请求

**请求 (FILE_TRANS_REQ)**:
```json
{
    "transfer_id": "uuid-transfer-001",
    "from_user_id": "user_001",
    "to_user_id": "user_002",          // 接收方（个人）或 group_id
    "file_name": "document.pdf",
    "file_size": 1048576,              // 文件大小（字节）
    "file_hash": "sha256_hash",        // 文件内容哈希
    "mime_type": "application/pdf",
    "thumbnail": "base64_encoded_thumbnail",  // 图片/视频缩略图
    "client_time": 1713900000
}
```

**响应 (FILE_TRANS_RSP)**:
```json
{
    "transfer_id": "uuid-transfer-001",
    "code": 0,
    "message": "开始传输",
    "server_time": 1713900001,
    "download_url": "https://cdn.example.com/files/uuid-transfer-001"  // CDN地址
}
```

#### 3.6.2 文件分片数据

**请求 (FILE_TRANS_DATA)**:
```json
{
    "transfer_id": "uuid-transfer-001",
    "chunk_index": 0,                  // 分片索引（从0开始）
    "chunk_size": 65536,               // 分片大小（64KB）
    "total_chunks": 16,                 // 总分片数
    "data": "base64_encoded_data",
    "offset": 0                         // 文件内偏移
}
```

#### 3.6.3 传输完成

**请求 (FILE_TRANS_END)**:
```json
{
    "transfer_id": "uuid-transfer-001",
    "file_hash": "sha256_hash",
    "code": 0,
    "message": "传输完成",
    "client_time": 1713900000
}
```

---

### 3.7 图片消息

**请求 (IMAGE_MSG)**:
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

### 3.8 语音消息

**请求 (VOICE_MSG)**:
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

### 3.9 音视频通话

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
   |  1. 发送 P2P_MSG                            |
   |     msg_id: "uuid-001"                      |
   |--------------------------------------------->|
   |                                              |
   |                         服务器转发消息给B    |
   |                                              |
   |  2. 接收 P2P_MSG_ACK (msg_id确认)           |
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
[客户端A] --P2P_MSG--> [服务器] --P2P_MSG--> [客户端B]
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
   |  1. 发送 GROUP_MSG                     |                                |
   |     group_id: "group_001"             |                                |
   |--------------------------------------->|                                |
   |                                        |                                |
   |                   2. 存储消息到DB     |                                |
   |                   3. 查询群成员列表    |                                |
   |                   4. 转发消息给每个成员 |                                |
   |                                        |                                |
   |  5. GROUP_MSG_ACK                     |                                |
   |<---------------------------------------|                                |
   |                                        |                                |
   |                              6. GROUP_MSG (转发给B, C, D)              |
   |                             ------------------------------------------>|
```

### 4.5 文件传输流程

```
发送方                                              服务器
   |                                                    |
   |  1. 发送 FILE_TRANS_REQ                           |
   |     file_name: "doc.pdf", size: 1048576           |
   |-------------------------------------------------->|
   |                                                    |
   |                              2. 验证 & 创建传输任务                     |
   |                              3. 返回 download_url                       |
   |                                                    |
   |  接收 FILE_TRANS_RSP                              |
   |  { download_url: "https://cdn..." }                |
   |<--------------------------------------------------|
   |                                                    |
   |  4. 分片上传 FILE_TRANS_DATA                      |
   |     chunk_index: 0, data: "base64..."             |
   |-------------------------------------------------->|
   |  5. chunk_index: 1                                |
   |-------------------------------------------------->|
   |  ... (继续上传剩余分片)                           |
   |                                                    |
   |  N. 发送 FILE_TRANS_END                          |
   |     file_hash: "sha256..."                        |
   |-------------------------------------------------->|
   |                                                    |
   |                              验证 file_hash       |
   |                              生成 CDN URL          |
   |                                                    |
   |  接收 FILE_TRANS_END_ACK                          |
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
│  Header  │   Length  │        Body            │
│  12字节   │   4字节    │   Length 指定的字节数  │
└──────────┴────────────┴─────────────────────────┘
```

### 5.3 处理流程

**发送端**:
```
1. 序列化消息体 (JSON)
2. 计算长度
3. 组装: [Version(1)] [Type(2)] [Length(4)] [Reserved(5)] [Body]
4. 发送
```

**接收端**:
```
1. 读取 12 字节包头
2. 解析 Type 和 Length
3. 根据 Length 读取对应字节数
4. 反序列化 Body
5. 处理消息
6. 循环直到数据处理完毕
```

### 5.4 示例

假设发送两条消息：

**消息1**: Type=0x0201 (P2P_MSG), Body=`{"content":"hi"}`
```
0x01 0x02 0x01 0x00 0x00 0x00 0x0D ...(13字节body)...
[VER][  TYPE  ][     LENGTH     ][RESERVED][   BODY   ]
```

**消息2**: Type=0x0201 (P2P_MSG), Body=`{"content":"hello"}`
```
0x01 0x02 0x01 0x00 0x00 0x00 0x0F ...(15字节body)...
```

**接收端缓冲区状态**:
```
[包头1][13字节body][包头2][15字节body]
  12        13           12        15   = 53 字节
```

接收端循环处理：
1. 读取12字节 → 获取 Type=0x0201, Length=13
2. 读取13字节 → 完整消息1
3. 继续处理剩余数据 → 获取 Type=0x0201, Length=15
4. 读取15字节 → 完整消息2

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

message P2PMessage {
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
