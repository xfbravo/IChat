package com.ichat.android.ui.moments

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.net.Uri
import android.os.Bundle
import android.util.Base64
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.fragment.app.viewModels
import com.ichat.android.IChatApplication
import com.ichat.android.R
import com.ichat.android.data.model.MomentImage
import com.ichat.android.data.model.MomentPost
import com.ichat.android.ui.common.AvatarProfile
import com.ichat.android.ui.common.BackIconButton
import com.ichat.android.ui.common.IChatAvatar
import com.ichat.android.ui.main.MainViewModel
import com.ichat.android.ui.main.ViewModelFactory
import com.ichat.android.ui.me.MyProfileFragment
import com.ichat.android.ui.profile.ContactProfileFragment
import com.ichat.android.ui.theme.IChatTheme

class MomentsFragment : Fragment() {
    private val factory: ViewModelFactory by lazy {
        ViewModelFactory((requireActivity().application as IChatApplication).container.repository)
    }
    private val viewModel: MomentsViewModel by viewModels { factory }
    private val mainViewModel: MainViewModel by activityViewModels { factory }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        val targetUserId = arguments?.getString(ArgTargetUserId).orEmpty()
        val targetTitle = arguments?.getString(ArgTargetTitle).orEmpty()
        return ComposeView(requireContext()).apply {
            setContent {
                IChatTheme {
                    MomentsScreen(
                        viewModel = viewModel,
                        mainViewModel = mainViewModel,
                        targetUserId = targetUserId,
                        targetTitle = targetTitle,
                        onBack = { parentFragmentManager.popBackStack() },
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
            .addToBackStack("moment_profile")
            .commit()
    }

    companion object {
        fun newInstance(targetUserId: String, targetTitle: String): MomentsFragment {
            return MomentsFragment().apply {
                arguments = Bundle().apply {
                    putString(ArgTargetUserId, targetUserId)
                    putString(ArgTargetTitle, targetTitle)
                }
            }
        }

        private const val ArgTargetUserId = "target_user_id"
        private const val ArgTargetTitle = "target_title"
    }
}

@Composable
private fun MomentsScreen(
    viewModel: MomentsViewModel,
    mainViewModel: MainViewModel,
    targetUserId: String,
    targetTitle: String,
    onBack: () -> Unit,
    onOpenProfile: (AvatarProfile, Boolean) -> Unit
) {
    val moments by viewModel.moments.collectAsState()
    val refreshing by viewModel.refreshing.collectAsState()
    val publishing by viewModel.publishing.collectAsState()
    val status by viewModel.status.collectAsState()
    val currentUser by mainViewModel.currentUser.collectAsState()
    var showComposer by remember { mutableStateOf(false) }
    var previewImage by remember { mutableStateOf<MomentImage?>(null) }
    val isContactTimeline = targetUserId.isNotBlank()

    // 同一个朋友圈页既服务底部 Tab，也服务联系人资料页；targetUserId 决定请求全量或个人时间流。
    LaunchedEffect(targetUserId) {
        viewModel.refresh(targetUserId)
    }
    LaunchedEffect(isContactTimeline) {
        if (isContactTimeline) {
            mainViewModel.setBottomNavigationVisible(false)
        }
    }
    DisposableEffect(isContactTimeline) {
        onDispose {
            if (isContactTimeline) {
                mainViewModel.setBottomNavigationVisible(true)
            }
        }
    }
    BackHandler(enabled = isContactTimeline, onBack = onBack)

    val title = if (isContactTimeline) {
        "${targetTitle.ifBlank { targetUserId }}的朋友圈"
    } else {
        "朋友圈"
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(Color(0xFFEEF2EF))
    ) {
        MomentsTopBar(
            title = title,
            status = status,
            refreshing = refreshing,
            publishing = publishing,
            showBack = isContactTimeline,
            showCreate = !isContactTimeline,
            onBack = onBack,
            onRefresh = { viewModel.refresh(targetUserId) },
            onCreate = { showComposer = true }
        )
        LazyColumn(
            modifier = Modifier.fillMaxSize(),
            contentPadding = androidx.compose.foundation.layout.PaddingValues(horizontal = 16.dp, vertical = 14.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            if (moments.isEmpty()) {
                item {
                    EmptyMoments(refreshing = refreshing)
                }
            }
            items(moments, key = { it.momentId.ifBlank { "${it.userId}-${it.createTimestamp}-${it.content.hashCode()}" } }) { moment ->
                MomentCard(
                    moment = moment,
                    onAvatarClick = {
                        onOpenProfile(moment.profile(), moment.userId == currentUser?.userId)
                    },
                    onImageClick = { previewImage = it }
                )
            }
        }
    }

    if (showComposer) {
        MomentComposerDialog(
            publishing = publishing,
            onDismiss = { showComposer = false },
            onPublish = { content, images ->
                viewModel.createMoment(content, images)
                showComposer = false
            }
        )
    }
    MomentImagePreview(image = previewImage, onDismiss = { previewImage = null })
}

@Composable
private fun MomentsTopBar(
    title: String,
    status: String,
    refreshing: Boolean,
    publishing: Boolean,
    showBack: Boolean,
    showCreate: Boolean,
    onBack: () -> Unit,
    onRefresh: () -> Unit,
    onCreate: () -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .background(Color(0xFFEEF2EF))
            .padding(top = 30.dp, bottom = 8.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        if (showBack) {
            BackIconButton(onClick = onBack)
        } else {
            IconButton(onClick = onCreate, enabled = showCreate && !publishing, modifier = Modifier.width(56.dp)) {
                Text("+", color = Color(0xFF2F6F3E), fontSize = 28.sp, fontWeight = FontWeight.Bold)
            }
        }
        Column(
            modifier = Modifier.weight(1f),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Text(title, fontWeight = FontWeight.Bold, fontSize = 20.sp)
            if (status.isNotBlank()) {
                Text(status, color = Color(0xFF6B756E), fontSize = 12.sp)
            }
        }
        IconButton(onClick = onRefresh, enabled = !refreshing, modifier = Modifier.width(56.dp)) {
            if (refreshing) {
                CircularProgressIndicator(modifier = Modifier.size(20.dp), strokeWidth = 2.dp)
            } else {
                Icon(
                    painter = painterResource(R.drawable.ic_refresh),
                    contentDescription = "刷新",
                    tint = Color(0xFF2F6F3E)
                )
            }
        }
    }
    HorizontalDivider(color = Color(0xFFDDE6DF))
}

@Composable
private fun EmptyMoments(refreshing: Boolean) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 72.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        if (refreshing) {
            CircularProgressIndicator()
            Spacer(Modifier.height(14.dp))
            Text("正在加载朋友圈...")
        } else {
            Text("暂无朋友圈动态", color = Color(0xFF6B756E))
        }
    }
}

@Composable
private fun MomentCard(
    moment: MomentPost,
    onAvatarClick: () -> Unit,
    onImageClick: (MomentImage) -> Unit
) {
    val displayName = moment.nickname.ifBlank { moment.userId }.ifBlank { "IChat" }
    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(8.dp),
        colors = CardDefaults.cardColors(containerColor = Color.White),
        elevation = CardDefaults.cardElevation(defaultElevation = 1.dp)
    ) {
        Row(
            modifier = Modifier.padding(14.dp),
            verticalAlignment = Alignment.Top
        ) {
            IChatAvatar(
                avatarUrl = moment.avatarUrl,
                displayName = displayName,
                size = 44.dp,
                onClick = onAvatarClick
            )
            Spacer(Modifier.width(12.dp))
            Column(modifier = Modifier.weight(1f)) {
                Text(displayName, color = Color(0xFF22342B), fontWeight = FontWeight.Bold)
                if (moment.content.isNotBlank()) {
                    Spacer(Modifier.height(6.dp))
                    Text(moment.content, color = Color(0xFF17211C), lineHeight = 21.sp)
                }
                if (moment.images.isNotEmpty()) {
                    Spacer(Modifier.height(10.dp))
                    MomentImageGrid(images = moment.images, onImageClick = onImageClick)
                }
                Spacer(Modifier.height(8.dp))
                Text(moment.createTime, color = Color(0xFF6B756E), fontSize = 12.sp)
            }
        }
    }
}

private fun MomentPost.profile(): AvatarProfile {
    return AvatarProfile(
        title = nickname.ifBlank { userId }.ifBlank { "IChat" },
        subtitle = "账号：$userId",
        avatarUrl = avatarUrl,
        userId = userId,
        chatType = "p2p",
        nickname = nickname
    )
}

@Composable
private fun MomentImageGrid(images: List<MomentImage>, onImageClick: (MomentImage) -> Unit) {
    BoxWithConstraints(modifier = Modifier.fillMaxWidth()) {
        val columns = images.size.coerceIn(1, 3)
        val spacing = 6.dp
        val totalSpacing = spacing * (columns - 1).toFloat()
        val tileSize = minOf(112.dp, (maxWidth - totalSpacing) / columns.toFloat())

        Column(verticalArrangement = Arrangement.spacedBy(spacing)) {
            images.chunked(3).forEach { rowImages ->
                Row(horizontalArrangement = Arrangement.spacedBy(spacing)) {
                    rowImages.forEach { image ->
                        MomentImageTile(
                            image = image,
                            modifier = Modifier.size(tileSize),
                            onClick = { onImageClick(image) }
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun MomentImageTile(image: MomentImage, modifier: Modifier = Modifier, onClick: () -> Unit) {
    val bitmap = remember(image.thumbUrl) { decodeDataImageBitmap(image.thumbUrl) }
    Box(
        modifier = modifier
            .clip(RoundedCornerShape(6.dp))
            .background(Color(0xFFE4ECE6))
            .clickable(onClick = onClick),
        contentAlignment = Alignment.Center
    ) {
        if (bitmap != null) {
            Image(
                bitmap = bitmap,
                contentDescription = "朋友圈图片",
                modifier = Modifier.fillMaxSize(),
                contentScale = ContentScale.Crop
            )
        } else {
            Text("图片", color = Color(0xFF6B756E), fontSize = 12.sp)
        }
    }
}

@Composable
private fun MomentComposerDialog(
    publishing: Boolean,
    onDismiss: () -> Unit,
    onPublish: (String, List<Uri>) -> Unit
) {
    val context = LocalContext.current
    var content by remember { mutableStateOf("") }
    var images by remember { mutableStateOf<List<Uri>>(emptyList()) }
    var warning by remember { mutableStateOf("") }
    val imagePicker = rememberLauncherForActivityResult(ActivityResultContracts.GetMultipleContents()) { picked ->
        val merged = (images + picked).distinct().take(MaxMomentImages)
        warning = if (images.size + picked.size > MaxMomentImages) "朋友圈图片最多九张，已保留前九张" else ""
        images = merged
    }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("发布朋友圈") },
        text = {
            Column {
                OutlinedTextField(
                    value = content,
                    onValueChange = { content = it },
                    placeholder = { Text("这一刻的想法...") },
                    minLines = 3,
                    maxLines = 6,
                    modifier = Modifier.fillMaxWidth()
                )
                Spacer(Modifier.height(10.dp))
                Row(verticalAlignment = Alignment.CenterVertically) {
                    TextButton(onClick = { imagePicker.launch("image/*") }, enabled = !publishing) {
                        Text("选择图片")
                    }
                    Text("${images.size}/$MaxMomentImages", color = Color(0xFF6B756E))
                    Spacer(Modifier.weight(1f))
                    if (images.isNotEmpty()) {
                        TextButton(onClick = { images = emptyList() }, enabled = !publishing) {
                            Text("清空")
                        }
                    }
                }
                if (warning.isNotBlank()) {
                    Text(warning, color = MaterialTheme.colorScheme.error, fontSize = 12.sp)
                }
                if (images.isNotEmpty()) {
                    Spacer(Modifier.height(8.dp))
                    LazyVerticalGrid(
                        columns = GridCells.Fixed(3),
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(166.dp),
                        horizontalArrangement = Arrangement.spacedBy(6.dp),
                        verticalArrangement = Arrangement.spacedBy(6.dp)
                    ) {
                        items(images, key = { it.toString() }) { uri ->
                            ComposerImageTile(context = context, uri = uri)
                        }
                    }
                }
            }
        },
        confirmButton = {
            Button(
                enabled = !publishing && (content.trim().isNotEmpty() || images.isNotEmpty()),
                onClick = { onPublish(content, images) }
            ) {
                Text(if (publishing) "发布中" else "发布")
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss, enabled = !publishing) {
                Text("取消")
            }
        }
    )
}

@Composable
private fun ComposerImageTile(context: Context, uri: Uri) {
    val bitmap = remember(uri) { decodeUriPreview(context, uri) }
    Box(
        modifier = Modifier
            .aspectRatio(1f)
            .clip(RoundedCornerShape(6.dp))
            .background(Color(0xFFE4ECE6)),
        contentAlignment = Alignment.Center
    ) {
        if (bitmap != null) {
            Image(
                bitmap = bitmap,
                contentDescription = "已选择图片",
                modifier = Modifier.fillMaxSize(),
                contentScale = ContentScale.Crop
            )
        } else {
            Text("图片", color = Color(0xFF6B756E), fontSize = 12.sp)
        }
    }
}

@Composable
private fun MomentImagePreview(image: MomentImage?, onDismiss: () -> Unit) {
    if (image == null) return
    val bitmap = remember(image.imageUrl) { decodeDataImageBitmap(image.imageUrl) }

    AlertDialog(
        onDismissRequest = onDismiss,
        text = {
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .background(Color.Black, RoundedCornerShape(8.dp))
                    .padding(8.dp),
                contentAlignment = Alignment.Center
            ) {
                if (bitmap != null) {
                    Image(
                        bitmap = bitmap,
                        contentDescription = "朋友圈大图",
                        modifier = Modifier.fillMaxWidth(),
                        contentScale = ContentScale.Fit
                    )
                } else {
                    Text("图片无法打开", color = Color.White, modifier = Modifier.padding(28.dp))
                }
            }
        },
        confirmButton = {
            TextButton(onClick = onDismiss) {
                Text("关闭")
            }
        }
    )
}

private fun decodeDataImageBitmap(value: String): ImageBitmap? {
    val dataUrl = value.trim()
    if (!dataUrl.startsWith("data:image/", ignoreCase = true)) return null
    return runCatching {
        // 朋友圈沿用服务端 data URL 字段，渲染前只在本地解码，不再走网络加载器。
        val commaIndex = dataUrl.indexOf(',')
        if (commaIndex <= 0) return@runCatching null
        val bytes = Base64.decode(dataUrl.substring(commaIndex + 1), Base64.DEFAULT)
        BitmapFactory.decodeByteArray(bytes, 0, bytes.size)?.asImageBitmap()
    }.getOrNull()
}

private fun decodeUriPreview(context: Context, uri: Uri): ImageBitmap? {
    return runCatching {
        context.contentResolver.openInputStream(uri)?.use { input ->
            BitmapFactory.decodeStream(input)
        }?.scaledToMaxEdge(320)?.asImageBitmap()
    }.getOrNull()
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

private const val MaxMomentImages = 9
