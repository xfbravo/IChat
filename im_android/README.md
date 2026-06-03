# IChat Android

Android 客户端采用 Kotlin + Jetpack Compose，当前工程按“单 Activity + 多 Fragment + ComposeView”组织。它不是独立协议分支，而是在移动端同步 Windows 客户端能力，并复用同一套 Boost.Asio 服务端协议。

## 架构约定

- `MainActivity` 承载所有页面，底部导航替代 Windows 端左侧导航。
- 每个 Fragment 内使用 `ComposeView` 渲染 UI。
- 页面状态放在 ViewModel，进程级网络、Room、通知、文件目录和账号态放在 `IChatRepository`。
- TCP 协议与现有服务端保持一致：6 字节大端包头 + UTF-8 JSON body。
- `MsgType.kt` 必须和 Windows `im_client/include/protocol.h`、服务端 `im_server/src/protocol/message.h` 保持一致。
- Room 本地库保存会话、好友、群聊和聊天消息，所有本地缓存都按 `ownerUserId` 隔离。
- 媒体消息保留 `content_type` 与原始 `content` JSON，不为普通媒体新增包头类型。
- 文件默认下载到应用专属下载目录 `Android/data/com.ichat.android/files/Download/IChat`，不再弹出保存位置选择。

## 已同步能力

- 登录/注册请求。
- 已登录用户冷启动自动恢复：保存上次登录返回的 token，只有在“我”页退出登录后才回到登录页。
- 消息、联系人、朋友圈、“我”四个底部 Tab。
- 单聊/群聊文本消息收发和本地落库。
- 聊天历史分页拉取。
- 离线消息落库与 ACK。
- 好友/群聊列表同步到 Room。
- 好友请求发送、处理和列表刷新。
- 联系人/群资料页，支持从资料页进入聊天或个人朋友圈。
- 当前用户资料编辑、头像选择压缩和同步。
- 朋友圈图文发布、时间流拉取、个人朋友圈入口和图片预览。
- 文件上传/下载的 IO 线程入口和默认下载目录。
- App 退到后台但进程仍存活时，收到聊天消息会发系统通知；点击通知进入对应联系人或群聊聊天界面。

## 暂未补齐

- 视频消息发送、下载后播放体验和视频封面处理。
- 一对一音视频通话媒体管线和 Android 通话 UI。
- 更完整的文件传输任务进度、失败重试和断点续传体验。

## 构建要求

当前配置使用 Android Gradle Plugin 9.1.1、Gradle Wrapper 9.3.1、Kotlin 2.3.21、Compose BOM 2026.05.00。
需要 Android Studio/Gradle 对应环境，并使用 JDK 17。

```powershell
.\gradlew.bat :app:assembleDebug
```
