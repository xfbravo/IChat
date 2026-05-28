package com.ichat.android.data.repository

import android.content.Context
import android.net.Uri
import android.util.Base64
import com.ichat.android.data.db.ChatMessageEntity
import com.ichat.android.data.db.ConversationEntity
import com.ichat.android.data.db.FriendEntity
import com.ichat.android.data.db.GroupEntity
import com.ichat.android.data.db.IChatDatabase
import com.ichat.android.data.model.ChatTarget
import com.ichat.android.data.model.CurrentUser
import com.ichat.android.data.model.LoginResult
import com.ichat.android.data.model.MessageDraft
import com.ichat.android.data.network.ConnectionState
import com.ichat.android.data.network.IChatSocketClient
import com.ichat.android.data.network.ImPacket
import com.ichat.android.data.network.MsgType
import com.ichat.android.data.network.ProtocolCodec
import com.ichat.android.data.storage.AttachmentStore
import com.ichat.android.data.storage.PreferencesStore
import com.ichat.android.notification.ChatNotificationManager
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.flow.filter
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeout
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.net.URLConnection
import java.util.concurrent.ConcurrentHashMap

/**
 * Android 端的业务中枢。
 *
 * UI 只和 Repository/ViewModel 交互；TCP、Room、默认下载目录、后台通知都收拢在这里，
 * 这样 Activity/Fragment 销毁后，只要应用进程还在，Socket 读循环和通知逻辑仍然存在。
 */
class IChatRepository(
    private val context: Context,
    private val appScope: CoroutineScope,
    private val database: IChatDatabase,
    private val preferences: PreferencesStore,
    private val attachmentStore: AttachmentStore,
    private val socketClient: IChatSocketClient,
    private val notificationManager: ChatNotificationManager
) {
    private val messageDao = database.messageDao()
    private val conversationDao = database.conversationDao()
    private val contactDao = database.contactDao()

    private val savedUser = preferences.loadUser()?.takeIf { it.token.isNotBlank() }

    // 用户主动退出前保留上次登录态；冷启动先显示本地资料，再用 token 恢复服务端在线会话。
    private val _currentUser = MutableStateFlow<CurrentUser?>(savedUser)
    val currentUser: StateFlow<CurrentUser?> = _currentUser.asStateFlow()

    private val _isAppForeground = MutableStateFlow(true)
    private val _activeConversationKey = MutableStateFlow<String?>(null)

    private var loginSequence = 0
    private var pendingLogin: PendingLogin? = null
    private var pendingRegister: CompletableDeferred<LoginResult>? = null
    private val uploadResponses = kotlinx.coroutines.flow.MutableSharedFlow<FileUploadResponse>(extraBufferCapacity = 32)
    private val pendingDownloads = ConcurrentHashMap<String, PendingDownload>()

    val connectionState: StateFlow<ConnectionState> = socketClient.state

    init {
        appScope.launch {
            socketClient.incoming.collect { packet -> handlePacket(packet) }
        }
        savedUser?.let { user ->
            appScope.launch { restoreSavedLogin(user) }
        }
    }

    fun setAppForeground(foreground: Boolean) {
        _isAppForeground.value = foreground
    }

    fun observeConversations(): Flow<List<ConversationEntity>> = conversationDao.observeConversations()

    fun observeFriends(): Flow<List<FriendEntity>> = contactDao.observeFriends()

    fun observeGroups(): Flow<List<GroupEntity>> = contactDao.observeGroups()

    fun observeMessages(target: ChatTarget): Flow<List<ChatMessageEntity>> {
        return messageDao.observeMessages(target.conversationKey)
    }

    suspend fun openConversation(target: ChatTarget) {
        _activeConversationKey.value = target.conversationKey
        conversationDao.clearUnread(target.conversationKey)
        requestChatHistory(target.peerId, target.chatType)
    }

    suspend fun login(userId: String, password: String): LoginResult {
        return performLogin(
            JSONObject()
                .put("user_id", userId)
                .put("password", password)
        )
    }

    private suspend fun restoreSavedLogin(user: CurrentUser) {
        val result = runCatching {
            loginWithToken(user.userId, user.token)
        }.getOrElse {
            // 网络不可用时保留本地登录态，用户仍可查看本地会话，后续操作按连接状态反馈。
            return
        }
        if (result.code != 0) {
            preferences.clearUser()
            socketClient.disconnect()
            _currentUser.value = null
        }
    }

    private suspend fun loginWithToken(userId: String, token: String): LoginResult {
        return performLogin(
            JSONObject()
                .put("user_id", userId)
                .put("token", token)
        )
    }

    private suspend fun performLogin(payload: JSONObject): LoginResult {
        ensureConnected()
        val waiter = CompletableDeferred<LoginResult>()
        pendingLogin = PendingLogin(loginSequence, waiter)
        socketClient.sendJson(MsgType.LOGIN, payload)
        return withTimeout(10_000) { waiter.await() }
    }

    suspend fun register(phone: String, nickname: String, password: String): LoginResult {
        ensureConnected()
        val waiter = CompletableDeferred<LoginResult>()
        pendingRegister = waiter
        socketClient.sendJson(
            MsgType.REGISTER_REQ,
            JSONObject()
                .put("phone", phone)
                .put("nickname", nickname)
                .put("password", password)
        )
        return withTimeout(10_000) { waiter.await() }
    }

    suspend fun logout() {
        loginSequence += 1
        pendingLogin?.waiter?.complete(LoginResult(499, "已退出登录"))
        pendingLogin = null
        runCatching { socketClient.send(MsgType.LOGOUT, "{}") }
        socketClient.disconnect()
        preferences.clearUser()
        _currentUser.value = null
    }

    suspend fun refreshContacts() {
        if (_currentUser.value == null) return
        socketClient.send(MsgType.GET_FRIEND_LIST, "{}")
        socketClient.send(MsgType.GET_GROUP_LIST, "{}")
    }

    suspend fun sendTextMessage(draft: MessageDraft) {
        sendChatMessage(
            peerId = draft.peerId,
            chatType = draft.chatType,
            contentType = "text",
            content = draft.content,
            localPath = null
        )
    }

    suspend fun sendFriendRequest(toUserIdOrPhone: String, remark: String) {
        socketClient.sendJson(
            MsgType.FRIEND_REQUEST,
            JSONObject()
                .put("phone", toUserIdOrPhone)
                .put("remark", remark)
        )
    }

    suspend fun respondFriendRequest(requestId: String, accept: Boolean) {
        socketClient.sendJson(
            MsgType.FRIEND_REQUEST_RSP,
            JSONObject()
                .put("request_id", requestId)
                .put("accept", accept)
        )
    }

    suspend fun updateFriendRemark(friendId: String, remark: String) {
        socketClient.sendJson(
            MsgType.UPDATE_FRIEND_REMARK,
            JSONObject()
                .put("friend_id", friendId)
                .put("remark", remark)
        )
    }

    suspend fun updateProfile(nickname: String, gender: String, region: String, signature: String) {
        socketClient.sendJson(
            MsgType.UPDATE_PROFILE,
            JSONObject()
                .put("nickname", nickname)
                .put("gender", gender)
                .put("region", region)
                .put("signature", signature)
        )
    }

    suspend fun changePassword(oldPassword: String, newPassword: String) {
        socketClient.sendJson(
            MsgType.CHANGE_PASSWORD,
            JSONObject()
                .put("old_password", oldPassword)
                .put("new_password", newPassword)
        )
    }

    suspend fun downloadFile(fileId: String, fileName: String): File = withContext(Dispatchers.IO) {
        val transferId = ProtocolCodec.generateMsgId()
        val target = attachmentStore.reserveDownloadFile(fileId, fileName)
        pendingDownloads[transferId] = PendingDownload(transferId, fileId, fileName, target)
        socketClient.sendJson(
            MsgType.FILE_DOWNLOAD_REQ,
            JSONObject()
                .put("transfer_id", transferId)
                .put("file_id", fileId)
                .put("file_name", fileName)
        )
        target
    }

    suspend fun uploadFile(peerId: String, chatType: String, uri: Uri) = withContext(Dispatchers.IO) {
        val fileName = uri.lastPathSegment?.substringAfterLast('/')?.ifBlank { "file" } ?: "file"
        val mimeType = context.contentResolver.getType(uri)
            ?: URLConnection.guessContentTypeFromName(fileName)
            ?: "application/octet-stream"
        if (mimeType.startsWith("video/")) {
            error("视频消息暂不实现")
        }

        val transferId = ProtocolCodec.generateMsgId()
        val tempFile = File(context.cacheDir, "ichat_upload_$transferId.tmp")
        context.contentResolver.openInputStream(uri)?.use { input ->
            FileOutputStream(tempFile).use { output -> input.copyTo(output) }
        } ?: error("无法读取文件")

        val fileSize = tempFile.length()
        if (fileSize <= 0) error("不能上传空文件")
        if (fileSize > 200L * 1024L * 1024L) error("单个文件不能超过200MB")

        val totalChunks = ((fileSize + ChunkSize - 1) / ChunkSize).toInt()
        val readyDeferred = CompletableDeferred<FileUploadResponse>()
        val readyWaiter = appScope.launch {
            uploadResponses
                .filter { it.transferId == transferId && (it.status == "ready" || it.status == "failed") }
                .first()
                .also { readyDeferred.complete(it) }
        }

        socketClient.sendJson(
            MsgType.FILE_UPLOAD_START,
            JSONObject()
                .put("transfer_id", transferId)
                .put("to_user_id", peerId)
                .put("chat_type", chatType)
                .put("file_name", fileName)
                .put("file_size", fileSize)
                .put("mime_type", mimeType)
                .put("total_chunks", totalChunks)
        )
        val ready = withTimeout(15_000) { readyDeferred.await() }
        readyWaiter.cancel()
        if (ready.code != 0) error(ready.message.ifBlank { "服务器拒绝上传" })

        FileInputStream(tempFile).use { input ->
            val buffer = ByteArray(ChunkSize)
            for (index in 0 until totalChunks) {
                val read = input.read(buffer)
                if (read <= 0) error("读取文件分片失败")
                val chunk = if (read == buffer.size) buffer else buffer.copyOf(read)
                socketClient.sendJson(
                    MsgType.FILE_UPLOAD_CHUNK,
                    JSONObject()
                        .put("transfer_id", transferId)
                        .put("chunk_index", index)
                        .put("data", Base64.encodeToString(chunk, Base64.NO_WRAP))
                )
            }
        }
        tempFile.delete()

        val complete = withTimeout(30_000) {
            uploadResponses
                .filter { it.transferId == transferId && it.status == "complete" }
                .first()
        }
        val contentType = if (mimeType.startsWith("image/")) "image" else "file"
        val content = JSONObject()
            .put("file_id", complete.fileId)
            .put("file_name", complete.fileName.ifBlank { fileName })
            .put("file_size", complete.fileSize)
            .put("mime_type", complete.mimeType.ifBlank { mimeType })
            .put("transfer_id", transferId)
            .toString()
        sendChatMessage(peerId, chatType, contentType, content, localPath = null)
    }

    private suspend fun requestChatHistory(peerId: String, chatType: String) {
        if (_currentUser.value == null) return
        socketClient.sendJson(
            MsgType.GET_CHAT_HISTORY,
            JSONObject()
                .put("peer_id", peerId)
                .put("chat_type", chatType)
                .put("limit", 30)
        )
    }

    private suspend fun sendChatMessage(
        peerId: String,
        chatType: String,
        contentType: String,
        content: String,
        localPath: String?
    ) {
        val user = _currentUser.value ?: return
        val msgId = ProtocolCodec.generateMsgId()
        val nowSeconds = System.currentTimeMillis() / 1000
        val nowMillis = System.currentTimeMillis()
        val key = "$chatType:$peerId"
        val title = conversationTitle(peerId, chatType)
        val message = ChatMessageEntity(
            msgId = msgId,
            conversationKey = key,
            peerId = peerId,
            chatType = chatType,
            fromUserId = user.userId,
            toUserId = peerId,
            contentType = contentType,
            content = content,
            localPath = localPath,
            transferId = null,
            transferStatus = "none",
            sendStatus = "sending",
            clientTime = nowSeconds,
            serverTimestamp = nowMillis,
            serverTime = "",
            isMine = true
        )
        messageDao.upsert(message)
        upsertConversationFor(message, title, incrementUnread = false)

        val payload = JSONObject()
            .put("msg_id", msgId)
            .put("from_user_id", user.userId)
            .put("to_user_id", peerId)
            .put("chat_type", chatType)
            .put("content_type", contentType)
            .put("content", content)
            .put("client_time", nowSeconds)

        runCatching {
            socketClient.sendJson(MsgType.CHAT_MESSAGE, payload)
        }.onFailure {
            messageDao.updateSendStatus(msgId, "failed")
        }
    }

    private suspend fun handlePacket(packet: ImPacket) {
        when (packet.type) {
            MsgType.LOGIN_RSP -> handleLogin(packet.body)
            MsgType.REGISTER_RSP -> handleRegister(packet.body)
            MsgType.CHAT_MESSAGE, MsgType.IMAGE, MsgType.FILE, MsgType.VOICE -> handleIncomingMessage(JSONObject(packet.body))
            MsgType.OFFLINE_MESSAGE -> handleOfflineMessages(packet.body)
            MsgType.CHAT_HISTORY_RSP -> handleHistory(packet.body)
            MsgType.ACK -> handleAck(packet.body)
            MsgType.FRIEND_LIST_RSP, MsgType.FRIEND_LIST_UPDATE -> handleFriendList(packet.body)
            MsgType.GROUP_LIST_RSP, MsgType.GROUP_LIST_UPDATE -> handleGroupList(packet.body)
            MsgType.FILE_UPLOAD_RSP -> handleFileUploadRsp(packet.body)
            MsgType.FILE_DOWNLOAD_RSP -> handleFileDownloadRsp(packet.body)
            MsgType.FILE_DOWNLOAD_CHUNK -> handleFileDownloadChunk(packet.body)
            MsgType.HEARTBEAT -> Unit
            else -> Unit
        }
    }

    private fun handleLogin(body: String) {
        val obj = JSONObject(body)
        val code = obj.optInt("code")
        val pending = pendingLogin
        val canApplyLogin = pending?.sequence == loginSequence
        val result = if (code == 0) {
            val user = CurrentUser(
                userId = obj.optString("user_id"),
                nickname = obj.optString("nickname"),
                avatarUrl = obj.optString("avatar_url"),
                token = obj.optString("token"),
                gender = obj.optString("gender"),
                region = obj.optString("region"),
                signature = obj.optString("signature")
            )
            if (canApplyLogin) {
                preferences.saveUser(user)
                _currentUser.value = user
                socketClient.markLoggedIn()
                appScope.launch { refreshContacts() }
            }
            LoginResult(code, obj.optString("message"), user)
        } else {
            LoginResult(code, obj.optString("message"))
        }
        pending?.waiter?.complete(result)
        if (pendingLogin === pending) {
            pendingLogin = null
        }
    }

    private fun handleRegister(body: String) {
        val obj = JSONObject(body)
        pendingRegister?.complete(
            LoginResult(
                code = obj.optInt("code"),
                message = obj.optString("message"),
                user = null
            )
        )
        pendingRegister = null
    }

    private suspend fun handleIncomingMessage(obj: JSONObject) {
        persistRemoteMessage(obj, notifyIfBackground = true)
    }

    private suspend fun handleOfflineMessages(body: String) {
        val arr = JSONArray(body)
        for (index in 0 until arr.length()) {
            val obj = arr.getJSONObject(index)
            persistRemoteMessage(obj, notifyIfBackground = false)
            val msgId = obj.optString("msg_id")
            if (msgId.isNotBlank()) {
                socketClient.sendJson(MsgType.OFFLINE_MESSAGE_ACK, JSONObject().put("msg_id", msgId))
            }
        }
    }

    private suspend fun handleHistory(body: String) {
        val arr = JSONArray(body)
        for (index in 0 until arr.length()) {
            persistRemoteMessage(arr.getJSONObject(index), notifyIfBackground = false)
        }
    }

    private suspend fun persistRemoteMessage(obj: JSONObject, notifyIfBackground: Boolean) {
        val user = _currentUser.value ?: return
        val fromUserId = obj.optString("from_user_id")
        val toUserId = obj.optString("to_user_id")
        val chatType = normalizeChatType(obj)
        val peerId = if (chatType == "group") toUserId else if (fromUserId == user.userId) toUserId else fromUserId
        if (peerId.isBlank()) return

        val key = "$chatType:$peerId"
        val contentType = obj.optString("content_type", "text")
        val serverTimestamp = obj.optLong("server_timestamp", System.currentTimeMillis())
        val title = conversationTitle(peerId, chatType)
        val isMine = fromUserId == user.userId
        val message = ChatMessageEntity(
            msgId = obj.optString("msg_id", ProtocolCodec.generateMsgId()),
            conversationKey = key,
            peerId = peerId,
            chatType = chatType,
            fromUserId = fromUserId,
            toUserId = toUserId,
            contentType = contentType,
            content = obj.optString("content"),
            localPath = null,
            transferId = null,
            transferStatus = "none",
            sendStatus = if (isMine) "sent" else "received",
            clientTime = obj.optLong("client_time", 0L),
            serverTimestamp = serverTimestamp,
            serverTime = obj.optString("server_time"),
            isMine = isMine
        )
        messageDao.upsert(message)
        val incrementUnread = !isMine && _activeConversationKey.value != key
        upsertConversationFor(message, title, incrementUnread)

        if (notifyIfBackground && !isMine && !_isAppForeground.value) {
            notificationManager.showMessage(peerId, chatType, title, previewFor(message))
        }
    }

    private suspend fun handleAck(body: String) {
        val obj = JSONObject(body)
        val msgId = obj.optString("msg_id")
        if (msgId.isNotBlank()) {
            messageDao.updateSendStatus(msgId, obj.optString("status", "sent"))
        }
    }

    private suspend fun handleFriendList(body: String) {
        val arr = JSONArray(body)
        val friends = buildList {
            for (index in 0 until arr.length()) {
                val obj = arr.getJSONObject(index)
                val userId = obj.optString("friend_id", obj.optString("user_id"))
                if (userId.isBlank()) continue
                add(
                    FriendEntity(
                        userId = userId,
                        nickname = obj.optString("nickname", obj.optString("friend_nickname")),
                        remark = obj.optString("remark"),
                        avatarUrl = obj.optString("avatar_url", obj.optString("friend_avatar")),
                        gender = obj.optString("gender"),
                        region = obj.optString("region"),
                        signature = obj.optString("signature")
                    )
                )
            }
        }
        contactDao.upsertFriends(friends)
    }

    private suspend fun handleGroupList(body: String) {
        val arr = JSONArray(body)
        val groups = buildList {
            for (index in 0 until arr.length()) {
                val obj = arr.getJSONObject(index)
                val groupId = obj.optString("group_id")
                if (groupId.isBlank()) continue
                add(
                    GroupEntity(
                        groupId = groupId,
                        groupName = obj.optString("group_name"),
                        groupAvatar = obj.optString("group_avatar"),
                        ownerId = obj.optString("owner_id"),
                        memberCount = obj.optInt("member_count")
                    )
                )
            }
        }
        contactDao.upsertGroups(groups)
    }

    private suspend fun handleFileUploadRsp(body: String) {
        val obj = JSONObject(body)
        uploadResponses.emit(
            FileUploadResponse(
                transferId = obj.optString("transfer_id"),
                status = obj.optString("status"),
                code = obj.optInt("code"),
                message = obj.optString("message"),
                fileId = obj.optString("file_id"),
                fileName = obj.optString("file_name"),
                fileSize = obj.optLong("file_size"),
                mimeType = obj.optString("mime_type")
            )
        )
    }

    private suspend fun handleFileDownloadRsp(body: String) {
        val obj = JSONObject(body)
        val transferId = obj.optString("transfer_id")
        val download = pendingDownloads[transferId] ?: return
        when (obj.optString("status")) {
            "ready" -> withContext(Dispatchers.IO) {
                download.totalChunks = obj.optInt("total_chunks")
                download.fileSize = obj.optLong("file_size")
                download.target.writeBytes(ByteArray(0))
            }
            "complete" -> pendingDownloads.remove(transferId)
        }
    }

    private suspend fun handleFileDownloadChunk(body: String) {
        val obj = JSONObject(body)
        val transferId = obj.optString("transfer_id")
        val download = pendingDownloads[transferId] ?: return
        val chunk = Base64.decode(obj.optString("data"), Base64.DEFAULT)
        withContext(Dispatchers.IO) {
            download.target.appendBytes(chunk)
            download.receivedSize += chunk.size
            download.nextChunkIndex += 1
        }
    }

    private suspend fun upsertConversationFor(
        message: ChatMessageEntity,
        title: String,
        incrementUnread: Boolean
    ) {
        val old = conversationDao.find(message.conversationKey)
        val unread = when {
            !incrementUnread -> old?.unreadCount ?: 0
            else -> (old?.unreadCount ?: 0) + 1
        }
        conversationDao.upsert(
            ConversationEntity(
                conversationKey = message.conversationKey,
                peerId = message.peerId,
                chatType = message.chatType,
                title = title,
                avatarUrl = old?.avatarUrl ?: "",
                lastMessage = previewFor(message),
                lastTimestamp = message.serverTimestamp,
                unreadCount = unread
            )
        )
    }

    private suspend fun conversationTitle(peerId: String, chatType: String): String {
        return if (chatType == "group") {
            contactDao.findGroup(peerId)?.groupName?.ifBlank { peerId } ?: peerId
        } else {
            contactDao.findFriend(peerId)?.let { friend ->
                friend.remark.ifBlank { friend.nickname }.ifBlank { peerId }
            } ?: peerId
        }
    }

    private suspend fun ensureConnected() {
        if (socketClient.state.value == ConnectionState.Connected || socketClient.state.value == ConnectionState.LoggedIn) {
            return
        }
        socketClient.connect(preferences.serverHost(), preferences.serverPort())
    }

    private fun normalizeChatType(obj: JSONObject): String {
        val raw = obj.opt("chat_type")
        return when (raw) {
            is Number -> if (raw.toInt() == 2) "group" else "p2p"
            else -> obj.optString("chat_type", "p2p").ifBlank { "p2p" }
        }
    }

    private fun previewFor(message: ChatMessageEntity): String {
        return when (message.contentType) {
            "image" -> "[图片]"
            "voice" -> "[语音]"
            "video" -> "[视频]"
            "file" -> runCatching { JSONObject(message.content).optString("file_name") }.getOrNull()
                ?.takeIf { it.isNotBlank() }
                ?.let { "[文件] $it" } ?: "[文件]"
            else -> message.content
        }
    }

    private data class FileUploadResponse(
        val transferId: String,
        val status: String,
        val code: Int,
        val message: String,
        val fileId: String,
        val fileName: String,
        val fileSize: Long,
        val mimeType: String
    )

    private data class PendingLogin(
        val sequence: Int,
        val waiter: CompletableDeferred<LoginResult>
    )

    private data class PendingDownload(
        val transferId: String,
        val fileId: String,
        val fileName: String,
        val target: File,
        var fileSize: Long = 0,
        var totalChunks: Int = 0,
        var nextChunkIndex: Int = 0,
        var receivedSize: Int = 0
    )

    companion object {
        private const val ChunkSize = 256 * 1024
    }
}
