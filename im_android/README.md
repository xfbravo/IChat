# IChat Android

Android 客户端采用 Kotlin + Jetpack Compose，当前工程按“单 Activity + 多 Fragment”组织：

- `MainActivity` 承载所有页面，底部导航替代 Windows 端左侧导航。
- 每个 Fragment 内使用 `ComposeView` 渲染 UI。
- 页面状态放在 ViewModel，进程级网络和通知逻辑放在 `IChatRepository`。
- TCP 协议与现有服务端保持一致：6 字节大端包头 + UTF-8 JSON body。
- Room 本地库保存会话、好友、群聊和聊天消息，媒体消息保留 `content_type` 与原始 `content` JSON。
- 文件默认下载到应用专属下载目录 `Android/data/com.ichat.android/files/Download/IChat`，不再弹出保存位置选择。

## 已落地的骨架能力

- 登录/注册请求。
- 已登录用户冷启动自动恢复：保存上次登录返回的 token，只有在“我”页退出登录后才回到登录页。
- 消息、联系人、朋友圈、“我”四个底部 Tab。
- 单聊/群聊文本消息收发和本地落库。
- 离线消息落库与 ACK。
- 好友/群聊列表同步到 Room。
- 文件上传/下载的 IO 线程入口和默认下载目录。
- App 退到后台但进程仍存活时，收到聊天消息会发系统通知；点击通知会进入对应联系人或群聊聊天界面。

## 暂不实现

- 视频发送、视频播放和视频通话媒体管线。
- 图片预览压缩、朋友圈完整发布/时间流 UI 和音视频通话 UI 后续继续补齐。

## 构建要求

当前配置使用 Android Gradle Plugin 9.2.0、Gradle Wrapper 9.4.1、Kotlin 2.3.21、Compose BOM 2026.05.00。
需要 Android Studio/Gradle 对应环境，并使用 JDK 17。
