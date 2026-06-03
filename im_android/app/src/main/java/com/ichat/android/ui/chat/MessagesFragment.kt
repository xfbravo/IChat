package com.ichat.android.ui.chat

import android.content.Context
import android.content.ContentValues
import android.content.ContentUris
import android.graphics.BitmapFactory
import android.graphics.SurfaceTexture
import android.media.MediaPlayer
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.provider.MediaStore
import android.util.Base64
import android.view.LayoutInflater
import android.view.Surface
import android.view.TextureView
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.PickVisualMediaRequest
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.scaleIn
import androidx.compose.animation.scaleOut
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.key
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.runtime.snapshotFlow
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.Outline
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.Density
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.fragment.app.viewModels
import androidx.core.content.FileProvider
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
import kotlinx.coroutines.delay
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.io.File
import java.io.FileInputStream
import java.lang.ref.WeakReference
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
    val activeChatTarget by mainViewModel.activeChatTarget.collectAsState()
    val conversations by viewModel.conversations.collectAsState()
    val friends by viewModel.friends.collectAsState()
    val groups by viewModel.groups.collectAsState()
    val selected by viewModel.selectedTarget.collectAsState()
    val messages by viewModel.messages.collectAsState()
    val draft by viewModel.draft.collectAsState()
    val actionStatus by viewModel.actionStatus.collectAsState()
    val currentUser by viewModel.currentUser.collectAsState()
    val friendsById = remember(friends) { friends.associateBy { it.userId } }
    val groupsById = remember(groups) { groups.associateBy { it.groupId } }
    var showAddFriend by remember { mutableStateOf(false) }
    var showCreateGroup by remember { mutableStateOf(false) }

    LaunchedEffect(pending) {
        val target = pending ?: return@LaunchedEffect
        mainViewModel.setActiveChatTarget(target)
        viewModel.openConversation(target)
        mainViewModel.consumePendingChatTarget()
    }

    val selectedTarget = selected
    LaunchedEffect(activeChatTarget?.conversationKey, selectedTarget?.conversationKey) {
        val target = activeChatTarget ?: return@LaunchedEffect
        if (selectedTarget == null) {
            viewModel.openConversation(target)
        }
    }
    LaunchedEffect(selectedTarget) {
        mainViewModel.setBottomNavigationVisible(selectedTarget == null)
        if (selectedTarget != null) {
            mainViewModel.setActiveChatTarget(selectedTarget)
        }
    }
    DisposableEffect(Unit) {
        onDispose {
            viewModel.clearActiveConversation()
            mainViewModel.setBottomNavigationVisible(true)
        }
    }
    BackHandler(enabled = selectedTarget != null) {
        mainViewModel.clearActiveChatTarget()
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
        key(selectedTarget.conversationKey) {
            ChatPanel(
                target = selectedTarget,
                messages = messages,
                draft = draft,
                actionStatus = actionStatus,
                currentUser = currentUser,
                friendsById = friendsById,
                groupsById = groupsById,
                onDraftChange = viewModel::updateDraft,
                onSend = viewModel::sendText,
                onSendAttachment = { uri -> viewModel.sendAttachment(uri, selectedTarget ?: activeChatTarget) },
                onDownloadAttachment = viewModel::downloadAttachment,
                onLoadOlder = viewModel::loadOlderMessages,
                onBack = {
                    mainViewModel.clearActiveChatTarget()
                    viewModel.backToConversations()
                    mainViewModel.setBottomNavigationVisible(true)
                },
                onAvatarClick = { clicked ->
                    onOpenProfile(clicked, clicked.userId == currentUser?.userId && clicked.chatType != "group")
                }
            )
        }
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
                            UnreadBadge(conversation.unreadCount)
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun UnreadBadge(count: Int) {
    val label = if (count > 99) "99+" else count.toString()
    Surface(
        modifier = if (count > 99) {
            Modifier.height(22.dp).widthIn(min = 30.dp)
        } else {
            Modifier.size(22.dp)
        },
        shape = RoundedCornerShape(11.dp),
        color = Color(0xFFE53935)
    ) {
        Box(
            modifier = Modifier.padding(horizontal = if (count > 99) 6.dp else 0.dp),
            contentAlignment = Alignment.Center
        ) {
            Text(
                text = label,
                color = Color.White,
                fontSize = 12.sp,
                fontWeight = FontWeight.Bold,
                lineHeight = 14.sp
            )
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
    actionStatus: String,
    currentUser: CurrentUser?,
    friendsById: Map<String, FriendEntity>,
    groupsById: Map<String, GroupEntity>,
    onDraftChange: (String) -> Unit,
    onSend: () -> Unit,
    onSendAttachment: (Uri) -> Unit,
    onDownloadAttachment: (ChatMessageEntity) -> Unit,
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
    val coroutineScope = rememberCoroutineScope()
    var isAtBottom by remember(target.conversationKey) { mutableStateOf(true) }
    var pendingIncomingCount by remember(target.conversationKey) { mutableStateOf(0) }
    var handledLatestMessageId by remember(target.conversationKey) { mutableStateOf<String?>(null) }
    var fullscreenPreview by remember(target.conversationKey) { mutableStateOf<MediaPreview?>(null) }
    var showAttachmentPanel by remember(target.conversationKey) { mutableStateOf(false) }
    var showFilePanel by remember(target.conversationKey) { mutableStateOf(false) }
    val context = LocalContext.current
    var pendingCameraUri by remember { mutableStateOf<Uri?>(null) }
    val photoPicker = rememberLauncherForActivityResult(ActivityResultContracts.PickVisualMedia()) { uri ->
        if (uri != null) {
            showAttachmentPanel = false
            showFilePanel = false
            onSendAttachment(uri)
        }
    }
    val fileFallbackPicker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) {
            showAttachmentPanel = false
            showFilePanel = false
            onSendAttachment(uri)
        } else {
            showAttachmentPanel = true
            showFilePanel = true
        }
    }
    val cameraLauncher = rememberLauncherForActivityResult(ActivityResultContracts.TakePicture()) { saved ->
        val uri = pendingCameraUri
        pendingCameraUri = null
        if (saved && uri != null) {
            showAttachmentPanel = false
            showFilePanel = false
            onSendAttachment(uri)
        }
    }

    LaunchedEffect(latestMessage?.msgId) {
        val latest = latestMessage ?: return@LaunchedEffect
        if (handledLatestMessageId == latest.msgId) return@LaunchedEffect

        val hasHandledInitialMessage = handledLatestMessageId != null
        handledLatestMessageId = latest.msgId
        if (!hasHandledInitialMessage) return@LaunchedEffect

        if (latest.isMine || isAtBottom) {
            pendingIncomingCount = 0
            listState.scrollToItem(0)
        } else {
            pendingIncomingCount += 1
        }
    }
    LaunchedEffect(target.conversationKey, listState) {
        snapshotFlow {
            listState.firstVisibleItemIndex == 0 && listState.firstVisibleItemScrollOffset <= 4
        }.distinctUntilChanged().collect { atBottom ->
            isAtBottom = atBottom
            if (atBottom) {
                pendingIncomingCount = 0
            }
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
    BackHandler(enabled = showFilePanel) {
        showFilePanel = false
    }
    BackHandler(enabled = showAttachmentPanel && !showFilePanel) {
        showAttachmentPanel = false
    }

    Box(Modifier.fillMaxSize()) {
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
        Box(
            modifier = Modifier
                .weight(1f)
                .fillMaxWidth()
                .background(Color(0xFFF5F7F5))
        ) {
            LazyColumn(
                state = listState,
                reverseLayout = true,
                modifier = Modifier
                    .fillMaxSize()
                    .padding(horizontal = 12.dp, vertical = 8.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                items(displayMessages, key = { it.msgId }) { message ->
                    MessageRow(
                        message = message,
                        currentUser = currentUser,
                        friendsById = friendsById,
                        onAvatarClick = onAvatarClick,
                        onOpenPreview = { fullscreenPreview = it },
                        onDownloadAttachment = onDownloadAttachment
                    )
                    }
                }
            if (pendingIncomingCount > 0) {
                NewMessageNotice(
                    count = pendingIncomingCount,
                    modifier = Modifier
                        .align(Alignment.BottomCenter)
                        .padding(bottom = 8.dp),
                    onClick = {
                        pendingIncomingCount = 0
                        coroutineScope.launch { listState.scrollToItem(0) }
                    }
                )
            }
        }
        ChatInputBar(
            draft = draft,
            attachmentPanelVisible = showAttachmentPanel,
            onDraftChange = onDraftChange,
            onSend = onSend,
            onToggleAttachmentPanel = { showAttachmentPanel = !showAttachmentPanel }
        )
        if (actionStatus.isNotBlank()) {
            Text(
                text = actionStatus,
                modifier = Modifier.padding(start = 14.dp, end = 14.dp, bottom = 8.dp),
                color = Color(0xFF6B756E),
                fontSize = 12.sp
            )
        }
        if (showAttachmentPanel) {
            if (showFilePanel) {
                ChatFilePickerPanel(
                    context = context,
                    onBack = { showFilePanel = false },
                    onCancel = {
                        showAttachmentPanel = false
                        showFilePanel = false
                    },
                    onBrowseSystem = { fileFallbackPicker.launch(arrayOf("*/*")) },
                    onPickFile = { uri ->
                        showAttachmentPanel = false
                        showFilePanel = false
                        onSendAttachment(uri)
                    }
                )
            } else {
                ChatAttachmentPanel(
                    onPickPhoto = {
                        photoPicker.launch(
                            PickVisualMediaRequest(ActivityResultContracts.PickVisualMedia.ImageOnly)
                        )
                    },
                    onTakePhoto = {
                        val uri = createCameraImageUri(context)
                        pendingCameraUri = uri
                        cameraLauncher.launch(uri)
                    },
                    onPickFile = { showFilePanel = true }
                )
            }
        }
        }
        fullscreenPreview?.let { preview ->
            FullscreenMediaOverlay(
                preview = preview,
                onDismiss = { fullscreenPreview = null }
            )
        }
    }
}

@Composable
private fun ChatInputBar(
    draft: String,
    attachmentPanelVisible: Boolean,
    onDraftChange: (String) -> Unit,
    onSend: () -> Unit,
    onToggleAttachmentPanel: () -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .background(Color.White)
            .padding(horizontal = 10.dp, vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Surface(
            modifier = Modifier
                .weight(1f)
                .height(40.dp),
            shape = RoundedCornerShape(20.dp),
            color = Color(0xFFF5F7F5),
            border = BorderStroke(1.dp, Color(0xFFDCE6DE))
        ) {
            BasicTextField(
                value = draft,
                onValueChange = onDraftChange,
                modifier = Modifier
                    .fillMaxSize()
                    .padding(horizontal = 14.dp),
                singleLine = true,
                textStyle = MaterialTheme.typography.bodyMedium.copy(color = Color(0xFF111111)),
                keyboardOptions = KeyboardOptions(imeAction = ImeAction.Send),
                keyboardActions = KeyboardActions(onSend = { onSend() }),
                decorationBox = { innerTextField ->
                    Box(Modifier.fillMaxSize(), contentAlignment = Alignment.CenterStart) {
                        if (draft.isBlank()) {
                            Text("输入消息", color = Color(0xFF9AA59E), fontSize = 14.sp)
                        }
                        innerTextField()
                    }
                }
            )
        }
        Spacer(Modifier.width(8.dp))
        PlusIconButton(
            expanded = attachmentPanelVisible,
            onClick = onToggleAttachmentPanel
        )
    }
}

@Composable
private fun PlusIconButton(expanded: Boolean, onClick: () -> Unit) {
    Surface(
        modifier = Modifier
            .size(40.dp)
            .clickable(onClick = onClick),
        shape = RoundedCornerShape(20.dp),
        color = if (expanded) Color(0xFFE0F3DB) else Color(0xFF2F6F3E),
        border = BorderStroke(1.dp, if (expanded) Color(0xFFB8DDB7) else Color(0xFF2F6F3E))
    ) {
        Canvas(Modifier.fillMaxSize()) {
            val stroke = 2.4.dp.toPx()
            val color = if (expanded) Color(0xFF2F6F3E) else Color.White
            drawLine(
                color = color,
                start = Offset(size.width * 0.30f, size.height * 0.50f),
                end = Offset(size.width * 0.70f, size.height * 0.50f),
                strokeWidth = stroke
            )
            if (!expanded) {
                drawLine(
                    color = color,
                    start = Offset(size.width * 0.50f, size.height * 0.30f),
                    end = Offset(size.width * 0.50f, size.height * 0.70f),
                    strokeWidth = stroke
                )
            }
        }
    }
}

@Composable
private fun ChatAttachmentPanel(
    onPickPhoto: () -> Unit,
    onTakePhoto: () -> Unit,
    onPickFile: () -> Unit
) {
    Surface(
        modifier = Modifier
            .fillMaxWidth()
            .height(116.dp),
        color = Color.White,
        border = BorderStroke(1.dp, Color(0xFFE3ECE5))
    ) {
        Row(
            modifier = Modifier
                .fillMaxSize()
                .padding(horizontal = 22.dp, vertical = 14.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            AttachmentActionButton(label = "照片", icon = AttachmentIcon.Photo, onClick = onPickPhoto)
            AttachmentActionButton(label = "拍摄", icon = AttachmentIcon.Camera, onClick = onTakePhoto)
            AttachmentActionButton(label = "文件", icon = AttachmentIcon.File, onClick = onPickFile)
        }
    }
}

@Composable
private fun ChatFilePickerPanel(
    context: Context,
    onBack: () -> Unit,
    onCancel: () -> Unit,
    onBrowseSystem: () -> Unit,
    onPickFile: (Uri) -> Unit
) {
    val files by produceState<List<LocalFileItem>>(initialValue = emptyList(), context) {
        value = withContext(Dispatchers.IO) { loadRecentLocalFiles(context) }
    }
    Surface(
        modifier = Modifier
            .fillMaxWidth()
            .heightIn(min = 260.dp, max = 420.dp),
        color = Color.White,
        border = BorderStroke(1.dp, Color(0xFFE3ECE5))
    ) {
        Column(Modifier.fillMaxWidth()) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 14.dp, vertical = 10.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = "返回",
                    modifier = Modifier
                        .clickable(onClick = onBack)
                        .padding(horizontal = 8.dp, vertical = 6.dp),
                    color = Color(0xFF2F6F3E),
                    fontWeight = FontWeight.SemiBold
                )
                Spacer(Modifier.width(8.dp))
                Text("选择文件", fontWeight = FontWeight.Bold, color = Color(0xFF17211C))
                Spacer(Modifier.weight(1f))
                Text(
                    text = "取消",
                    modifier = Modifier
                        .clickable(onClick = onCancel)
                        .padding(horizontal = 8.dp, vertical = 6.dp),
                    color = Color(0xFF2F6F3E),
                    fontWeight = FontWeight.SemiBold
                )
            }
            HorizontalDivider(color = Color(0xFFE3ECE5))
            SystemFilePickerRow(onClick = onBrowseSystem)
            HorizontalDivider(color = Color(0xFFE3ECE5))
            if (files.isEmpty()) {
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(210.dp)
                        .padding(18.dp),
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        text = "没有读取到可发送的本地文件",
                        color = Color(0xFF6B756E),
                        fontSize = 14.sp
                    )
                }
            } else {
                LazyColumn(
                    modifier = Modifier
                        .fillMaxWidth()
                        .heightIn(max = 350.dp)
                ) {
                    items(files, key = { it.uri.toString() }) { file ->
                        FilePickerRow(file = file, onClick = { onPickFile(file.uri) })
                    }
                }
            }
        }
    }
}

@Composable
private fun SystemFilePickerRow(onClick: () -> Unit) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(onClick = onClick)
            .padding(horizontal = 16.dp, vertical = 12.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        FileIcon()
        Spacer(Modifier.width(12.dp))
        Column(Modifier.weight(1f)) {
            Text(
                text = "系统文件",
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
                fontWeight = FontWeight.SemiBold,
                color = Color(0xFF17211C)
            )
            Text(
                text = "从手机文件管理器中选择",
                color = Color(0xFF6B756E),
                fontSize = 12.sp
            )
        }
        Text("打开", color = Color(0xFF2F6F3E), fontWeight = FontWeight.SemiBold)
    }
}

@Composable
private fun FilePickerRow(file: LocalFileItem, onClick: () -> Unit) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(onClick = onClick)
            .padding(horizontal = 16.dp, vertical = 10.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        FileIcon()
        Spacer(Modifier.width(12.dp))
        Column(Modifier.weight(1f)) {
            Text(
                text = file.name,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
                fontWeight = FontWeight.SemiBold,
                color = Color(0xFF17211C)
            )
            Text(
                text = humanReadableFileSize(file.size),
                color = Color(0xFF6B756E),
                fontSize = 12.sp
            )
        }
    }
}

@Composable
private fun AttachmentActionButton(label: String, icon: AttachmentIcon, onClick: () -> Unit) {
    Column(
        modifier = Modifier
            .width(76.dp)
            .clickable(onClick = onClick),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Surface(
            modifier = Modifier.size(54.dp),
            shape = RoundedCornerShape(8.dp),
            color = Color(0xFFF5F7F5),
            border = BorderStroke(1.dp, Color(0xFFDCE6DE))
        ) {
            Canvas(Modifier.fillMaxSize()) {
                val ink = Color(0xFF2F6F3E)
                when (icon) {
                    AttachmentIcon.Photo -> {
                        drawRect(ink, topLeft = Offset(size.width * 0.24f, size.height * 0.28f), size = Size(size.width * 0.52f, size.height * 0.42f))
                        drawCircle(Color.White, radius = size.width * 0.07f, center = Offset(size.width * 0.62f, size.height * 0.40f))
                    }
                    AttachmentIcon.Camera -> {
                        drawRect(ink, topLeft = Offset(size.width * 0.20f, size.height * 0.34f), size = Size(size.width * 0.60f, size.height * 0.34f))
                        drawRect(ink, topLeft = Offset(size.width * 0.34f, size.height * 0.25f), size = Size(size.width * 0.22f, size.height * 0.10f))
                        drawCircle(Color.White, radius = size.width * 0.11f, center = Offset(size.width * 0.50f, size.height * 0.51f))
                    }
                    AttachmentIcon.File -> {
                        val path = Path().apply {
                            moveTo(size.width * 0.32f, size.height * 0.22f)
                            lineTo(size.width * 0.58f, size.height * 0.22f)
                            lineTo(size.width * 0.72f, size.height * 0.36f)
                            lineTo(size.width * 0.72f, size.height * 0.76f)
                            lineTo(size.width * 0.32f, size.height * 0.76f)
                            close()
                        }
                        drawPath(path, ink)
                        drawLine(Color.White, Offset(size.width * 0.42f, size.height * 0.48f), Offset(size.width * 0.62f, size.height * 0.48f), strokeWidth = 2.dp.toPx())
                        drawLine(Color.White, Offset(size.width * 0.42f, size.height * 0.58f), Offset(size.width * 0.62f, size.height * 0.58f), strokeWidth = 2.dp.toPx())
                    }
                }
            }
        }
        Spacer(Modifier.height(6.dp))
        Text(label, color = Color(0xFF17211C), fontSize = 13.sp)
    }
}

private enum class AttachmentIcon {
    Photo,
    Camera,
    File
}

private data class LocalFileItem(
    val uri: Uri,
    val name: String,
    val size: Long,
    val modified: Long
)

private fun loadRecentLocalFiles(context: Context): List<LocalFileItem> {
    val collections = buildList {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            add(MediaStore.Downloads.EXTERNAL_CONTENT_URI)
        }
        add(MediaStore.Images.Media.EXTERNAL_CONTENT_URI)
        add(MediaStore.Video.Media.EXTERNAL_CONTENT_URI)
        add(MediaStore.Audio.Media.EXTERNAL_CONTENT_URI)
        add(MediaStore.Files.getContentUri("external"))
    }.distinctBy { it.toString() }
    val projection = arrayOf(
        MediaStore.MediaColumns._ID,
        MediaStore.MediaColumns.DISPLAY_NAME,
        MediaStore.MediaColumns.SIZE,
        MediaStore.MediaColumns.DATE_MODIFIED
    )
    val sortOrder = "${MediaStore.MediaColumns.DATE_MODIFIED} DESC"
    val result = linkedMapOf<String, LocalFileItem>()
    collections.forEach { collection ->
        runCatching {
            context.contentResolver.query(collection, projection, null, null, sortOrder)?.use { cursor ->
                val idIndex = cursor.getColumnIndexOrThrow(MediaStore.MediaColumns._ID)
                val nameIndex = cursor.getColumnIndexOrThrow(MediaStore.MediaColumns.DISPLAY_NAME)
                val sizeIndex = cursor.getColumnIndex(MediaStore.MediaColumns.SIZE)
                val modifiedIndex = cursor.getColumnIndex(MediaStore.MediaColumns.DATE_MODIFIED)
                while (cursor.moveToNext() && result.size < 120) {
                    val id = cursor.getLong(idIndex)
                    val name = cursor.getString(nameIndex).orEmpty()
                    if (name.isBlank()) continue
                    val uri = ContentUris.withAppendedId(collection, id)
                    val key = "${name.lowercase()}:${if (sizeIndex >= 0) cursor.getLong(sizeIndex) else 0L}"
                    if (result.containsKey(key)) continue
                    result[key] =
                        LocalFileItem(
                            uri = uri,
                            name = name,
                            size = if (sizeIndex >= 0) cursor.getLong(sizeIndex).coerceAtLeast(0L) else 0L,
                            modified = if (modifiedIndex >= 0) cursor.getLong(modifiedIndex) else 0L
                        )
                }
            }
        }
    }
    return result.values.sortedByDescending { it.modified }.take(120)
}

private fun humanReadableFileSize(size: Long): String {
    if (size <= 0L) return "大小未知"
    val units = listOf("B", "KB", "MB", "GB")
    var value = size.toDouble()
    var unitIndex = 0
    while (value >= 1024.0 && unitIndex < units.lastIndex) {
        value /= 1024.0
        unitIndex += 1
    }
    return if (unitIndex == 0) {
        "${value.toLong()} ${units[unitIndex]}"
    } else {
        String.format("%.1f %s", value, units[unitIndex])
    }
}

// The system camera writes into app cache through FileProvider, then the normal image upload path sends it.
private fun createCameraImageUri(context: Context): Uri {
    val root = File(context.cacheDir, "ichat_camera").apply { mkdirs() }
    val photo = File.createTempFile("ichat_camera_${System.currentTimeMillis()}_", ".jpg", root)
    return FileProvider.getUriForFile(context, "${context.packageName}.fileprovider", photo)
}

@Composable
private fun NewMessageNotice(count: Int, modifier: Modifier = Modifier, onClick: () -> Unit) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .padding(horizontal = 12.dp, vertical = 6.dp),
        horizontalArrangement = Arrangement.Center
    ) {
        Surface(
            modifier = Modifier.clickable(onClick = onClick),
            shape = RoundedCornerShape(14.dp),
            color = Color.White,
            border = BorderStroke(1.dp, Color(0xFFB9DFB8))
        ) {
            Text(
                text = "对方发送${count}条新消息",
                modifier = Modifier.padding(horizontal = 14.dp, vertical = 6.dp),
                color = Color(0xFF2F6F3E),
                fontSize = 13.sp,
                fontWeight = FontWeight.SemiBold
            )
        }
    }
}

@Composable
private fun MessageRow(
    message: ChatMessageEntity,
    currentUser: CurrentUser?,
    friendsById: Map<String, FriendEntity>,
    onAvatarClick: (AvatarProfile) -> Unit,
    onOpenPreview: (MediaPreview) -> Unit,
    onDownloadAttachment: (ChatMessageEntity) -> Unit
) {
    val profile = senderProfile(message, currentUser, friendsById)
    val columnAlignment = if (message.isMine) Alignment.End else Alignment.Start

    BoxWithConstraints(Modifier.fillMaxWidth()) {
        val maxBubbleWidth = maxWidth * 0.68f
        Column(
            modifier = Modifier.fillMaxWidth(),
            horizontalAlignment = columnAlignment
        ) {
            val time = messageTimeLabel(message)
            if (time.isNotBlank()) {
                Text(time, color = Color(0xFF888888), fontSize = 12.sp)
                Spacer(Modifier.height(3.dp))
            }
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = if (message.isMine) Arrangement.End else Arrangement.Start,
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
                MessageContent(
                    message = message,
                    maxWidth = maxBubbleWidth,
                    onOpenPreview = onOpenPreview,
                    onDownloadAttachment = onDownloadAttachment
                )
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
            val status = statusText(message.sendStatus)
            if (message.isMine && status.isNotBlank()) {
                Spacer(Modifier.height(3.dp))
                Text(status, color = Color(0xFF888888), fontSize = 12.sp)
            }
        }
    }
}

@Composable
private fun MessageContent(
    message: ChatMessageEntity,
    maxWidth: Dp,
    onOpenPreview: (MediaPreview) -> Unit,
    onDownloadAttachment: (ChatMessageEntity) -> Unit
) {
    if (message.contentType == "image" || message.contentType == "file" || message.contentType == "video") {
        MediaMessageCard(
            message = message,
            maxWidth = maxWidth,
            onOpenPreview = onOpenPreview,
            onDownloadAttachment = onDownloadAttachment
        )
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
        color = if (isMine) Color(0xFFE0F3DB) else Color.White,
        border = BorderStroke(1.dp, if (isMine) Color(0xFFB8DDB7) else Color(0xFFDFE7E1))
    ) {
        Text(
            text = text,
            modifier = Modifier.padding(
                start = if (isMine) 12.dp else 18.dp,
                top = 7.dp,
                end = if (isMine) 18.dp else 12.dp,
                bottom = 7.dp
            ),
            color = Color(0xFF111111),
            lineHeight = 19.sp
        )
    }
}

private class ChatBubbleShape(private val isMine: Boolean) : Shape {
    override fun createOutline(size: Size, layoutDirection: LayoutDirection, density: Density): Outline {
        val tailWidth = with(density) { 7.dp.toPx() }.coerceAtMost(size.width / 3f)
        val radius = with(density) { 4.dp.toPx() }
        val tailHeight = with(density) { 10.dp.toPx() }.coerceAtMost(size.height * 0.55f)
        // 气泡和头像顶端对齐时，尾巴固定锚在头像中心附近；长文本不会把尾巴挤到头像下方。
        val preferredTailMid = with(density) { 18.dp.toPx() }
        val minTailMid = tailHeight / 2f
        val maxTailMid = (size.height - tailHeight / 2f).coerceAtLeast(minTailMid)
        val tailMid = preferredTailMid.coerceIn(minTailMid, maxTailMid)
        val tailTop = tailMid - tailHeight / 2f
        val tailBottom = tailTop + tailHeight
        val bubbleRect = if (isMine) {
            Rect(0f, 0f, size.width - tailWidth, size.height)
        } else {
            Rect(tailWidth, 0f, size.width, size.height)
        }

        val path = Path().apply {
            if (isMine) {
                moveTo(bubbleRect.left + radius, bubbleRect.top)
                lineTo(bubbleRect.right - radius, bubbleRect.top)
                quadraticTo(bubbleRect.right, bubbleRect.top, bubbleRect.right, bubbleRect.top + radius)
                lineTo(bubbleRect.right, tailTop)
                lineTo(size.width, tailMid)
                lineTo(bubbleRect.right, tailBottom)
                lineTo(bubbleRect.right, bubbleRect.bottom - radius)
                quadraticTo(bubbleRect.right, bubbleRect.bottom, bubbleRect.right - radius, bubbleRect.bottom)
                lineTo(bubbleRect.left + radius, bubbleRect.bottom)
                quadraticTo(bubbleRect.left, bubbleRect.bottom, bubbleRect.left, bubbleRect.bottom - radius)
                lineTo(bubbleRect.left, bubbleRect.top + radius)
                quadraticTo(bubbleRect.left, bubbleRect.top, bubbleRect.left + radius, bubbleRect.top)
            } else {
                moveTo(bubbleRect.left + radius, bubbleRect.top)
                lineTo(bubbleRect.right - radius, bubbleRect.top)
                quadraticTo(bubbleRect.right, bubbleRect.top, bubbleRect.right, bubbleRect.top + radius)
                lineTo(bubbleRect.right, bubbleRect.bottom - radius)
                quadraticTo(bubbleRect.right, bubbleRect.bottom, bubbleRect.right - radius, bubbleRect.bottom)
                lineTo(bubbleRect.left + radius, bubbleRect.bottom)
                quadraticTo(bubbleRect.left, bubbleRect.bottom, bubbleRect.left, bubbleRect.bottom - radius)
                lineTo(bubbleRect.left, tailBottom)
                lineTo(0f, tailMid)
                lineTo(bubbleRect.left, tailTop)
                lineTo(bubbleRect.left, bubbleRect.top + radius)
                quadraticTo(bubbleRect.left, bubbleRect.top, bubbleRect.left + radius, bubbleRect.top)
            }
            close()
        }

        return Outline.Generic(path)
    }
}

@Composable
private fun MediaMessageCard(
    message: ChatMessageEntity,
    maxWidth: Dp,
    onOpenPreview: (MediaPreview) -> Unit,
    onDownloadAttachment: (ChatMessageEntity) -> Unit
) {
    when (message.contentType) {
        "image" -> ImageMessageBubble(message, maxWidth, onOpenPreview, onDownloadAttachment)
        "video" -> VideoMessageBubble(message, maxWidth, onOpenPreview, onDownloadAttachment)
        else -> FileMessageCard(message, maxWidth, onDownloadAttachment)
    }
}

@Composable
private fun ImageMessageBubble(
    message: ChatMessageEntity,
    maxWidth: Dp,
    onOpenPreview: (MediaPreview) -> Unit,
    onDownloadAttachment: (ChatMessageEntity) -> Unit
) {
    val file = remember(message.content) { messageContentObject(message.content) }
    val previewData = file.optString("preview_data_url")
    val imagePath = message.localPath?.takeIf { File(it).exists() }
    val bitmap = remember(imagePath, previewData) { decodeMediaBitmap(imagePath, previewData) }
    val imageWidth = if (maxWidth < 240.dp) maxWidth else 240.dp
    val imageHeight = bitmap?.let {
        val ratio = it.width.toFloat().coerceAtLeast(1f) / it.height.toFloat().coerceAtLeast(1f)
        (imageWidth.value / ratio).dp.coerceIn(96.dp, 300.dp)
    } ?: 150.dp
    Surface(
        modifier = Modifier
            .width(imageWidth)
            .height(imageHeight)
            .clickable {
                if (bitmap != null) {
                    onOpenPreview(MediaPreview("image", imagePath, previewData, file.optString("file_name", "image")))
                } else {
                    onDownloadAttachment(message)
                }
            },
        shape = RoundedCornerShape(6.dp),
        color = Color(0xFFE9EEE9),
        border = BorderStroke(1.dp, Color(0xFFD9E2DC))
    ) {
        if (bitmap != null) {
            Image(
                bitmap = bitmap,
                contentDescription = file.optString("file_name", "image"),
                modifier = Modifier.fillMaxSize(),
                contentScale = ContentScale.Crop
            )
        } else {
            Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                Text("图片未下载", color = Color(0xFF6B756E), fontSize = 13.sp)
            }
        }
    }
}

@Composable
private fun VideoMessageBubble(
    message: ChatMessageEntity,
    maxWidth: Dp,
    onOpenPreview: (MediaPreview) -> Unit,
    onDownloadAttachment: (ChatMessageEntity) -> Unit
) {
    val file = remember(message.content) { messageContentObject(message.content) }
    val posterData = file.optString("poster_data_url")
    val poster = remember(posterData) { decodeMediaBitmap(null, posterData) }
    val localPath = message.localPath?.takeIf { File(it).exists() && message.transferStatus == "downloaded" }
    val status = mediaStatusLabel(message)
    Surface(
        modifier = Modifier
            .width(if (maxWidth < 260.dp) maxWidth else 260.dp)
            .height(150.dp)
            .clickable {
                if (localPath != null) {
                    onOpenPreview(MediaPreview("video", localPath, posterData, file.optString("file_name", "video")))
                } else if (message.transferStatus != "downloading") {
                    onDownloadAttachment(message)
                }
            },
        shape = RoundedCornerShape(6.dp),
        color = Color(0xFF111111)
    ) {
        Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
            if (poster != null) {
                Image(
                    bitmap = poster,
                    contentDescription = file.optString("file_name", "video"),
                    modifier = Modifier.fillMaxSize(),
                    contentScale = ContentScale.Crop
                )
            }
            PlayButtonMark()
            if (status.isNotBlank()) {
                Text(
                    text = status,
                    modifier = Modifier
                        .align(Alignment.BottomStart)
                        .background(Color(0xAA000000))
                        .padding(horizontal = 8.dp, vertical = 4.dp),
                    color = Color.White,
                    fontSize = 12.sp
                )
            }
        }
    }
}

@Composable
private fun FileMessageCard(
    message: ChatMessageEntity,
    maxWidth: Dp,
    onDownloadAttachment: (ChatMessageEntity) -> Unit
) {
    val file = remember(message.content) { messageContentObject(message.content) }
    val fileName = file.optString("file_name").ifBlank { "file" }
    val status = mediaStatusLabel(message).ifBlank { if (isDownloaded(message)) "已下载" else "未下载" }
    Surface(
        modifier = Modifier
            .widthIn(max = maxWidth)
            .clickable {
                if (!isDownloaded(message) && message.transferStatus != "downloading") {
                    onDownloadAttachment(message)
                }
            },
        shape = ChatBubbleShape(message.isMine),
        color = if (message.isMine) Color(0xFFE0F3DB) else MaterialTheme.colorScheme.surface,
        border = BorderStroke(1.dp, if (message.isMine) Color(0xFFB8DDB7) else Color(0xFFDFE7E1))
    ) {
        Row(
            modifier = Modifier.padding(
                start = if (message.isMine) 12.dp else 18.dp,
                top = 10.dp,
                end = if (message.isMine) 18.dp else 12.dp,
                bottom = 10.dp
            ),
            verticalAlignment = Alignment.CenterVertically
        ) {
            FileIcon()
            Spacer(Modifier.width(10.dp))
            Column(Modifier.widthIn(max = maxWidth - 76.dp)) {
                Text(
                    text = fileName,
                    maxLines = 2,
                    overflow = TextOverflow.Ellipsis,
                    fontWeight = FontWeight.SemiBold,
                    color = Color(0xFF17211C)
                )
                Text(status, color = Color(0xFF6B756E), fontSize = 12.sp)
            }
        }
    }
}

@Composable
private fun FileIcon() {
    Surface(
        modifier = Modifier.size(42.dp),
        shape = RoundedCornerShape(4.dp),
        color = Color(0xFF2F6F3E)
    ) {
        Box(contentAlignment = Alignment.Center) {
            Text("FILE", color = Color.White, fontSize = 11.sp, fontWeight = FontWeight.Bold)
        }
    }
}

@Composable
private fun PlayButtonMark() {
    Canvas(Modifier.size(58.dp)) {
        drawCircle(Color(0xAA000000))
        val triangle = Path().apply {
            moveTo(size.width * 0.42f, size.height * 0.32f)
            lineTo(size.width * 0.42f, size.height * 0.68f)
            lineTo(size.width * 0.70f, size.height * 0.50f)
            close()
        }
        drawPath(triangle, Color.White)
    }
}

@Composable
private fun FullscreenMediaOverlay(preview: MediaPreview, onDismiss: () -> Unit) {
    val context = LocalContext.current
    val coroutineScope = rememberCoroutineScope()
    var visible by remember(preview) { mutableStateOf(false) }
    var saveStatus by remember(preview) { mutableStateOf("") }
    LaunchedEffect(preview) { visible = true }
    LaunchedEffect(visible) {
        if (!visible) {
            delay(220)
            onDismiss()
        }
    }

    AnimatedVisibility(
        visible = visible,
        enter = if (preview.type == "video") fadeIn(tween(140)) else fadeIn(tween(220)) + scaleIn(tween(220), initialScale = 0.88f),
        exit = if (preview.type == "video") fadeOut(tween(140)) else fadeOut(tween(220)) + scaleOut(tween(220), targetScale = 0.88f)
    ) {
        Box(
            modifier = Modifier
                .fillMaxSize()
                .background(Color.Black)
                .clickable { visible = false },
            contentAlignment = Alignment.Center
        ) {
            Column(Modifier.fillMaxSize()) {
                Box(
                    modifier = Modifier
                        .weight(1f)
                        .fillMaxWidth()
                        .padding(8.dp),
                    contentAlignment = Alignment.Center
                ) {
                    if (preview.type == "video" && preview.localPath != null) {
                        val path = preview.localPath
                        AndroidView(
                            factory = { viewContext ->
                                TextureVideoPlayerView(viewContext).apply {
                                    play(File(path))
                                    setOnClickListener { visible = false }
                                }
                            },
                            update = { it.play(File(path)) },
                            modifier = Modifier
                                .fillMaxWidth()
                                .aspectRatio(16f / 9f)
                        )
                    } else {
                        val bitmap = remember(preview) { decodeMediaBitmap(preview.localPath, preview.dataUrl) }
                        if (bitmap != null) {
                            Image(
                                bitmap = bitmap,
                                contentDescription = preview.fileName,
                                modifier = Modifier.fillMaxSize(),
                                contentScale = ContentScale.Fit
                            )
                        }
                    }
                }
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .background(Color(0xDD000000))
                        .padding(horizontal = 18.dp, vertical = 14.dp),
                    horizontalAlignment = Alignment.CenterHorizontally
                ) {
                    Surface(
                        modifier = Modifier
                            .width(160.dp)
                            .height(42.dp)
                            .clickable {
                                coroutineScope.launch {
                                    saveStatus = "正在保存..."
                                    saveStatus = savePreviewToGallery(context, preview)
                                }
                            },
                        shape = RoundedCornerShape(21.dp),
                        color = Color.White
                    ) {
                        Box(contentAlignment = Alignment.Center) {
                            Text("保存到相册", color = Color(0xFF17211C), fontWeight = FontWeight.SemiBold)
                        }
                    }
                    if (saveStatus.isNotBlank()) {
                        Spacer(Modifier.height(8.dp))
                        Text(saveStatus, color = Color.White, fontSize = 12.sp)
                    }
                }
            }
        }
    }
}

private data class MediaPreview(
    val type: String,
    val localPath: String?,
    val dataUrl: String?,
    val fileName: String
)

private class TextureVideoPlayerView(context: Context) : FrameLayout(context) {
    private val textureView = TextureView(context)
    private var mediaPlayer: MediaPlayer? = null
    private var playerSurface: Surface? = null
    private var pendingFile: File? = null
    private var currentPath: String = ""

    init {
        setBackgroundColor(android.graphics.Color.BLACK)
        addView(
            textureView,
            LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT)
        )
        textureView.surfaceTextureListener = VideoSurfaceListener(this)
    }

    fun play(file: File) {
        if (currentPath == file.absolutePath && mediaPlayer != null) return
        currentPath = file.absolutePath
        pendingFile = file
        textureView.surfaceTexture?.let { startPlayer(file, it) }
    }

    override fun onDetachedFromWindow() {
        releasePlayer()
        super.onDetachedFromWindow()
    }

    private fun startPlayer(file: File, surfaceTexture: SurfaceTexture) {
        releasePlayer()
        val surface = Surface(surfaceTexture)
        playerSurface = surface
        mediaPlayer = MediaPlayer().apply {
            setSurface(surface)
            setDataSource(file.absolutePath)
            setOnPreparedListener { player ->
                player.isLooping = false
                player.start()
            }
            setOnErrorListener { _, _, _ ->
                true
            }
            // prepareAsync keeps video open/decode setup off the main thread; VideoView did not give us that control.
            prepareAsync()
        }
    }

    private fun releasePlayer() {
        mediaPlayer?.let { player ->
            runCatching {
                player.stop()
                player.reset()
                player.release()
            }
        }
        mediaPlayer = null
        playerSurface?.release()
        playerSurface = null
    }

    private class VideoSurfaceListener(view: TextureVideoPlayerView) : TextureView.SurfaceTextureListener {
        private val viewRef = WeakReference(view)

        override fun onSurfaceTextureAvailable(surface: SurfaceTexture, width: Int, height: Int) {
            val view = viewRef.get() ?: return
            val file = view.pendingFile ?: return
            view.startPlayer(file, surface)
        }

        override fun onSurfaceTextureSizeChanged(surface: SurfaceTexture, width: Int, height: Int) = Unit

        override fun onSurfaceTextureDestroyed(surface: SurfaceTexture): Boolean {
            viewRef.get()?.releasePlayer()
            return true
        }

        override fun onSurfaceTextureUpdated(surface: SurfaceTexture) = Unit
    }
}

private fun messageContentObject(content: String): JSONObject {
    return runCatching { JSONObject(content) }.getOrDefault(JSONObject())
}

private fun decodeMediaBitmap(localPath: String?, dataUrl: String?): ImageBitmap? {
    return runCatching {
        val bitmap = localPath
            ?.takeIf { it.isNotBlank() }
            ?.let { File(it) }
            ?.takeIf { it.exists() }
            ?.let { BitmapFactory.decodeFile(it.absolutePath) }
            ?: decodeDataUrlBitmap(dataUrl.orEmpty())
        bitmap?.asImageBitmap()
    }.getOrNull()
}

private fun decodeDataUrlBitmap(value: String) = runCatching {
    val dataUrl = value.trim()
    if (!dataUrl.startsWith("data:image/", ignoreCase = true)) return@runCatching null
    val commaIndex = dataUrl.indexOf(',')
    if (commaIndex <= 0) return@runCatching null
    val bytes = Base64.decode(dataUrl.substring(commaIndex + 1), Base64.DEFAULT)
    BitmapFactory.decodeByteArray(bytes, 0, bytes.size)
}.getOrNull()

private suspend fun savePreviewToGallery(context: Context, preview: MediaPreview): String {
    return withContext(Dispatchers.IO) {
        runCatching {
            if (preview.type == "video") {
                val source = preview.localPath?.let { File(it) }?.takeIf { it.exists() }
                    ?: return@runCatching "视频尚未缓存"
                saveFileToMediaStore(
                    context = context,
                    source = source,
                    displayName = preview.fileName.ifBlank { "ichat_video.mp4" },
                    mimeType = "video/mp4",
                    collection = MediaStore.Video.Media.EXTERNAL_CONTENT_URI,
                    relativePath = "Movies/IChat"
                )
            } else {
                val imageBytes = preview.localPath
                    ?.let { File(it) }
                    ?.takeIf { it.exists() }
                    ?.readBytes()
                    ?: preview.dataUrl?.let { dataUrlBytes(it) }
                    ?: return@runCatching "图片不可用"
                saveBytesToMediaStore(
                    context = context,
                    bytes = imageBytes,
                    displayName = preview.fileName.ifBlank { "ichat_image.jpg" },
                    mimeType = "image/jpeg",
                    collection = MediaStore.Images.Media.EXTERNAL_CONTENT_URI,
                    relativePath = "Pictures/IChat"
                )
            }
            "已保存到系统相册"
        }.getOrElse {
            it.message?.takeIf { message -> message.isNotBlank() } ?: "保存失败"
        }
    }
}

private fun saveFileToMediaStore(
    context: Context,
    source: File,
    displayName: String,
    mimeType: String,
    collection: Uri,
    relativePath: String
) {
    val target = createMediaStoreItem(context, displayName, mimeType, collection, relativePath)
    context.contentResolver.openOutputStream(target)?.use { output ->
        FileInputStream(source).use { input -> input.copyTo(output) }
    } ?: error("无法写入系统相册")
    finishMediaStoreItem(context, target)
}

private fun saveBytesToMediaStore(
    context: Context,
    bytes: ByteArray,
    displayName: String,
    mimeType: String,
    collection: Uri,
    relativePath: String
) {
    val target = createMediaStoreItem(context, displayName, mimeType, collection, relativePath)
    context.contentResolver.openOutputStream(target)?.use { output ->
        output.write(bytes)
    } ?: error("无法写入系统相册")
    finishMediaStoreItem(context, target)
}

private fun createMediaStoreItem(
    context: Context,
    displayName: String,
    mimeType: String,
    collection: Uri,
    relativePath: String
): Uri {
    val values = ContentValues().apply {
        put(MediaStore.MediaColumns.DISPLAY_NAME, safeGalleryName(displayName, mimeType))
        put(MediaStore.MediaColumns.MIME_TYPE, mimeType)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            put(MediaStore.MediaColumns.RELATIVE_PATH, relativePath)
            put(MediaStore.MediaColumns.IS_PENDING, 1)
        }
    }
    return context.contentResolver.insert(collection, values) ?: error("无法创建系统相册条目")
}

private fun finishMediaStoreItem(context: Context, uri: Uri) {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
        val values = ContentValues().apply {
            put(MediaStore.MediaColumns.IS_PENDING, 0)
        }
        context.contentResolver.update(uri, values, null, null)
    }
}

private fun dataUrlBytes(value: String): ByteArray? {
    val commaIndex = value.indexOf(',')
    if (commaIndex <= 0) return null
    return Base64.decode(value.substring(commaIndex + 1), Base64.DEFAULT)
}

private fun safeGalleryName(name: String, mimeType: String): String {
    val clean = name.replace(Regex("[\\\\/:*?\"<>|]"), "_").ifBlank { "ichat_media" }
    val extension = when {
        clean.contains('.') -> ""
        mimeType == "video/mp4" -> ".mp4"
        else -> ".jpg"
    }
    return clean + extension
}

private fun isDownloaded(message: ChatMessageEntity): Boolean {
    return message.transferStatus == "downloaded" && message.localPath?.let { File(it).exists() } == true
}

private fun mediaStatusLabel(message: ChatMessageEntity): String {
    return when {
        message.transferStatus == "downloading" -> "下载中"
        message.transferStatus.startsWith("failed:") -> "下载失败"
        isDownloaded(message) -> "已下载"
        message.contentType == "file" || message.contentType == "video" -> "未下载"
        else -> ""
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
