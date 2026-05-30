package com.ichat.android.ui.chat

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.snapshotFlow
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.geometry.RoundRect
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Outline
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.Density
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.fragment.app.viewModels
import com.ichat.android.IChatApplication
import com.ichat.android.data.db.ChatMessageEntity
import com.ichat.android.data.db.ConversationEntity
import com.ichat.android.data.db.FriendEntity
import com.ichat.android.data.db.GroupEntity
import com.ichat.android.data.model.ChatTarget
import com.ichat.android.data.model.CurrentUser
import com.ichat.android.ui.common.AvatarProfile
import com.ichat.android.ui.common.BackIconButton
import com.ichat.android.ui.common.ContactActionDialogs
import com.ichat.android.ui.common.HomeTopBar
import com.ichat.android.ui.common.IChatAvatar
import com.ichat.android.ui.common.IChatSearchField
import com.ichat.android.ui.main.MainViewModel
import com.ichat.android.ui.main.ViewModelFactory
import com.ichat.android.ui.me.MyProfileFragment
import com.ichat.android.ui.profile.ContactProfileFragment
import com.ichat.android.ui.theme.IChatTheme
import kotlinx.coroutines.flow.distinctUntilChanged
import org.json.JSONObject
import java.time.Instant
import java.time.ZoneId
import java.time.format.DateTimeFormatter

class MessagesFragment : Fragment() {
    private val factory: ViewModelFactory by lazy {
        ViewModelFactory((requireActivity().application as IChatApplication).container.repository)
    }
    private val viewModel: MessagesViewModel by viewModels { factory }
    private val mainViewModel: MainViewModel by activityViewModels { factory }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        return ComposeView(requireContext()).apply {
            setContent {
                IChatTheme {
                    MessagesScreen(
                        viewModel = viewModel,
                        mainViewModel = mainViewModel,
                        onOpenProfile = ::openProfile
                    )
                }
            }
        }
    }

    private fun openProfile(profile: AvatarProfile, isCurrentUser: Boolean) {
        val hostId = (view?.parent as? ViewGroup)?.id ?: return
        val fragment = if (isCurrentUser) {
            MyProfileFragment()
        } else {
            ContactProfileFragment.newInstance(profile)
        }
        parentFragmentManager.beginTransaction()
            .replace(hostId, fragment)
            .addToBackStack("profile")
            .commit()
    }
}

@Composable
private fun MessagesScreen(
    viewModel: MessagesViewModel,
    mainViewModel: MainViewModel,
    onOpenProfile: (AvatarProfile, Boolean) -> Unit
) {
    val pending by mainViewModel.pendingChatTarget.collectAsState()
    val conversations by viewModel.conversations.collectAsState()
    val friends by viewModel.friends.collectAsState()
    val groups by viewModel.groups.collectAsState()
    val selected by viewModel.selectedTarget.collectAsState()
    val messages by viewModel.messages.collectAsState()
    val draft by viewModel.draft.collectAsState()
    val currentUser by viewModel.currentUser.collectAsState()
    val friendsById = remember(friends) { friends.associateBy { it.userId } }
    val groupsById = remember(groups) { groups.associateBy { it.groupId } }
    var showAddFriend by remember { mutableStateOf(false) }
    var showCreateGroup by remember { mutableStateOf(false) }

    LaunchedEffect(pending) {
        val target = pending ?: return@LaunchedEffect
        viewModel.openConversation(target)
        mainViewModel.consumePendingChatTarget()
    }

    val selectedTarget = selected
    LaunchedEffect(selectedTarget) {
        mainViewModel.setBottomNavigationVisible(selectedTarget == null)
    }
    DisposableEffect(Unit) {
        onDispose {
            mainViewModel.setBottomNavigationVisible(true)
        }
    }
    BackHandler(enabled = selectedTarget != null) {
        viewModel.backToConversations()
    }

    if (selectedTarget == null) {
        ConversationList(
            conversations = conversations,
            friendsById = friendsById,
            groupsById = groupsById,
            onOpen = viewModel::openConversation,
            onAddFriend = { showAddFriend = true },
            onCreateGroup = { showCreateGroup = true }
        )
    } else {
        ChatPanel(
            target = selectedTarget,
            messages = messages,
            draft = draft,
            currentUser = currentUser,
            friendsById = friendsById,
            groupsById = groupsById,
            onDraftChange = viewModel::updateDraft,
            onSend = viewModel::sendText,
            onLoadOlder = viewModel::loadOlderMessages,
            onBack = {
                viewModel.backToConversations()
                mainViewModel.setBottomNavigationVisible(true)
            },
            onAvatarClick = { clicked ->
                onOpenProfile(clicked, clicked.userId == currentUser?.userId && clicked.chatType != "group")
            }
        )
    }

    ContactActionDialogs(
        showAddFriend = showAddFriend,
        showCreateGroup = showCreateGroup,
        friends = friends,
        currentUser = currentUser,
        onDismissAddFriend = { showAddFriend = false },
        onDismissCreateGroup = { showCreateGroup = false },
        onSendFriendRequest = viewModel::sendFriendRequest,
        onCreateGroup = viewModel::createGroup
    )
}

@Composable
private fun ConversationList(
    conversations: List<ConversationEntity>,
    friendsById: Map<String, FriendEntity>,
    groupsById: Map<String, GroupEntity>,
    onOpen: (ConversationEntity) -> Unit,
    onAddFriend: () -> Unit,
    onCreateGroup: () -> Unit
) {
    var searchQuery by remember { mutableStateOf("") }
    val visibleConversations = remember(conversations, friendsById, groupsById, searchQuery) {
        conversations.filter { conversation ->
            conversationMatches(conversation, friendsById, groupsById, searchQuery)
        }
    }

    Column(Modifier.fillMaxSize()) {
        HomeTopBar(
            title = "IChat",
            onAddFriend = onAddFriend,
            onCreateGroup = onCreateGroup
        )
        IChatSearchField(
            value = searchQuery,
            onValueChange = { searchQuery = it },
            placeholder = "搜索"
        )
        LazyColumn(Modifier.fillMaxSize()) {
            if (visibleConversations.isEmpty()) {
                item {
                    Text(
                        text = if (searchQuery.isBlank()) "暂无会话" else "没有匹配的会话",
                        color = Color(0xFF6B756E),
                        modifier = Modifier.padding(horizontal = 20.dp, vertical = 18.dp)
                    )
                }
            }
            items(visibleConversations, key = { it.conversationKey }) { conversation ->
                val profile = conversationProfile(conversation, friendsById, groupsById)
                Card(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 16.dp, vertical = 6.dp)
                        .clickable { onOpen(conversation) },
                    shape = androidx.compose.foundation.shape.RoundedCornerShape(8.dp),
                    colors = CardDefaults.cardColors(containerColor = Color.White),
                    elevation = CardDefaults.cardElevation(defaultElevation = 1.dp)
                ) {
                    Row(
                        modifier = Modifier.padding(horizontal = 14.dp, vertical = 12.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        IChatAvatar(
                            avatarUrl = profile.avatarUrl,
                            displayName = profile.title,
                            size = 44.dp
                        )
                        Spacer(Modifier.width(12.dp))
                        Column(Modifier.weight(1f)) {
                            Text(profile.title, fontWeight = FontWeight.SemiBold)
                            Spacer(Modifier.height(4.dp))
                            Text(
                                conversation.lastMessage,
                                maxLines = 1,
                                overflow = TextOverflow.Ellipsis,
                                color = Color(0xFF6B756E)
                            )
                        }
                        if (conversation.unreadCount > 0) {
                            Text(conversation.unreadCount.toString())
                        }
                    }
                }
            }
        }
    }
}

private fun conversationMatches(
    conversation: ConversationEntity,
    friendsById: Map<String, FriendEntity>,
    groupsById: Map<String, GroupEntity>,
    query: String
): Boolean {
    val keyword = query.trim()
    if (keyword.isBlank()) return true

    // 搜索范围覆盖会话标题、账号/群号和最后一条消息，符合聊天首页的快速定位习惯。
    val profile = conversationProfile(conversation, friendsById, groupsById)
    return listOf(profile.title, conversation.peerId, conversation.lastMessage)
        .any { it.contains(keyword, ignoreCase = true) }
}

@Composable
private fun ChatPanel(
    target: ChatTarget,
    messages: List<ChatMessageEntity>,
    draft: String,
    currentUser: CurrentUser?,
    friendsById: Map<String, FriendEntity>,
    groupsById: Map<String, GroupEntity>,
    onDraftChange: (String) -> Unit,
    onSend: () -> Unit,
    onLoadOlder: () -> Unit,
    onBack: () -> Unit,
    onAvatarClick: (AvatarProfile) -> Unit
) {
    val targetProfile = targetProfile(target, friendsById, groupsById)
    val listState = rememberLazyListState()
    val scopedMessages = remember(target.conversationKey, messages) {
        messages.filter { it.conversationKey == target.conversationKey }
    }
    val displayMessages = remember(scopedMessages) { scopedMessages.asReversed() }
    val latestMessage = scopedMessages.lastOrNull()

    LaunchedEffect(latestMessage?.msgId) {
        if (latestMessage?.isMine == true) {
            listState.scrollToItem(0)
        }
    }
    LaunchedEffect(target.conversationKey, listState, displayMessages.size) {
        snapshotFlow {
            val layoutInfo = listState.layoutInfo
            val lastVisibleIndex = layoutInfo.visibleItemsInfo.maxOfOrNull { it.index } ?: 0
            layoutInfo.totalItemsCount > 0 && lastVisibleIndex >= layoutInfo.totalItemsCount - 3
        }.distinctUntilChanged().collect { shouldLoadOlder ->
            if (shouldLoadOlder) {
                onLoadOlder()
            }
        }
    }

    Column(Modifier.fillMaxSize()) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 12.dp, vertical = 8.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            BackIconButton(onClick = onBack)
            Spacer(Modifier.width(8.dp))
            Text(
                text = targetProfile.title,
                fontWeight = FontWeight.Bold,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
        }
        HorizontalDivider()
        LazyColumn(
            state = listState,
            reverseLayout = true,
            modifier = Modifier
                .weight(1f)
                .fillMaxWidth()
                .background(Color(0xFFF5F7F5))
                .padding(horizontal = 12.dp, vertical = 8.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            items(displayMessages, key = { it.msgId }) { message ->
                MessageRow(
                    message = message,
                    currentUser = currentUser,
                    friendsById = friendsById,
                    onAvatarClick = onAvatarClick
                )
            }
        }
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(10.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            OutlinedTextField(
                value = draft,
                onValueChange = onDraftChange,
                modifier = Modifier.weight(1f),
                placeholder = { Text("输入消息") }
            )
            Spacer(Modifier.width(8.dp))
            Button(onClick = onSend) {
                Text("发送")
            }
        }
    }
}

@Composable
private fun MessageRow(
    message: ChatMessageEntity,
    currentUser: CurrentUser?,
    friendsById: Map<String, FriendEntity>,
    onAvatarClick: (AvatarProfile) -> Unit
) {
    val profile = senderProfile(message, currentUser, friendsById)
    val horizontalArrangement = if (message.isMine) Arrangement.End else Arrangement.Start
    val columnAlignment = if (message.isMine) Alignment.End else Alignment.Start

    BoxWithConstraints(Modifier.fillMaxWidth()) {
        val maxBubbleWidth = maxWidth * 0.68f
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = horizontalArrangement,
            verticalAlignment = Alignment.Top
        ) {
            if (!message.isMine) {
                IChatAvatar(
                    avatarUrl = profile.avatarUrl,
                    displayName = profile.title,
                    size = 36.dp,
                    onClick = { onAvatarClick(profile) }
                )
                Spacer(Modifier.width(8.dp))
            }

            Column(horizontalAlignment = columnAlignment) {
                val time = messageTimeLabel(message)
                if (time.isNotBlank()) {
                    Text(time, color = Color(0xFF888888), fontSize = 12.sp)
                    Spacer(Modifier.height(3.dp))
                }
                MessageContent(message = message, maxWidth = maxBubbleWidth)
                val status = statusText(message.sendStatus)
                if (message.isMine && status.isNotBlank()) {
                    Spacer(Modifier.height(3.dp))
                    Text(status, color = Color(0xFF888888), fontSize = 12.sp)
                }
            }

            if (message.isMine) {
                Spacer(Modifier.width(8.dp))
                IChatAvatar(
                    avatarUrl = profile.avatarUrl,
                    displayName = profile.title,
                    size = 36.dp,
                    onClick = { onAvatarClick(profile) }
                )
            }
        }
    }
}

@Composable
private fun MessageContent(message: ChatMessageEntity, maxWidth: Dp) {
    if (message.contentType == "image" || message.contentType == "file" || message.contentType == "video") {
        MediaMessageCard(message = message, maxWidth = maxWidth)
    } else {
        TextMessageBubble(
            text = messageDisplayText(message),
            isMine = message.isMine,
            maxWidth = maxWidth
        )
    }
}

@Composable
private fun TextMessageBubble(text: String, isMine: Boolean, maxWidth: Dp) {
    Surface(
        modifier = Modifier.widthIn(max = maxWidth),
        shape = ChatBubbleShape(isMine),
        color = if (isMine) Color(0xFFD8F0D4) else Color.White,
        border = BorderStroke(1.dp, if (isMine) Color(0xFFB9DFB8) else Color(0xFFDFE7E1))
    ) {
        Text(
            text = text,
            modifier = Modifier.padding(
                start = if (isMine) 14.dp else 22.dp,
                top = 9.dp,
                end = if (isMine) 22.dp else 14.dp,
                bottom = 9.dp
            ),
            color = Color(0xFF111111),
            lineHeight = 20.sp
        )
    }
}

private class ChatBubbleShape(private val isMine: Boolean) : Shape {
    override fun createOutline(size: Size, layoutDirection: LayoutDirection, density: Density): Outline {
        val tailWidth = with(density) { 8.dp.toPx() }.coerceAtMost(size.width / 3f)
        val radius = with(density) { 8.dp.toPx() }
        val tailTop = with(density) { 13.dp.toPx() }
            .coerceAtMost((size.height - with(density) { 13.dp.toPx() }).coerceAtLeast(0f))
        val tailHeight = with(density) { 12.dp.toPx() }.coerceAtMost(size.height - tailTop)
        val tailMid = tailTop + tailHeight / 2f
        val tailBottom = tailTop + tailHeight
        val bubbleRect = if (isMine) {
            Rect(0f, 0f, size.width - tailWidth, size.height)
        } else {
            Rect(tailWidth, 0f, size.width, size.height)
        }

        val path = Path().apply {
            addRoundRect(RoundRect(bubbleRect, radius, radius))
            if (isMine) {
                moveTo(bubbleRect.right - 0.5f, tailTop)
                lineTo(size.width, tailMid)
                lineTo(bubbleRect.right - 0.5f, tailBottom)
            } else {
                moveTo(bubbleRect.left + 0.5f, tailTop)
                lineTo(0f, tailMid)
                lineTo(bubbleRect.left + 0.5f, tailBottom)
            }
            close()
        }

        return Outline.Generic(path)
    }
}

@Composable
private fun MediaMessageCard(message: ChatMessageEntity, maxWidth: Dp) {
    Surface(
        modifier = Modifier.widthIn(max = maxWidth),
        shape = ChatBubbleShape(message.isMine),
        color = if (message.isMine) Color(0xFFDFF5DF) else MaterialTheme.colorScheme.surface,
        border = BorderStroke(1.dp, if (message.isMine) Color(0xFFB9DFB8) else Color(0xFFDFE7E1))
    ) {
        Text(
            text = messageDisplayText(message),
            modifier = Modifier.padding(
                start = if (message.isMine) 14.dp else 22.dp,
                top = 10.dp,
                end = if (message.isMine) 22.dp else 14.dp,
                bottom = 10.dp
            ),
            color = Color(0xFF1F2A24)
        )
    }
}

private fun conversationProfile(
    conversation: ConversationEntity,
    friendsById: Map<String, FriendEntity>,
    groupsById: Map<String, GroupEntity>
): AvatarProfile {
    return if (conversation.chatType == "group") {
        val group = groupsById[conversation.peerId]
        AvatarProfile(
            title = group?.displayName() ?: conversation.title.ifBlank { conversation.peerId },
            subtitle = "群聊：${conversation.peerId}",
            avatarUrl = group?.groupAvatar ?: conversation.avatarUrl,
            detail = group?.let { "${it.memberCount} 人" }.orEmpty(),
            userId = conversation.peerId,
            chatType = "group"
        )
    } else {
        val friend = friendsById[conversation.peerId]
        AvatarProfile(
            title = friend?.displayName() ?: conversation.title.ifBlank { conversation.peerId },
            subtitle = "账号：${conversation.peerId}",
            avatarUrl = friend?.avatarUrl ?: conversation.avatarUrl,
            detail = friend?.signature.orEmpty(),
            userId = conversation.peerId,
            chatType = "p2p",
            gender = friend?.gender.orEmpty(),
            region = friend?.region.orEmpty(),
            signature = friend?.signature.orEmpty(),
            nickname = friend?.nickname.orEmpty(),
            remark = friend?.remark.orEmpty()
        )
    }
}

private fun targetProfile(
    target: ChatTarget,
    friendsById: Map<String, FriendEntity>,
    groupsById: Map<String, GroupEntity>
): AvatarProfile {
    return conversationProfile(
        ConversationEntity(
            ownerUserId = "",
            conversationKey = target.conversationKey,
            peerId = target.peerId,
            chatType = target.chatType,
            title = target.title,
            avatarUrl = "",
            lastMessage = "",
            lastTimestamp = 0L,
            unreadCount = 0
        ),
        friendsById,
        groupsById
    )
}

private fun senderProfile(
    message: ChatMessageEntity,
    currentUser: CurrentUser?,
    friendsById: Map<String, FriendEntity>
): AvatarProfile {
    if (message.isMine) {
        val userId = currentUser?.userId ?: message.fromUserId
        val title = currentUser?.nickname?.ifBlank { userId } ?: userId
        return AvatarProfile(
            title = title,
            subtitle = "账号：$userId",
            avatarUrl = currentUser?.avatarUrl.orEmpty(),
            detail = currentUser?.signature.orEmpty(),
            userId = userId,
            chatType = "p2p",
            gender = currentUser?.gender.orEmpty(),
            region = currentUser?.region.orEmpty(),
            signature = currentUser?.signature.orEmpty(),
            nickname = currentUser?.nickname.orEmpty()
        )
    }

    val senderId = message.fromUserId.ifBlank { message.peerId }
    val friend = friendsById[senderId]
    return AvatarProfile(
        title = friend?.displayName() ?: senderId,
        subtitle = "账号：$senderId",
        avatarUrl = friend?.avatarUrl.orEmpty(),
        detail = friend?.signature.orEmpty(),
        userId = senderId,
        chatType = "p2p",
        gender = friend?.gender.orEmpty(),
        region = friend?.region.orEmpty(),
        signature = friend?.signature.orEmpty(),
        nickname = friend?.nickname.orEmpty(),
        remark = friend?.remark.orEmpty()
    )
}

private fun messageDisplayText(message: ChatMessageEntity): String {
    return when (message.contentType) {
        "image" -> "[图片]"
        "voice" -> "[语音]"
        "video" -> "[视频暂未支持播放]"
        "file" -> {
            val fileName = runCatching { JSONObject(message.content).optString("file_name") }
                .getOrNull()
                .orEmpty()
            if (fileName.isBlank()) "[文件]" else "[文件] $fileName"
        }
        else -> message.content
    }
}

private fun statusText(status: String): String {
    return when {
        status == "sending" -> "发送中"
        status == "sent" -> "已发送"
        status == "delivered" -> "已送达"
        status == "read" -> "已读"
        status == "failed" -> "发送失败"
        status.startsWith("failed:") -> "发送失败：${status.removePrefix("failed:")}"
        else -> ""
    }
}

private fun messageTimeLabel(message: ChatMessageEntity): String {
    if (message.serverTime.isNotBlank()) return message.serverTime
    val millis = when {
        message.serverTimestamp > 1_000_000_000_000L -> message.serverTimestamp
        message.clientTime > 0L -> message.clientTime * 1000L
        else -> 0L
    }
    if (millis <= 0L) return ""
    return runCatching {
        Instant.ofEpochMilli(millis)
            .atZone(ZoneId.systemDefault())
            .format(MessageTimeFormatter)
    }.getOrDefault("")
}

private fun FriendEntity.displayName(): String = remark.ifBlank { nickname }.ifBlank { userId }

private fun GroupEntity.displayName(): String = groupName.ifBlank { groupId }

private val MessageTimeFormatter: DateTimeFormatter = DateTimeFormatter.ofPattern("HH:mm:ss")
