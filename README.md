# IChat

IChat 是一个基于 C++17 的桌面即时通讯系统，包含 Qt6 客户端和 Boost.Asio 服务端。项目实现了账号体系、好友关系、单聊/群聊、离线消息、聊天记录、图片/视频/文件传输、朋友圈以及一对一音视频通话信令等核心 IM 能力。

服务端使用自定义 TCP 协议承载业务消息，客户端使用 Qt Widgets 构建桌面界面；音视频通话采用 WebRTC 方案，服务端负责通话信令转发，媒体流由浏览器 WebRTC 和 STUN/TURN 网络服务完成。

## 项目功能

### 账号与个人资料

- 用户注册、登录、登出
- 自动保存登录凭证
- 修改昵称、性别、地区、个性签名
- 本地选择头像并压缩为 data URL 后同步到服务端
- 查看自己和好友的个人资料

### 好友与联系人

- 添加好友、处理好友请求
- 获取好友列表
- 删除好友
- 修改好友备注
- 联系人资料弹窗和快捷发起聊天

### 即时消息

- 单聊文本消息
- 群聊消息
- 消息发送 ACK 回执
- 离线消息拉取与确认
- 历史消息查询
- 会话列表按最后消息时间排序
- 文本、图片、视频、文件等不同消息气泡展示

### 媒体与文件传输

- 图片、视频和普通文件发送
- 文件分片上传，避免大文件直接塞入聊天 JSON
- 文件分片下载
- 下载链路支持发送背压：服务端按分片写入完成后再发送下一片，避免一次性堆积大量发送队列
- 图片预览和视频封面展示
- 本地附件路径索引，已下载文件可直接打开

### 群聊

- 创建群聊
- 获取群聊列表
- 群成员消息转发
- 群离线消息保存

### 朋友圈

- 发布文字和图片动态
- 获取朋友圈时间流
- 查看指定用户动态
- 图片预览

### 音视频通话

- 一对一视频通话邀请、接听、拒绝、取消、挂断和超时
- WebRTC offer / answer / ICE 信令转发
- Qt 客户端启动本地通话页面，使用系统浏览器承载真实音视频通话
- 支持 STUN/TURN 配置，适配公网或复杂 NAT 环境

## 成果展示


### 登录与注册

![登录与注册](docs/images/login.png)

### 主界面与会话列表

![主界面与会话列表](docs/images/main-window.png)

### 单聊与媒体消息

![单聊与媒体消息](docs/images/chat-media.png)

### 文件/视频传输

![文件和视频传输](docs/images/file-transfer.png)

### 群聊

![群聊](docs/images/group-chat.png)

### 朋友圈

![朋友圈](docs/images/moments.png)

### 视频通话

![视频通话](docs/images/video-call.png)

## 技术架构

```text
IChat
├── im_client/                Qt6 Widgets 桌面客户端
│   ├── src/tcpclient.cpp     TCP 客户端、协议收发、文件上传下载、通话信令
│   ├── src/mainwindow*.cpp   主界面、消息页、联系人页、朋友圈页、我的页面
│   └── web/call.html         浏览器 WebRTC 通话页面
│
├── im_server/                C++17 Boost.Asio 服务端
│   ├── src/session/          TCP 会话、异步读写、心跳和粘包处理
│   ├── src/protocol/         6 字节包头协议编解码
│   ├── src/dispatcher/       消息类型分发
│   ├── src/db/               MySQL 连接池和业务数据访问
│   ├── src/server/           服务端主逻辑、聊天/文件/通话处理
│   └── sql/                  数据库初始化和迁移脚本
│
└── im_server/docs/           协议和数据库设计文档
```

## 核心协议

客户端和服务端之间使用自定义 TCP 协议。每条消息由固定 6 字节包头和变长 JSON 消息体组成：

```text
+------------------+------------------+------------------+
|  消息类型 2B      |   消息长度 4B     |   消息体 N bytes  |
+------------------+------------------+------------------+
```

- 消息类型使用 `uint16_t`
- 消息长度使用 `uint32_t`
- 多字节字段使用大端序
- 消息体通常为 UTF-8 JSON
- `CHAT_MESSAGE` 作为统一聊天消息，使用 `content_type` 区分 `text`、`image`、`video`、`file`、`voice`

详细协议见 [im_server/docs/PROTOCOL.md](im_server/docs/PROTOCOL.md)。

## 数据存储

服务端使用 MySQL 保存用户、好友、群聊、消息、离线消息、朋友圈等数据。媒体文件本体保存在服务端本地 `storage/files` 目录，聊天消息只保存文件元数据，例如 `file_id`、`file_name`、`file_size`、`mime_type` 和预览信息。

数据库设计见 [im_server/docs/DATABASE.md](im_server/docs/DATABASE.md)。

## 服务端构建与运行

### 环境要求

- C++17 编译器
- CMake 3.10+
- Boost.System / Boost.Thread / Boost.JSON
- MySQL client library
- MySQL Server

### 编译

```bash
cmake -S im_server -B im_server/build -DCMAKE_BUILD_TYPE=Release
cmake --build im_server/build --target im_server -j$(nproc)
```

### 初始化数据库

```bash
mysql -u root -p < im_server/sql/init_database.sql
```

服务端默认数据库连接参数位于 [im_server/src/main.cpp](im_server/src/main.cpp)。

### 启动

```bash
./im_server/build/im_server 8080
```

也可以指定线程数：

```bash
./im_server/build/im_server 8080 4
```



## 音视频通话部署说明

`im_server` 只负责 WebRTC 信令，不承载音视频媒体流。公网通话建议部署 coturn：

- 开放 `TCP/UDP 3478`
- 开放 TURN relay UDP 端口范围
- 客户端配置 `ICHAT_TURN_HOST`、`ICHAT_TURN_USER`、`ICHAT_TURN_PASSWORD`

## 项目亮点

- 自定义 TCP 协议，处理粘包和半包
- Boost.Asio 异步网络模型
- Session 级读写队列和心跳管理
- 文件分片上传/下载，支持大文件传输
- 下载发送链路具备背压控制，降低服务端内存峰值
- 聊天、好友、群聊、朋友圈、音视频通话信令形成完整 IM 闭环
- Qt 客户端界面按消息、联系人、朋友圈、我的页面拆分实现

## 目录说明

```text
.
├── Agents.md                 项目开发说明
├── im_client/                Qt6 客户端
├── im_server/                Boost.Asio 服务端
├── cmake/                    交叉编译相关 CMake 配置
└── README.md                 项目总览
```

## 后续可扩展方向

- 文件存储接入对象存储或 CDN
- 群聊成员管理、群公告、群头像设置
- 朋友圈点赞、评论、可见范围
- 消息撤回、已读状态、多端同步
- 客户端文件传输任务队列和断点续传
- 服务端配置文件化，移除硬编码数据库参数

