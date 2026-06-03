# IChat Agents Guide

## 项目定位

IChat 是一个跨端即时通讯项目，服务端为 C++17 + Boost.Asio + MySQL，桌面端为 Qt6 Widgets Windows 客户端，移动端为 Kotlin + Jetpack Compose Android 客户端。当前 Windows 端是功能最完整的参考实现，Android 端正在按 Windows 端能力做同步迁移。

已实现的核心能力包括登录、注册、好友列表、好友请求、单聊/群聊文本消息、离线消息、聊天记录、联系人备注、头像同步、个人资料、修改密码、朋友圈图文、文件分片上传下载和一对一音视频通话信令。Android 端已落地主要 IM 骨架，视频消息播放和音视频通话媒体管线仍待补齐。

## 目录总览

- `im_client/`: Qt6 Widgets Windows 客户端。
- `im_client/include/`: Windows 客户端协议、窗口、TCP 客户端和通话桥接声明。
- `im_client/src/`: Windows 客户端实现，`mainwindow_*.cpp` 按页面拆分。
- `im_client/web/`: 浏览器 WebRTC 通话页面，Windows 端通过本地 HTTP bridge 打开。
- `im_android/`: Kotlin + Jetpack Compose Android 客户端。
- `im_android/app/src/main/java/com/ichat/android/data/`: Android 数据、Room、本地存储、TCP 协议和 Repository。
- `im_android/app/src/main/java/com/ichat/android/ui/`: Android 登录、消息、联系人、朋友圈、我的页面和通用 Compose UI。
- `im_server/`: Boost.Asio + MySQL 服务端。
- `im_server/src/protocol/`: 6 字节固定包头和消息类型定义。
- `im_server/src/dispatcher/`: 服务端消息类型到业务处理函数的分发。
- `im_server/src/session/`: 单个 TCP 连接的读写、粘包处理和在线会话管理。
- `im_server/src/db/`: 数据库连接池和 `UserService`，封装用户、好友、群组、消息、朋友圈和文件业务。
- `im_server/sql/`: 初始化和迁移 SQL。
- `im_server/docs/`: 协议和数据库设计文档。
- `docs/images/`: README 使用的 Windows 端功能截图。

## Windows 客户端窗口拆分

`MainWindow` 是 Windows 客户端主窗口协调者，持有登录用户、当前会话、联系人备注、会话列表、资料缓存和通话状态等共享状态。不要在页面文件之间复制这些状态。

- `im_client/src/mainwindow.cpp`: 主窗口构造、左侧导航栏、全局信号连接、页面切换和登出。
- `im_client/src/mainwindow_messages.cpp`: 消息列表、聊天面板、消息发送/接收、聊天历史、离线消息、会话排序、气泡渲染、附件打开/下载和发送状态。
- `im_client/src/mainwindow_contacts.cpp`: 联系人页、好友请求弹窗、群聊创建、联系人双击进入聊天、好友/群列表刷新和联系人备注结果。
- `im_client/src/mainwindow_me.cpp`: “我”页、个人信息、头像选择/压缩/同步、账号设置和相关状态提示。
- `im_client/src/mainwindow_moments.cpp`: 朋友圈时间流、图文发布、图片预览和个人朋友圈入口。
- `im_client/src/mainwindow_calls.cpp`: 一对一音视频通话 UI、通话信令、浏览器 WebRTC 页面桥接。
- `im_client/include/mainwindow_helpers.h`: 主窗口页面共享的小型 UI 工具，例如导航图标、聊天气泡、会话列表项、头像预览和时间解析。

## Android 客户端拆分

Android 端按“单 Activity + 多 Fragment + ComposeView”组织，底部导航对应 Windows 端左侧导航。迁移 Windows 功能时，优先保持协议字段、状态流和用户体验语义一致，不要把页面状态散落到 Fragment。

- `MainActivity`: 承载登录态切换、底部导航、通知点击进入会话和 Fragment 容器。
- `AppContainer`: 创建进程级依赖，包括 Room、Preferences、AttachmentStore、SocketClient、通知管理和 Repository。
- `IChatRepository`: Android 业务中枢，集中处理 TCP、Room、账号态、后台通知、文件下载目录、朋友圈和资料同步。
- `IChatSocketClient`: 进程级 TCP 长连接、读循环、发送互斥、心跳和连接状态。
- `ProtocolCodec.kt` / `MsgType.kt`: Android 协议编解码与消息类型表，必须与 C++ 两端完全一致。
- `IChatDatabase.kt` / `Entities.kt` / `Daos.kt`: Room 本地库。所有会话、消息、好友和群组缓存必须带 `ownerUserId`，避免账号切换后串数据。
- `PreferencesStore`: 保存登录用户资料、token 和服务端地址；退出登录只清账号态，不重置服务端地址。
- `AttachmentStore`: Android 下载文件统一进入应用专属下载目录，不弹系统保存位置选择。
- `ui/auth`: 登录/注册。
- `ui/chat`: 会话列表、聊天面板、历史分页、单聊/群聊文本、媒体/文件卡片展示。
- `ui/contacts`: 好友、群聊、好友请求和联系人索引。
- `ui/profile`: 头像资料页、资料拉取、发起聊天和查看朋友圈入口。
- `ui/moments`: 朋友圈时间流、图文发布、图片预览。
- `ui/me`: 当前用户资料、头像更新、修改密码和退出登录。
- `ui/common`: 头像、页面顶部栏、搜索框、添加好友和建群弹窗等复用组件。

## 协议约定

客户端 `im_client/include/protocol.h`、Android `im_android/app/src/main/java/com/ichat/android/data/network/MsgType.kt`、服务端 `im_server/src/protocol/message.h` 和 `im_server/docs/PROTOCOL.md` 必须保持消息类型一致。

当前协议包头固定为 6 字节：2 字节类型 + 4 字节长度，均为大端序；消息体为 UTF-8 JSON。不要静默改变包头格式。

新增普通聊天业务时优先复用 `CHAT_MESSAGE`，通过 `content_type` 区分 `text`、`image`、`voice`、`video`、`file`，通过 `chat_type` 区分 `p2p` 和 `group`。不要再为普通媒体消息新增包头类型，`IMAGE`、`FILE`、`VOICE` 只作为旧兼容类型保留。

## 同步开发边界

- Windows 端是 Android 同步的行为参考，但 Android 端应使用本地平台范式：Repository + ViewModel + StateFlow + Compose UI + Room。
- Android 页面只和 ViewModel/Repository 交互，不直接操作 Socket、Room 或通知。
- Android 长耗时任务必须放到协程 IO/Default 调度器，例如文件复制、分片上传下载、头像/朋友圈图片压缩。
- Android 冷启动可先展示本地 token 对应的用户资料，再用 token 登录恢复服务端在线会话；token 失效时清账号态和对应用户缓存。
- Android 进程仍存活但 App 在后台时，收到聊天消息由 `ChatNotificationManager` 发系统通知；点击通知进入对应联系人或群聊。
- 账号切换、退出登录、重连和旧连接回包要小心处理，避免旧账号消息、旧 token 或旧 socket 回包污染当前用户。

## 功能扩展点

- 图片/视频/文件发送：Windows 端已支持分片上传下载和图片/视频卡片；Android 端已有文件上传下载入口，图片按 `content_type=image` 发送，视频消息当前仍显式暂不实现。补 Android 视频时继续走 `CHAT_MESSAGE` + 文件元数据，不要把大文件塞进 JSON。
- 语音通话/视频通话：TCP 协议只承载呼叫信令，例如邀请、接听、拒绝、取消、挂断、offer/answer/ICE；实时音视频流应走独立通道，优先复用 WebRTC，不要通过聊天消息通道传原始音视频流。
- 朋友圈：服务端已有朋友圈表和独立消息类型；Windows 与 Android 都从各自朋友圈页面维护独立状态，不要复用单聊消息表表达朋友圈。
- 群聊：沿用 `CHAT_MESSAGE`，通过 `chat_type=group` 和 `to_user_id=group_id` 区分；服务端已有群组表和成员转发逻辑。

## 开发守则

- 一定要给代码写好注释，复杂状态机、协议兼容、账号隔离和异步流程尤其要说明原因。
- 改协议时同步更新 Windows 协议头、Android `MsgType.kt`、服务端协议头和 `im_server/docs/PROTOCOL.md`。
- 改数据库结构时新增 SQL 迁移，并更新 `im_server/docs/DATABASE.md`。
- 新增 Windows 客户端源文件后同步更新 `im_client/CMakeLists.txt` 的 `SOURCES` 或 `HEADERS`。
- 新增 Android 源文件放在现有包结构下；新增依赖时同步更新 `im_android/app/build.gradle.kts`，不要绕过 Gradle。
- Qt UI 逻辑保持在主线程，网络 I/O 继续通过 `TcpClient` 信号槽进入窗口层。
- Android UI 逻辑保持在主线程，网络和数据库继续通过 Repository/ViewModel 的协程进入页面层。
- 服务端业务入口优先放在 dispatcher 注册的处理器中，数据库读写封装在 `UserService` 或后续拆出的领域服务里。

## 构建和验证

Windows 客户端：

不需要构建 Windows 客户端，除非本次任务明确要求。

Android 客户端：

```powershell
cd im_android
.\gradlew.bat :app:assembleDebug
```

如果本机缺少匹配的 Android Studio、JDK 17、Android Gradle Plugin 或 SDK，说明环境限制即可。

服务端：

```bash
cmake -S im_server -B im_server/build
cmake --build im_server/build
```
