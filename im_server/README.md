# IM Server - C++ 即时通讯服务端

基于 Boost.Asio 实现的高性能即时通讯服务器框架。

## 技术特性

- **C++17** - 现代 C++ 特性
- **Boost.Asio** - 异步 I/O 网络库
- **Reactor 模型** - 基于 epoll/kqueue/IOCP 的事件驱动
- **多线程** - 线程池处理业务逻辑
- **Session 管理** - 每个连接独立管理
- **粘包处理** - 固定头部 + 变长数据的协议设计

## 项目结构

```
im_server/
├── CMakeLists.txt              # CMake 构建配置
├── README.md                   # 本文件
└── src/
    ├── main.cpp                # 程序入口
    ├── server/
    │   ├── server.cpp          # TCP 服务器主类
    │   └── server.h
    ├── session/
    │   ├── session.cpp         # 客户端会话管理
    │   └── session.h
    ├── protocol/
    │   ├── message.h           # 消息类型定义
    │   ├── codec.cpp           # 粘包处理编解码器
    │   └── codec.h
    ├── thread/
    │   ├── thread_pool.cpp     # 线程池实现
    │   └── thread_pool.h
    └── dispatcher/
        ├── dispatcher.cpp      # 消息分发器
        └── dispatcher.h
```

## 消息协议

### 消息格式（解决粘包问题）

```
+------------------+------------------+------------------+
|  消息类型 (2B)   |   消息长度 (4B)   |   消息内容 (N)   |
+------------------+------------------+------------------+
     uint16_t           uint32_t          bytes[N]
```

- **头部固定 6 字节**（网络字节序/大端序）
- **类型**：2 字节，标识消息种类
- **长度**：4 字节，标识消息体长度
- **内容**：变长字节流（通常为 JSON 格式）

### 消息类型

| 类型 | 值 | 方向 | 说明 |
|------|-----|------|------|
| HEARTBEAT | 0x0001 | 双向 | 心跳包 |
| LOGIN | 0x0002 | C→S | 登录请求 |
| REGISTER_REQ | 0x0003 | C→S | 注册请求 |
| LOGOUT | 0x0004 | C→S | 登出请求 |
| CHAT_MESSAGE | 0x0005 | 双向 | 统一聊天消息，使用 content_type 区分 text/image/video/file/voice |
| FILE_UPLOAD_START / FILE_UPLOAD_CHUNK | 0x0019 / 0x001A | C→S | 文件分片上传，单文件最大 200MB |
| FILE_DOWNLOAD_REQ | 0x001B | C→S | 文件下载请求 |
| LOGIN_RSP | 0x8002 | S→C | 登录响应 |
| REGISTER_RSP | 0x8003 | S→C | 注册响应 |
| FILE_UPLOAD_RSP | 0x8019 | S→C | 文件上传进度/完成响应 |
| FILE_DOWNLOAD_RSP / FILE_DOWNLOAD_CHUNK | 0x801A / 0x801B | S→C | 文件下载响应和下载分片 |
| IMAGE/FILE/VOICE | 0x0006/0x0007/0x0008 | C→S | 旧媒体消息类型，仅兼容；新实现请用 CHAT_MESSAGE |
| ACK | 0x0009 | 双向 | 消息确认 |
| ERROR | 0x800F | S→C | 错误响应 |

## 编译

### 环境要求

- C++17 兼容编译器（GCC 7+ / Clang 5+ / MSVC 2019+）
- CMake 3.10+
- Boost.Asio（需要 Boost.System 和 Boost.Thread）

### Ubuntu / Debian

```bash
# 安装依赖
sudo apt install build-essential cmake libboost-all-dev

# 编译
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4

# 运行
./im_server 8080
```

### Windows

```batch
# 使用 Visual Studio Developer Command Prompt
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# 运行
Release\im_server.exe 8080
```

## 运行

```bash
# 基本用法
./im_server 8080

# 指定线程数
./im_server 8080 8
```

## 线程模型

```
┌─────────────────────────────────────────────────────────┐
│                    Main Reactor                          │
│  (epoll/IOCP) - 监听端口 accept 新连接                    │
└─────────────────────────┬───────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────┐
│              Session (per client connection)              │
│  - 读写缓冲区（boost::asio::streambuf）                 │
│  - 异步读写                                               │
│  - 心跳计时器                                            │
│  - 粘包处理                                               │
└─────────────────────────┬───────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────┐
│                    Thread Pool                           │
│  - 4-8 个工作线程（默认 CPU 核心数）                      │
│  - 业务逻辑处理                                          │
│  - 消息分发                                              │
└─────────────────────────────────────────────────────────┘
```

## 网络模型

采用 **Reactor 模型**（也称为事件驱动模型）：

1. **Main Reactor**：主线程运行 `io_context`，负责：
   - 监听端口
   - 接受新连接（`async_accept`）
   - 分发 I/O 事件

2. **Session Reactor**：每个 Session 独立处理：
   - 异步读取（`async_read_some`）
   - 异步写入（`async_write`）
   - 心跳超时检测

3. **Thread Pool**：业务逻辑处理：
   - 消息解析
   - 业务分发
   - 跨 Session 通信

## 粘包处理

### 问题

TCP 是流式协议，可能出现：
- **粘包**：多个消息合并为一个数据包
- **拆包**：一个消息分散到多个数据包

### 解决方案

固定头部 + 变长数据：

```
1. 读取时先读取 6 字节头部（类型 + 长度）
2. 根据长度再读取完整的消息体
3. 缓冲区保留未处理完的数据
```

### 示例

假设发送两条消息：
- 消息A: `{"type":"text","content":"hi"}`
- 消息B: `{"type":"image","url":"xxx.jpg"}`

发送顺序：
```
[TYPE][LENGTH][content...][TYPE][LENGTH][content...]
  2B      4B       N         2B      4B       N
```

接收端可以正确拆分为两条消息。

## 客户端示例

### Python

```python
import socket
import struct
import json

def send_msg(sock, msg_type, body):
    """发送消息"""
    body_bytes = body.encode('utf-8')
    # 头部：2字节类型 + 4字节长度
    header = struct.pack('>HI', msg_type, len(body_bytes))
    sock.sendall(header + body_bytes)

def recv_msg(sock):
    """接收消息"""
    # 先读取 6 字节头部
    header = b''
    while len(header) < 6:
        header += sock.recv(6 - len(header))
    msg_type, length = struct.unpack('>HI', header)

    # 读取消息体
    body = b''
    while len(body) < length:
        body += sock.recv(length - len(body))
    return msg_type, body.decode('utf-8')

# 连接服务器
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('127.0.0.1', 8080))

# 登录
login_req = json.dumps({'user_id': 'user_001'})
send_msg(sock, 0x0002, login_req)
msg_type, resp = recv_msg(sock)
print(f"登录响应: {resp}")

# 发送文本消息
text_msg = json.dumps({
    'from_user_id': 'user_001',
    'to_user_id': 'user_002',
    'content': 'Hello!'
})
send_msg(sock, 0x0004, text_msg)

# 心跳
send_msg(sock, 0x0001, '{}')

sock.close()
```

## 扩展建议

1. **数据库**：添加 SQLite/MySQL 存储用户和消息
2. **集群**：使用 Redis 发布/订阅实现跨服务器通信
3. **负载均衡**：Nginx/HAProxy 做负载均衡
4. **安全**：TLS/SSL 加密，JWT 认证
5. **协议**：使用 Protobuf 替代 JSON 提高性能

## License

MIT
