package com.ichat.android.data.repository

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.media.MediaMetadataRetriever
import android.net.Uri
import android.provider.OpenableColumns
import android.util.Base64
import com.ichat.android.data.db.ChatMessageEntity
import com.ichat.android.data.db.ConversationEntity
import com.ichat.android.data.db.FriendEntity
import com.ichat.android.data.db.GroupEntity
import com.ichat.android.data.db.IChatDatabase
import com.ichat.android.data.model.ChatTarget
import com.ichat.android.data.model.CurrentUser
import com.ichat.android.data.model.FriendRequest
import com.ichat.android.data.model.LoginResult
import com.ichat.android.data.model.MessageDraft
import com.ichat.android.data.model.MomentImage
import com.ichat.android.data.model.MomentPost
import com.ichat.android.data.model.UserProfile
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
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.flow.filter
import kotlinx.coroutines.flow.flatMapLatest
import kotlinx.coroutines.flow.flowOf
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeout
import kotlinx.coroutines.TimeoutCancellationException
import kotlin.coroutines.cancellation.CancellationException
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.FileOutputStream
import java.io.RandomAccessFile
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
    private val _friendRequests = MutableStateFlow<List<FriendRequest>>(emptyList())
    val friendRequests: StateFlow<List<FriendRequest>> = _friendRequests.asStateFlow()
    private val _userProfiles = MutableStateFlow<Map<String, UserProfile>>(emptyMap())
    val userProfiles: StateFlow<Map<String, UserProfile>> = _userProfiles.asStateFlow()
    private val _moments = MutableStateFlow<List<MomentPost>>(emptyList())
    val moments: StateFlow<List<MomentPost>> = _moments.asStateFlow()
    private val _momentsRefreshing = MutableStateFlow(false)
    val momentsRefreshing: StateFlow<Boolean> = _momentsRefreshing.asStateFlow()
    private val _momentPublishing = MutableStateFlow(false)
    val momentPublishing: StateFlow<Boolean> = _momentPublishing.asStateFlow()
    private val _momentsStatus = MutableStateFlow("")
    val momentsStatus: StateFlow<String> = _momentsStatus.asStateFlow()
    private val _profileStatus = MutableStateFlow("")
    val profileStatus: StateFlow<String> = _profileStatus.asStateFlow()

    private var loginSequence = 0
    private var acceptedConnectionSerial = 0L
    private var pendingLogin: PendingLogin? = null
    private var pendingRegister: CompletableDeferred<LoginResult>? = null
    private val uploadResponses = kotlinx.coroutines.flow.MutableSharedFlow<FileUploadResponse>(extraBufferCapacity = 32)
    private val pendingDownloads = ConcurrentHashMap<String, PendingDownload>()

    val connectionState: StateFlow<ConnectionState> = socketClient.state

    init {
        launchRepositoryTask {
            socketClient.incoming.collect { packet ->
                runRepositoryTask { handlePacket(packet) }
            }
        }
        savedUser?.let { user ->
            launchRepositoryTask {
                recoverInterruptedOutgoingMessages(user.userId)
                restoreSavedLogin(user)
            }
        }
    }

    private fun launchRepositoryTask(block: suspend () -> Unit) {
        appScope.launch {
            runRepositoryTask(block)
        }
    }

    private suspend fun runRepositoryTask(block: suspend () -> Unit) {
        try {
            block()
        } catch (error: CancellationException) {
            throw error
        } catch (_: Throwable) {
            // 后台同步失败不应终止进程；发起操作的页面会通过状态流或本地消息状态反馈失败。
        }
    }

    private suspend fun recoverInterruptedOutgoingMessages(ownerUserId: String) {
        withContext(Dispatchers.IO) {
            messageDao.markSendingAsFailed(ownerUserId, "failed:发送中断，请重新发送")
        }
    }

    fun setAppForeground(foreground: Boolean) {
        _isAppForeground.value = foreground
    }

    @OptIn(ExperimentalCoroutinesApi::class)
    fun observeConversations(): Flow<List<ConversationEntity>> =
        currentUser.flatMapLatest { user ->
            user?.let { conversationDao.observeConversations(it.userId) } ?: flowOf(emptyList())
        }

    @OptIn(ExperimentalCoroutinesApi::class)
    fun observeFriends(): Flow<List<FriendEntity>> =
        currentUser.flatMapLatest { user ->
            user?.let { contactDao.observeFriends(it.userId) } ?: flowOf(emptyList())
        }

    @OptIn(ExperimentalCoroutinesApi::class)
    fun observeGroups(): Flow<List<GroupEntity>> =
        currentUser.flatMapLatest { user ->
            user?.let { contactDao.observeGroups(it.userId) } ?: flowOf(emptyList())
        }

    @OptIn(ExperimentalCoroutinesApi::class)
    fun observeMessages(target: ChatTarget, limit: Int): Flow<List<ChatMessageEntity>> {
        return currentUser.flatMapLatest { user ->
            user?.let {
                messageDao.observeRecentMessages(it.userId, target.conversationKey, limit.coerceAtLeast(1))
            } ?: flowOf(emptyList())
        }
    }

    private suspend fun clearUserScopedCache(clearCurrentUser: Boolean) {
        val ownerUserId = _currentUser.value?.userId
        // Clear volatile account state before switching users or leaving the signed-in session.
        _activeConversationKey.value = null
        _activeConversationKey.value = null
        _friendRequests.value = emptyList()
        _userProfiles.value = emptyMap()
        _moments.value = emptyList()
        _momentsRefreshing.value = false
        _momentPublishing.value = false
        _momentsStatus.value = ""
        _profileStatus.value = ""
        pendingDownloads.clear()
        notificationManager.clearMessages()
        if (clearCurrentUser) {
            acceptedConnectionSerial = 0L
            _currentUser.value = null
        }

        if (!ownerUserId.isNullOrBlank()) {
            // Room rows are account-scoped; clear only the outgoing account instead of touching other users.
            withContext(Dispatchers.IO) {
                messageDao.clearForOwner(ownerUserId)
                conversationDao.clearForOwner(ownerUserId)
                contactDao.clearFriends(ownerUserId)
                contactDao.clearGroups(ownerUserId)
            }
        }
    }

    suspend fun openConversation(target: ChatTarget) {
        val user = _currentUser.value ?: return
        _activeConversationKey.value = target.conversationKey
        conversationDao.clearUnread(user.userId, target.conversationKey)
        requestChatHistory(target.peerId, target.chatType)
    }

    fun closeConversation(target: ChatTarget? = null) {
        val activeKey = _activeConversationKey.value
        if (target == null || activeKey == target.conversationKey) {
            _activeConversationKey.value = null
        }
    }

    suspend fun loadOlderMessages(target: ChatTarget, oldestMessage: ChatMessageEntity, limit: Int = 30) {
        val beforeTimeSeconds = historyBeforeTimeSeconds(oldestMessage)
        if (beforeTimeSeconds <= 0L) return
        requestChatHistory(
            peerId = target.peerId,
            chatType = target.chatType,
            limit = limit,
            beforeTimeSeconds = beforeTimeSeconds
        )
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
            clearUserScopedCache(clearCurrentUser = true)
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
        ensureSocketConnected()
        loginSequence += 1
        val sequence = loginSequence
        val waiter = CompletableDeferred<LoginResult>()
        pendingLogin = PendingLogin(sequence, waiter)
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
        clearUserScopedCache(clearCurrentUser = true)
    }

    suspend fun refreshContacts() {
        if (_currentUser.value == null) return
        ensureConnected()
        socketClient.send(MsgType.GET_FRIEND_LIST, "{}")
        socketClient.send(MsgType.GET_GROUP_LIST, "{}")
    }

    suspend fun refreshFriendRequests() {
        if (_currentUser.value == null) return
        ensureConnected()
        socketClient.send(MsgType.GET_FRIEND_REQUESTS, "{}")
    }

    suspend fun requestUserProfile(userId: String) {
        val targetUserId = userId.trim()
        if (_currentUser.value == null || targetUserId.isBlank()) return

        ensureConnected()
        val payload = JSONObject().put("user_id", targetUserId)
        val current = _currentUser.value
        if (current?.userId == targetUserId) {
            // 当前用户资料带本地快照，服务端可只回 ACK，减少大头像重复下发。
            payload
                .put("client_user_id", current.userId)
                .put(
                    "local_profile",
                    JSONObject()
                        .put("nickname", current.nickname)
                        .put("gender", current.gender)
                        .put("region", current.region)
                        .put("signature", current.signature)
                )
        }
        socketClient.sendJson(MsgType.GET_USER_PROFILE, payload)
    }

    suspend fun refreshMoments(limit: Int = 50, targetUserId: String = "") {
        if (_currentUser.value == null) {
            _moments.value = emptyList()
            _momentsStatus.value = "未登录"
            return
        }

        _momentsRefreshing.value = true
        _momentsStatus.value = "正在加载..."
        runCatching {
            ensureConnected()
            val payload = JSONObject().put("limit", limit.coerceIn(1, 100))
            if (targetUserId.isNotBlank()) {
                payload.put("target_user_id", targetUserId.trim())
            }
            socketClient.sendJson(MsgType.GET_MOMENTS, payload)
        }.onFailure {
            _momentsRefreshing.value = false
            _momentsStatus.value = it.message ?: "朋友圈加载失败"
        }
    }

    suspend fun createMoment(content: String, imageUris: List<Uri>) {
        val cleanContent = content.trim()
        if (_currentUser.value == null) {
            _momentsStatus.value = "未登录"
            return
        }
        if (cleanContent.isBlank() && imageUris.isEmpty()) {
            _momentsStatus.value = "请填写文字或选择图片"
            return
        }
        if (imageUris.size > MaxMomentImages) {
            _momentsStatus.value = "朋友圈图片最多九张"
            return
        }

        _momentPublishing.value = true
        _momentsStatus.value = "正在发布..."
        runCatching {
            val images = encodeMomentImages(imageUris)
            ensureConnected()
            socketClient.sendJson(
                MsgType.CREATE_MOMENT,
                JSONObject()
                    .put("content", cleanContent)
                    .put("images", images)
            )
        }.onFailure {
            _momentPublishing.value = false
            _momentsStatus.value = it.message ?: "发布失败"
        }
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
        ensureConnected()
        socketClient.sendJson(
            MsgType.FRIEND_REQUEST,
            JSONObject()
                .put("phone", toUserIdOrPhone)
                .put("remark", remark)
        )
    }

    suspend fun respondFriendRequest(requestId: String, accept: Boolean) {
        ensureConnected()
        // UI 先把已处理请求移除，服务端响应后会再刷新好友和请求列表。
        _friendRequests.value = _friendRequests.value.filterNot { it.requestId == requestId }
        socketClient.sendJson(
            MsgType.FRIEND_REQUEST_RSP,
            JSONObject()
                .put("request_id", requestId)
                .put("accept", accept)
        )
    }

    suspend fun createGroup(groupName: String, memberIds: List<String>) {
        ensureConnected()
        socketClient.sendJson(
            MsgType.CREATE_GROUP,
            JSONObject()
                .put("group_name", groupName)
                .put("member_ids", JSONArray(memberIds))
        )
    }

    suspend fun updateFriendRemark(friendId: String, remark: String) {
        ensureConnected()
        socketClient.sendJson(
            MsgType.UPDATE_FRIEND_REMARK,
            JSONObject()
                .put("friend_id", friendId)
                .put("remark", remark)
        )
    }

    suspend fun updateProfile(nickname: String, gender: String, region: String, signature: String) {
        _profileStatus.value = "正在保存资料..."
        ensureConnected()
        socketClient.sendJson(
            MsgType.UPDATE_PROFILE,
            JSONObject()
                .put("nickname", nickname)
                .put("gender", gender)
                .put("region", region)
                .put("signature", signature)
        )
    }

    suspend fun updateAvatar(uri: Uri) {
        _profileStatus.value = "正在处理头像..."
        val avatarUrl = withContext(Dispatchers.IO) {
            encodeAvatarImage(uri)
        }
        ensureConnected()
        _profileStatus.value = "正在上传头像..."
        socketClient.sendJson(
            MsgType.UPDATE_AVATAR,
            JSONObject().put("avatar_url", avatarUrl)
        )
    }

    suspend fun changePassword(oldPassword: String, newPassword: String) {
        ensureConnected()
        socketClient.sendJson(
            MsgType.CHANGE_PASSWORD,
            JSONObject()
                .put("old_password", oldPassword)
                .put("new_password", newPassword)
        )
    }

    suspend fun downloadFile(fileId: String, fileName: String): File = withContext(Dispatchers.IO) {
        ensureConnected()
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

    suspend fun downloadAttachment(message: ChatMessageEntity): File? {
        val user = _currentUser.value ?: return null
        if (message.ownerUserId != user.userId || message.transferStatus == "downloading") return null

        val cached = message.localPath
            ?.let { File(it) }
            ?.takeIf { message.transferStatus == "downloaded" && it.exists() }
        if (cached != null) return cached

        val file = attachmentContent(message.content)
        val fileId = file.optString("file_id")
        val fileName = file.optString("file_name", when (message.contentType) {
            "image" -> "image.jpg"
            "video" -> "video.mp4"
            else -> "file.bin"
        })
        if (fileId.isBlank()) {
            messageDao.updateLocalFile(user.userId, message.msgId, null, "failed:文件ID为空")
            return null
        }
        val existingDownload = attachmentStore.findExistingDownloadFile(fileId, fileName)
        if (existingDownload != null) {
            messageDao.updateLocalFile(user.userId, message.msgId, existingDownload.absolutePath, "downloaded")
            return existingDownload
        }
        if (pendingDownloads.values.any { it.ownerUserId == user.userId && it.msgId == message.msgId }) return null

        ensureConnected()
        return withContext(Dispatchers.IO) {
            val transferId = ProtocolCodec.generateMsgId()
            val target = attachmentStore.reserveDownloadFile(fileId, fileName)
            pendingDownloads[transferId] = PendingDownload(
                transferId = transferId,
                fileId = fileId,
                fileName = fileName,
                target = target,
                ownerUserId = user.userId,
                msgId = message.msgId
            )
            messageDao.updateLocalFile(user.userId, message.msgId, null, "downloading")
            socketClient.sendJson(
                MsgType.FILE_DOWNLOAD_REQ,
                JSONObject()
                    .put("transfer_id", transferId)
                    .put("file_id", fileId)
                    .put("file_name", fileName)
            )
            target
        }
    }

    suspend fun uploadFile(peerId: String, chatType: String, uri: Uri) = withContext(Dispatchers.IO) {
        ensureConnected()
        val fileName = displayNameForUri(uri)
        val mimeType = context.contentResolver.getType(uri)
            ?: URLConnection.guessContentTypeFromName(fileName)
            ?: "application/octet-stream"
        val contentType = when {
            mimeType.startsWith("image/") -> "image"
            mimeType.startsWith("video/") -> "video"
            else -> "file"
        }
        val previewDataUrl = if (contentType == "image") makeImagePreviewDataUrl(uri) else ""
        val posterDataUrl = if (contentType == "video") makeVideoPosterDataUrl(uri) else ""
        val transferId = ProtocolCodec.generateMsgId()
        val tempFile = File(context.cacheDir, "ichat_upload_$transferId.tmp")
        context.contentResolver.openInputStream(uri)?.use { input ->
            FileOutputStream(tempFile).use { output -> input.copyTo(output) }
        } ?: error("无法读取文件")

        val fileSize = tempFile.length()
        if (fileSize <= 0) error("不能上传空文件")
        if (fileSize > 200L * 1024L * 1024L) error("单个文件不能超过200MB")

        val totalChunks = ((fileSize + ChunkSize - 1) / ChunkSize).toInt()
        val ready = awaitUploadResponse(
            transferId = transferId,
            statuses = setOf("ready"),
            timeoutMillis = UploadResponseTimeoutMillis,
            timeoutMessage = "等待服务器准备上传超时"
        ) {
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
        }
        if (ready.code != 0 || ready.status == "failed") error(ready.message.ifBlank { "服务器拒绝上传" })

        val complete = try {
            uploadFileChunks(tempFile, transferId, totalChunks, ready.nextChunkIndex)
        } finally {
            tempFile.delete()
        }
        val localPath = if (contentType == "image" && previewDataUrl.isNotBlank()) {
            attachmentStore.saveDataUrlToCache(complete.fileId, complete.fileName.ifBlank { fileName }, previewDataUrl)
                ?.absolutePath
        } else {
            null
        }
        val content = JSONObject()
            .put("file_id", complete.fileId)
            .put("file_name", complete.fileName.ifBlank { fileName })
            .put("file_size", complete.fileSize)
            .put("mime_type", complete.mimeType.ifBlank { mimeType })
            .put("transfer_id", transferId)
        if (previewDataUrl.isNotBlank()) {
            content.put("preview_data_url", previewDataUrl)
        }
        if (posterDataUrl.isNotBlank()) {
            content.put("poster_data_url", posterDataUrl)
        }
        sendChatMessage(peerId, chatType, contentType, content.toString(), localPath = localPath)
    }

    private suspend fun uploadFileChunks(
        file: File,
        transferId: String,
        totalChunks: Int,
        firstChunkIndex: Int
    ): FileUploadResponse {
        var nextChunkIndex = firstChunkIndex.coerceAtLeast(0)
        while (nextChunkIndex < totalChunks) {
            val chunkIndex = nextChunkIndex
            val chunk = readUploadChunk(file, chunkIndex)
            val response = awaitUploadResponse(
                transferId = transferId,
                statuses = setOf("chunk", "complete"),
                timeoutMillis = UploadResponseTimeoutMillis,
                timeoutMessage = "等待服务器确认分片超时"
            ) {
                socketClient.sendJson(
                    MsgType.FILE_UPLOAD_CHUNK,
                    JSONObject()
                        .put("transfer_id", transferId)
                        .put("chunk_index", chunkIndex)
                        .put("data", Base64.encodeToString(chunk, Base64.NO_WRAP))
                )
            }
            if (response.code != 0 || response.status == "failed") error(response.message.ifBlank { "文件分片上传失败" })
            if (response.status == "complete") return response
            val serverNextChunk = response.nextChunkIndex
            if (serverNextChunk <= chunkIndex) {
                error("服务器分片进度异常")
            }
            nextChunkIndex = serverNextChunk
        }
        error("文件上传未收到完成确认")
    }

    private suspend fun awaitUploadResponse(
        transferId: String,
        statuses: Set<String>,
        timeoutMillis: Long,
        timeoutMessage: String,
        beforeAwait: suspend () -> Unit
    ): FileUploadResponse {
        val deferred = CompletableDeferred<FileUploadResponse>()
        val waiter = appScope.launch {
            uploadResponses
                .filter {
                    it.transferId == transferId &&
                        (it.status in statuses || it.status == "failed" || it.code != 0)
                }
                .first()
                .also {
                    if (!deferred.isCompleted) deferred.complete(it)
                }
        }
        try {
            beforeAwait()
            return try {
                withTimeout(timeoutMillis) { deferred.await() }
            } catch (_: TimeoutCancellationException) {
                error(timeoutMessage)
            }
        } finally {
            waiter.cancel()
        }
    }

    private fun readUploadChunk(file: File, chunkIndex: Int): ByteArray {
        val offset = chunkIndex.toLong() * ChunkSize
        val remaining = file.length() - offset
        if (remaining <= 0) error("读取文件分片失败")
        val size = minOf(ChunkSize.toLong(), remaining).toInt()
        val bytes = ByteArray(size)
        RandomAccessFile(file, "r").use { input ->
            input.seek(offset)
            input.readFully(bytes)
        }
        return bytes
    }

    private suspend fun requestChatHistory(
        peerId: String,
        chatType: String,
        limit: Int = 30,
        beforeTimeSeconds: Long = 0L
    ) {
        if (_currentUser.value == null) return
        ensureConnected()
        val payload = JSONObject()
            .put("peer_id", peerId)
            .put("chat_type", chatType)
            .put("limit", limit.coerceIn(1, 50))
        if (beforeTimeSeconds > 0L) {
            payload.put("before_time", beforeTimeSeconds)
        }
        socketClient.sendJson(
            MsgType.GET_CHAT_HISTORY,
            payload
        )
    }

    private fun historyBeforeTimeSeconds(message: ChatMessageEntity): Long {
        return when {
            message.serverTimestamp > 1_000_000_000_000L -> message.serverTimestamp / 1000L
            message.serverTimestamp > 0L -> message.serverTimestamp
            message.clientTime > 0L -> message.clientTime
            else -> 0L
        }
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
        val title = conversationTitle(user.userId, peerId, chatType)
        val message = ChatMessageEntity(
            ownerUserId = user.userId,
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
            transferStatus = initialTransferStatus(contentType, localPath),
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
            ensureConnected()
            socketClient.sendJson(MsgType.CHAT_MESSAGE, payload)
            scheduleOutgoingMessageTimeout(user.userId, msgId)
        }.onFailure {
            messageDao.updateSendStatus(user.userId, msgId, "failed")
        }
    }

    private fun scheduleOutgoingMessageTimeout(ownerUserId: String, msgId: String) {
        launchRepositoryTask {
            delay(OutgoingAckTimeoutMillis)
            messageDao.updateSendStatusIfSending(ownerUserId, msgId, "failed:发送超时，请重试")
        }
    }

    private suspend fun handlePacket(packet: ImPacket) {
        val isAuthPacket = packet.type == MsgType.LOGIN_RSP || packet.type == MsgType.REGISTER_RSP
        if (!isAuthPacket &&
            (_currentUser.value == null || packet.connectionSerial != acceptedConnectionSerial)
        ) {
            return
        }

        when (packet.type) {
            MsgType.LOGIN_RSP -> handleLogin(packet)
            MsgType.REGISTER_RSP -> handleRegister(packet.body)
            MsgType.CHAT_MESSAGE, MsgType.IMAGE, MsgType.FILE, MsgType.VOICE -> handleIncomingMessage(JSONObject(packet.body))
            MsgType.OFFLINE_MESSAGE -> handleOfflineMessages(packet.body)
            MsgType.CHAT_HISTORY_RSP -> handleHistory(packet.body)
            MsgType.ACK -> handleAck(packet.body)
            MsgType.FRIEND_LIST_RSP, MsgType.FRIEND_LIST_UPDATE -> handleFriendList(packet.body)
            MsgType.FRIEND_REQUEST_NEW -> handleFriendRequests(packet.body)
            MsgType.FRIEND_REQUEST_RSP -> handleFriendRequestRsp(packet.body)
            MsgType.USER_PROFILE_RSP -> handleUserProfileRsp(packet.body)
            MsgType.GROUP_LIST_RSP, MsgType.GROUP_LIST_UPDATE -> handleGroupList(packet.body)
            MsgType.CREATE_GROUP_RSP -> handleCreateGroupRsp(packet.body)
            MsgType.FILE_UPLOAD_RSP -> handleFileUploadRsp(packet.body)
            MsgType.FILE_DOWNLOAD_RSP -> handleFileDownloadRsp(packet.body)
            MsgType.FILE_DOWNLOAD_CHUNK -> handleFileDownloadChunk(packet.body)
            MsgType.UPDATE_AVATAR_RSP -> handleUpdateAvatarRsp(packet.body)
            MsgType.UPDATE_PROFILE_RSP -> handleUpdateProfileRsp(packet.body)
            MsgType.CREATE_MOMENT_RSP -> handleCreateMomentRsp(packet.body)
            MsgType.MOMENTS_RSP -> handleMoments(packet.body)
            MsgType.HEARTBEAT -> Unit
            else -> Unit
        }
    }

    private suspend fun handleLogin(packet: ImPacket) {
        val obj = JSONObject(packet.body)
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
                if (_currentUser.value?.userId != user.userId) {
                    clearUserScopedCache(clearCurrentUser = false)
                }
                acceptedConnectionSerial = packet.connectionSerial
                preferences.saveUser(user)
                _currentUser.value = user
                recoverInterruptedOutgoingMessages(user.userId)
                socketClient.markLoggedIn()
                launchRepositoryTask { refreshContacts() }
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
        val msgId = obj.optString("msg_id", ProtocolCodec.generateMsgId())
        val contentType = obj.optString("content_type", "text")
        val content = obj.optString("content")
        val existing = messageDao.findById(user.userId, msgId)
        val cachedPreviewPath = cacheIncomingImagePreview(contentType, content)
        val recoveredDownloadPath = recoverExistingDownloadPath(contentType, content)
        val preservedLocalPath = existing?.localPath
            ?.takeIf { it.isNotBlank() }
            ?.takeIf { File(it).exists() }
        val localPath = preservedLocalPath ?: recoveredDownloadPath ?: cachedPreviewPath
        val transferStatus = preservedTransferStatus(
            existing = existing,
            preservedLocalPath = preservedLocalPath,
            contentType = contentType,
            localPath = localPath
        )
        val serverTimestamp = obj.optLong("server_timestamp", System.currentTimeMillis())
        val title = conversationTitle(user.userId, peerId, chatType)
        val isMine = fromUserId == user.userId
        val message = ChatMessageEntity(
            ownerUserId = user.userId,
            msgId = msgId,
            conversationKey = key,
            peerId = peerId,
            chatType = chatType,
            fromUserId = fromUserId,
            toUserId = toUserId,
            contentType = contentType,
            content = content,
            localPath = localPath,
            transferId = null,
            transferStatus = transferStatus,
            sendStatus = if (isMine) "sent" else "received",
            clientTime = obj.optLong("client_time", 0L),
            serverTimestamp = serverTimestamp,
            serverTime = obj.optString("server_time"),
            isMine = isMine
        )
        messageDao.upsert(message)
        val incrementUnread = !isMine && (_activeConversationKey.value != key || !_isAppForeground.value)
        upsertConversationFor(message, title, incrementUnread)

        if (notifyIfBackground && !isMine && !_isAppForeground.value) {
            notificationManager.showMessage(peerId, chatType, title, previewFor(message))
        }
    }

    private suspend fun handleAck(body: String) {
        val user = _currentUser.value ?: return
        val obj = JSONObject(body)
        val msgId = obj.optString("msg_id")
        if (msgId.isNotBlank()) {
            messageDao.updateSendStatus(user.userId, msgId, obj.optString("status", "sent"))
        }
    }

    private suspend fun handleFriendList(body: String) {
        val user = _currentUser.value ?: return
        val arr = JSONArray(body)
        val friends = buildList {
            for (index in 0 until arr.length()) {
                val obj = arr.getJSONObject(index)
                val userId = obj.optString("friend_id", obj.optString("user_id"))
                if (userId.isBlank()) continue
                add(
                    FriendEntity(
                        ownerUserId = user.userId,
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
        contactDao.clearFriends(user.userId)
        if (friends.isNotEmpty()) {
            contactDao.upsertFriends(friends)
        }
    }

    private fun handleFriendRequests(body: String) {
        val arr = JSONArray(body)
        val requests = buildList {
            for (index in 0 until arr.length()) {
                val obj = arr.getJSONObject(index)
                val requestId = obj.optString("request_id")
                if (requestId.isBlank()) continue
                add(
                    FriendRequest(
                        requestId = requestId,
                        fromUserId = obj.optString("from_user_id"),
                        fromNickname = obj.optString("from_nickname"),
                        fromAvatar = obj.optString("from_avatar"),
                        remark = obj.optString("remark"),
                        createTime = obj.optString("create_time")
                    )
                )
            }
        }
        _friendRequests.value = requests
    }

    private fun handleFriendRequestRsp(body: String) {
        val obj = JSONObject(body)
        if (obj.optInt("code", -1) == 0) {
            launchRepositoryTask {
                refreshContacts()
                refreshFriendRequests()
            }
        }
    }

    private fun handleUserProfileRsp(body: String) {
        val obj = JSONObject(body)
        if (obj.optInt("code", -1) != 0) return

        val userId = obj.optString("user_id")
        if (userId.isBlank()) return

        val current = _currentUser.value
        val fallback = if (current?.userId == userId) {
            UserProfile(
                userId = current.userId,
                nickname = current.nickname,
                avatarUrl = current.avatarUrl,
                gender = current.gender,
                region = current.region,
                signature = current.signature
            )
        } else {
            _userProfiles.value[userId]
        }
        val profile = UserProfile(
            userId = userId,
            nickname = obj.optString("nickname", fallback?.nickname.orEmpty()),
            avatarUrl = obj.optString("avatar_url", fallback?.avatarUrl.orEmpty()),
            gender = obj.optString("gender", fallback?.gender.orEmpty()),
            region = obj.optString("region", fallback?.region.orEmpty()),
            signature = obj.optString("signature", fallback?.signature.orEmpty())
        )
        _userProfiles.value = _userProfiles.value + (userId to profile)

        if (current?.userId == userId) {
            updateCurrentUser {
                it.copy(
                    nickname = profile.nickname.ifBlank { it.nickname },
                    avatarUrl = profile.avatarUrl.ifBlank { it.avatarUrl },
                    gender = profile.gender,
                    region = profile.region,
                    signature = profile.signature
                )
            }
        }
    }

    private fun handleUpdateAvatarRsp(body: String) {
        val obj = JSONObject(body)
        val code = obj.optInt("code", -1)
        val message = obj.optString("message", if (code == 0) "头像已同步" else "头像上传失败")
        if (code == 0) {
            val avatarUrl = obj.optString("avatar_url")
            if (avatarUrl.isNotBlank()) {
                updateCurrentUser { it.copy(avatarUrl = avatarUrl) }
            }
        }
        _profileStatus.value = message
    }

    private fun handleUpdateProfileRsp(body: String) {
        val obj = JSONObject(body)
        val code = obj.optInt("code", -1)
        val message = obj.optString("message", if (code == 0) "资料已保存" else "资料保存失败")
        if (code == 0) {
            updateCurrentUser { user ->
                user.copy(
                    nickname = obj.optString("nickname", user.nickname),
                    gender = obj.optString("gender", user.gender),
                    region = obj.optString("region", user.region),
                    signature = obj.optString("signature", user.signature)
                )
            }
        }
        _profileStatus.value = message
    }

    private suspend fun handleGroupList(body: String) {
        val user = _currentUser.value ?: return
        val arr = JSONArray(body)
        val groups = buildList {
            for (index in 0 until arr.length()) {
                val obj = arr.getJSONObject(index)
                val groupId = obj.optString("group_id")
                if (groupId.isBlank()) continue
                add(
                    GroupEntity(
                        ownerUserId = user.userId,
                        groupId = groupId,
                        groupName = obj.optString("group_name"),
                        groupAvatar = obj.optString("group_avatar"),
                        ownerId = obj.optString("owner_id"),
                        memberCount = obj.optInt("member_count")
                    )
                )
            }
        }
        contactDao.clearGroups(user.userId)
        if (groups.isNotEmpty()) {
            contactDao.upsertGroups(groups)
        }
    }

    private suspend fun handleCreateGroupRsp(body: String) {
        val user = _currentUser.value ?: return
        val obj = JSONObject(body)
        if (obj.optInt("code", -1) != 0) return

        val groupId = obj.optString("group_id")
        if (groupId.isBlank()) return
        contactDao.upsertGroups(
            listOf(
                GroupEntity(
                    ownerUserId = user.userId,
                    groupId = groupId,
                    groupName = obj.optString("group_name"),
                    groupAvatar = obj.optString("group_avatar"),
                    ownerId = user.userId,
                    memberCount = obj.optInt("member_count")
                )
            )
        )
        refreshContacts()
    }

    private fun handleCreateMomentRsp(body: String) {
        val obj = JSONObject(body)
        val code = obj.optInt("code", -1)
        _momentPublishing.value = false
        _momentsStatus.value = obj.optString("message", if (code == 0) "发布成功" else "发布失败")
        if (code == 0) {
            launchRepositoryTask { refreshMoments() }
        }
    }

    private fun handleMoments(body: String) {
        _momentsRefreshing.value = false
        val posts = runCatching {
            val arr = JSONArray(body)
            buildList {
                for (index in 0 until arr.length()) {
                    add(parseMoment(arr.getJSONObject(index)))
                }
            }
        }.getOrElse {
            _momentsStatus.value = "朋友圈加载失败"
            return
        }
        _moments.value = posts
        _momentsStatus.value = if (posts.isEmpty()) "0 条动态" else "${posts.size} 条动态"
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
                mimeType = obj.optString("mime_type"),
                nextChunkIndex = obj.optInt("next_chunk_index")
            )
        )
    }

    private suspend fun handleFileDownloadRsp(body: String) {
        val obj = JSONObject(body)
        val transferId = obj.optString("transfer_id")
        val download = pendingDownloads[transferId] ?: return
        if (obj.optInt("code", 0) != 0 || obj.optString("status") == "failed") {
            markPendingDownload(download, "failed:${obj.optString("message", "下载失败")}")
            pendingDownloads.remove(transferId)
            return
        }
        when (obj.optString("status")) {
            "ready" -> withContext(Dispatchers.IO) {
                download.totalChunks = obj.optInt("total_chunks")
                download.fileSize = obj.optLong("file_size")
                download.target.writeBytes(ByteArray(0))
            }
            "complete" -> {
                markPendingDownload(download, "downloaded", download.target.absolutePath)
                pendingDownloads.remove(transferId)
            }
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

    private suspend fun markPendingDownload(download: PendingDownload, status: String, path: String? = null) {
        val ownerUserId = download.ownerUserId ?: return
        val msgId = download.msgId ?: return
        messageDao.updateLocalFile(ownerUserId, msgId, path, status)
    }

    private suspend fun upsertConversationFor(
        message: ChatMessageEntity,
        title: String,
        incrementUnread: Boolean
    ) {
        val old = conversationDao.find(message.ownerUserId, message.conversationKey)
        val unread = when {
            !incrementUnread -> old?.unreadCount ?: 0
            else -> (old?.unreadCount ?: 0) + 1
        }
        val latestMessage = messageDao.latestMessages(message.ownerUserId, message.conversationKey, 1).firstOrNull() ?: message
        // 聊天历史和离线消息可能不是按时间正序抵达，摘要始终从本地库中最新一条消息生成。
        conversationDao.upsert(
            ConversationEntity(
                ownerUserId = message.ownerUserId,
                conversationKey = message.conversationKey,
                peerId = message.peerId,
                chatType = message.chatType,
                title = title,
                avatarUrl = old?.avatarUrl ?: "",
                lastMessage = previewFor(latestMessage),
                lastTimestamp = latestMessage.serverTimestamp,
                unreadCount = unread
            )
        )
    }

    private suspend fun conversationTitle(ownerUserId: String, peerId: String, chatType: String): String {
        return if (chatType == "group") {
            contactDao.findGroup(ownerUserId, peerId)?.groupName?.ifBlank { peerId } ?: peerId
        } else {
            contactDao.findFriend(ownerUserId, peerId)?.let { friend ->
                friend.remark.ifBlank { friend.nickname }.ifBlank { peerId }
            } ?: peerId
        }
    }

    private suspend fun ensureConnected() {
        if (socketClient.state.value == ConnectionState.LoggedIn) {
            return
        }
        ensureSocketConnected()
        val user = _currentUser.value
        if (user?.token.isNullOrBlank()) return
        val result = loginWithToken(user.userId, user.token)
        if (result.code != 0) {
            error(result.message.ifBlank { "登录已失效，请重新登录" })
        }
    }

    private suspend fun ensureSocketConnected() {
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

    private fun parseMoment(obj: JSONObject): MomentPost {
        val media = obj.optJSONArray("media")
        val images = buildList {
            if (media == null) return@buildList
            for (index in 0 until media.length()) {
                val value = media.opt(index)
                if (value is JSONObject) {
                    val fullUrl = value.optString("image_url", value.optString("thumb_url"))
                    val thumbUrl = value.optString("thumb_url", fullUrl)
                    if (thumbUrl.isNotBlank() || fullUrl.isNotBlank()) {
                        add(MomentImage(thumbUrl = thumbUrl, imageUrl = fullUrl.ifBlank { thumbUrl }))
                    }
                } else {
                    val url = media.optString(index)
                    if (url.isNotBlank()) {
                        add(MomentImage(thumbUrl = url, imageUrl = url))
                    }
                }
            }
        }
        return MomentPost(
            momentId = obj.optString("moment_id"),
            userId = obj.optString("user_id"),
            nickname = obj.optString("nickname", obj.optString("user_id")),
            avatarUrl = obj.optString("avatar_url"),
            content = obj.optString("content"),
            mediaType = obj.optString("media_type", if (images.isEmpty()) "text" else "image"),
            images = images,
            createTime = obj.optString("create_time"),
            createTimestamp = obj.optLong("create_timestamp", 0L)
        )
    }

    private fun updateCurrentUser(update: (CurrentUser) -> CurrentUser) {
        val current = _currentUser.value ?: return
        val updated = update(current)
        _currentUser.value = updated
        preferences.saveUser(updated)
    }

    private fun encodeAvatarImage(uri: Uri): String {
        val source = context.contentResolver.openInputStream(uri)?.use { input ->
            BitmapFactory.decodeStream(input)
        } ?: error("无法读取头像")

        // 服务端限制头像 data URL 小于 700KB，这里先裁成正方形再压缩，避免原图直接上传阻塞聊天协议。
        val square = source.centerCropped(320)
        var bytes = square.toJpegBytes(76)
        if (bytes.size > MaxAvatarImageBytes) {
            bytes = square.scaledToMaxEdge(240).toJpegBytes(66)
        }
        if (bytes.size > MaxAvatarImageBytes) {
            error("头像图片过大，请选择更小的图片")
        }
        return "data:image/jpeg;base64," + Base64.encodeToString(bytes, Base64.NO_WRAP)
    }

    private suspend fun encodeMomentImages(imageUris: List<Uri>): JSONArray = withContext(Dispatchers.IO) {
        val images = JSONArray()
        imageUris.forEach { uri ->
            images.put(encodeMomentImage(uri))
        }
        images
    }

    private fun encodeMomentImage(uri: Uri): JSONObject {
        val source = context.contentResolver.openInputStream(uri)?.use { input ->
            BitmapFactory.decodeStream(input)
        } ?: error("无法读取图片")

        // 服务端和 Qt 客户端约定朋友圈图片直接使用 data URL；这里压缩原图并生成 240px 缩略图。
        val fullBitmap = source.scaledToMaxEdge(1280)
        var fullBytes = fullBitmap.toJpegBytes(78)
        if (fullBytes.size > MaxMomentImageBytes) {
            val smaller = source.scaledToMaxEdge(960)
            fullBytes = smaller.toJpegBytes(65)
            if (fullBytes.size > MaxMomentImageBytes) {
                error("图片过大，请选择更小的图片")
            }
        }
        val thumbBytes = source.centerCropped(240).toJpegBytes(58)

        return JSONObject()
            .put("thumb_url", "data:image/jpeg;base64," + Base64.encodeToString(thumbBytes, Base64.NO_WRAP))
            .put("image_url", "data:image/jpeg;base64," + Base64.encodeToString(fullBytes, Base64.NO_WRAP))
    }

    private fun displayNameForUri(uri: Uri): String {
        val queried = runCatching {
            context.contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)
                ?.use { cursor ->
                    val index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                    if (index >= 0 && cursor.moveToFirst()) cursor.getString(index) else null
                }
        }.getOrNull()
        return queried?.takeIf { it.isNotBlank() }
            ?: uri.lastPathSegment?.substringAfterLast('/')?.ifBlank { "file" }
            ?: "file"
    }

    private fun makeImagePreviewDataUrl(uri: Uri): String {
        val source = context.contentResolver.openInputStream(uri)?.use { input ->
            BitmapFactory.decodeStream(input)
        } ?: error("无法读取图片")
        return "data:image/jpeg;base64," + Base64.encodeToString(
            source.scaledToMaxEdge(1280).toJpegBytes(78),
            Base64.NO_WRAP
        )
    }

    private fun makeVideoPosterDataUrl(uri: Uri): String {
        val retriever = MediaMetadataRetriever()
        return runCatching {
            // Video messages should be recognizable before the user downloads the original file.
            retriever.setDataSource(context, uri)
            val frame = retriever.getFrameAtTime(0, MediaMetadataRetriever.OPTION_CLOSEST_SYNC)
                ?: retriever.getFrameAtTime()
                ?: return@runCatching ""
            "data:image/jpeg;base64," + Base64.encodeToString(
                frame.scaledToMaxEdge(640).toJpegBytes(68),
                Base64.NO_WRAP
            )
        }.getOrDefault("").also {
            runCatching { retriever.release() }
        }
    }

    private suspend fun cacheIncomingImagePreview(contentType: String, content: String): String? {
        if (contentType != "image") return null
        val file = attachmentContent(content)
        val preview = file.optString("preview_data_url")
        if (preview.isBlank()) return null
        // Images render directly from a local cached preview so conversation scrolling stays smooth.
        return withContext(Dispatchers.IO) {
            runCatching {
                attachmentStore.saveDataUrlToCache(
                    file.optString("file_id"),
                    file.optString("file_name", "image.jpg"),
                    preview
                )?.absolutePath
            }.getOrNull()
        }
    }

    private fun recoverExistingDownloadPath(contentType: String, content: String): String? {
        if (contentType !in setOf("image", "video", "file")) return null
        val file = attachmentContent(content)
        val fileId = file.optString("file_id")
        val fileName = file.optString("file_name")
        if (fileId.isBlank() || fileName.isBlank()) return null
        return attachmentStore.findExistingDownloadFile(fileId, fileName)?.absolutePath
    }

    private fun attachmentContent(content: String): JSONObject {
        return runCatching { JSONObject(content) }.getOrDefault(JSONObject())
    }

    private fun initialTransferStatus(contentType: String, localPath: String?): String {
        if (localPath != null) return "downloaded"
        return when (contentType) {
            "image", "video", "file" -> "not_downloaded"
            else -> "none"
        }
    }

    private fun preservedTransferStatus(
        existing: ChatMessageEntity?,
        preservedLocalPath: String?,
        contentType: String,
        localPath: String?
    ): String {
        // History refreshes replace full message rows, so keep durable local download state when the file still exists.
        if (preservedLocalPath != null && existing?.transferStatus == "downloaded") return "downloaded"
        if (existing?.transferStatus == "downloading") return "downloading"
        if (existing?.transferStatus?.startsWith("failed:") == true && localPath == null) return existing.transferStatus
        return initialTransferStatus(contentType, localPath)
    }

    private fun Bitmap.scaledToMaxEdge(maxEdge: Int): Bitmap {
        val edge = maxOf(width, height)
        if (edge <= maxEdge) return this
        val scale = maxEdge.toFloat() / edge.toFloat()
        return Bitmap.createScaledBitmap(
            this,
            (width * scale).toInt().coerceAtLeast(1),
            (height * scale).toInt().coerceAtLeast(1),
            true
        )
    }

    private fun Bitmap.centerCropped(size: Int): Bitmap {
        val scale = maxOf(size.toFloat() / width.toFloat(), size.toFloat() / height.toFloat())
        val scaledWidth = (width * scale).toInt().coerceAtLeast(size)
        val scaledHeight = (height * scale).toInt().coerceAtLeast(size)
        val scaled = Bitmap.createScaledBitmap(this, scaledWidth, scaledHeight, true)
        return Bitmap.createBitmap(
            scaled,
            ((scaledWidth - size) / 2).coerceAtLeast(0),
            ((scaledHeight - size) / 2).coerceAtLeast(0),
            size,
            size
        )
    }

    private fun Bitmap.toJpegBytes(quality: Int): ByteArray {
        val output = java.io.ByteArrayOutputStream()
        if (!compress(Bitmap.CompressFormat.JPEG, quality, output)) {
            error("图片压缩失败")
        }
        return output.toByteArray()
    }

    private data class FileUploadResponse(
        val transferId: String,
        val status: String,
        val code: Int,
        val message: String,
        val fileId: String,
        val fileName: String,
        val fileSize: Long,
        val mimeType: String,
        val nextChunkIndex: Int
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
        val ownerUserId: String? = null,
        val msgId: String? = null,
        var fileSize: Long = 0,
        var totalChunks: Int = 0,
        var nextChunkIndex: Int = 0,
        var receivedSize: Int = 0
    )

    companion object {
        private const val ChunkSize = 256 * 1024
        private const val UploadResponseTimeoutMillis = 15_000L
        private const val OutgoingAckTimeoutMillis = 30_000L
        private const val MaxMomentImages = 9
        private const val MaxMomentImageBytes = 850 * 1024
        private const val MaxAvatarImageBytes = 500 * 1024
    }
}
