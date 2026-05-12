# IChat Agents Guide

## 项目定位

IChat 是一个 C++17 即时通讯项目，分为 Qt6 客户端和 Boost.Asio 服务端。当前已实现登录、注册、好友列表、好友请求、单聊文本消息、离线消息、聊天记录、联系人备注、头像同步和修改密码。

## 目录总览

- `im_client/`: Qt6 Widgets 客户端。
- `im_client/include/`: 客户端头文件，协议、窗口和 TCP 客户端声明在这里。
- `im_client/src/`: 客户端实现。
- `im_server/`: Boost.Asio + MySQL 服务端。
- `im_server/src/protocol/`: 6 字节固定包头和消息类型定义。
- `im_server/src/dispatcher/`: 服务端消息类型到业务处理函数的分发。
- `im_server/src/session/`: 单个 TCP 连接的读写、粘包处理和在线会话管理。
- `im_server/src/db/`: 数据库连接池和用户/好友/消息业务。
- `im_server/sql/`: 初始化和迁移 SQL。
- `im_server/docs/`: 协议和数据库设计文档。

## 客户端窗口拆分

`MainWindow` 仍然是主窗口协调者，持有登录用户、当前会话、联系人备注、会话列表等共享状态。不要在页面文件之间复制这些状态。

- `im_client/src/mainwindow.cpp`: 主窗口构造、左侧导航栏、全局信号连接、页面切换和登出。
- `im_client/src/mainwindow_messages.cpp`: 消息列表、聊天面板、消息发送/接收、聊天历史、离线消息、会话排序、气泡渲染和发送状态。
- `im_client/src/mainwindow_contacts.cpp`: 联系人页、好友请求弹窗、联系人双击进入聊天、好友列表刷新和联系人备注结果。
- `im_client/src/mainwindow_settings.cpp`: 设置页、头像选择/压缩/同步、密码修改和相关状态提示。
- `im_client/src/mainwindow_moments.cpp`: 朋友圈页入口，目前是占位页，后续朋友圈功能从这里扩展。
- `im_client/include/mainwindow_helpers.h`: 主窗口页面共享的小型 UI 工具，例如导航图标、聊天气泡、会话列表项、头像预览和时间解析。

## 协议约定

客户端 `im_client/include/protocol.h` 和服务端 `im_server/src/protocol/message.h` 必须保持消息类型一致。当前协议包头固定为 6 字节：2 字节类型 + 4 字节长度，消息体为 UTF-8 JSON。

新增业务时优先复用 `CHAT_MESSAGE`，通过 `content_type` 区分 `text`、`image`、`voice`、`video`、`file`。不要再为普通媒体消息新增包头类型，`IMAGE`、`FILE`、`VOICE` 只作为旧兼容类型保留。

## 后续功能扩展点

- 图片/视频/文件发送：客户端先扩展 `mainwindow_messages.cpp` 的输入区和消息渲染，根据 `content_type` 显示缩略图、文件卡片或视频卡片；服务端在 `UserService::save_message` 和数据库消息表中保存媒体元数据。大文件不要直接塞进 JSON，建议新增上传/分片或对象存储 URL。
- 语音通话/视频通话：TCP 协议只承载呼叫信令，例如邀请、接听、拒绝、挂断、offer/answer/ICE；实时音视频流应走独立通道，优先考虑 WebRTC 或 UDP/RTP，不要通过聊天消息通道传原始音视频流。
- 朋友圈：客户端从 `mainwindow_moments.cpp` 新增独立页面状态；服务端建议新增动态、评论、点赞、可见范围等表和独立消息类型，不要复用单聊消息表表达朋友圈。
- 群聊：沿用 `CHAT_MESSAGE`，通过 `chat_type`、`group_id` 区分群聊；服务端已有数据库设计文档中的群组表规划。

## 开发守则

- 一定要给代码写好注释
- 改协议时同步更新客户端协议头、服务端协议头和 `im_server/docs/PROTOCOL.md`。
- 改数据库结构时新增 SQL 迁移，并更新 `im_server/docs/DATABASE.md`。
- 新增客户端源文件后同步更新 `im_client/CMakeLists.txt` 的 `SOURCES` 或 `HEADERS`。
- Qt UI 逻辑保持在主线程，网络 I/O 继续通过 `TcpClient` 信号槽进入窗口层。
- 长耗时任务不要阻塞 UI，例如大文件读取、缩略图生成、上传进度和音视频初始化都应异步化。
- 服务端业务入口优先放在 dispatcher 注册的处理器中，数据库读写封装在 `UserService` 或后续拆出的领域服务里。

## 构建和验证

客户端：

```bash
cmake -S im_client -B im_client/build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-toolchain.cmake
cmake --build im_client/build
```

服务端：

```bash
cmake -S im_server -B im_server/build
cmake --build im_server/build
```

当前仓库的客户端构建依赖 `/home/xf/Qt6Windows/6.5.3/mingw64`。如果 CMake 报 `Qt6::moc` 或 `moc.exe` 不存在，先修复 Qt 工具链路径，再判断代码问题。
