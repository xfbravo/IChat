package com.ichat.android.ui.profile

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentManager
import androidx.fragment.app.activityViewModels
import androidx.fragment.app.viewModels
import com.ichat.android.IChatApplication
import com.ichat.android.data.db.FriendEntity
import com.ichat.android.data.model.CurrentUser
import com.ichat.android.data.model.UserProfile
import com.ichat.android.ui.common.AvatarProfile
import com.ichat.android.ui.common.BackIconButton
import com.ichat.android.ui.common.ContactActionDialogs
import com.ichat.android.ui.common.IChatAvatar
import com.ichat.android.ui.main.MainViewModel
import com.ichat.android.ui.main.ViewModelFactory
import com.ichat.android.ui.moments.MomentsFragment
import com.ichat.android.ui.theme.IChatTheme

class ContactProfileFragment : Fragment() {
    private val factory: ViewModelFactory by lazy {
        ViewModelFactory((requireActivity().application as IChatApplication).container.repository)
    }
    private val mainViewModel: MainViewModel by activityViewModels { factory }
    private val profileViewModel: ProfileViewModel by viewModels { factory }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        val profile = requireArguments().toAvatarProfile()
        return ComposeView(requireContext()).apply {
            setContent {
                IChatTheme {
                    ContactProfileScreen(
                        profile = profile,
                        mainViewModel = mainViewModel,
                        profileViewModel = profileViewModel,
                        onBack = { parentFragmentManager.popBackStack() },
                        onOpenMoments = ::openMoments,
                        onOpenChat = ::openChat
                    )
                }
            }
        }
    }

    private fun openMoments(profile: AvatarProfile) {
        val hostId = (view?.parent as? ViewGroup)?.id ?: return
        parentFragmentManager.beginTransaction()
            .replace(hostId, MomentsFragment.newInstance(profile.userId, profile.title))
            .addToBackStack("contact_moments")
            .commit()
    }

    private fun openChat(profile: AvatarProfile) {
        // 发消息是跨 Tab 导航，先清掉资料页栈，避免系统返回键又回到旧资料页。
        parentFragmentManager.popBackStackImmediate(null, FragmentManager.POP_BACK_STACK_INCLUSIVE)
        mainViewModel.openChat(
            peerId = profile.userId,
            chatType = profile.chatType.ifBlank { "p2p" },
            title = profile.title.ifBlank { profile.userId }
        )
    }

    companion object {
        fun newInstance(profile: AvatarProfile): ContactProfileFragment {
            return ContactProfileFragment().apply {
                arguments = Bundle().apply {
                    putString(ArgTitle, profile.title)
                    putString(ArgSubtitle, profile.subtitle)
                    putString(ArgAvatarUrl, profile.avatarUrl)
                    putString(ArgDetail, profile.detail)
                    putString(ArgUserId, profile.userId)
                    putString(ArgChatType, profile.chatType)
                    putString(ArgGender, profile.gender)
                    putString(ArgRegion, profile.region)
                    putString(ArgSignature, profile.signature)
                    putString(ArgNickname, profile.nickname)
                    putString(ArgRemark, profile.remark)
                }
            }
        }

        private const val ArgTitle = "title"
        private const val ArgSubtitle = "subtitle"
        private const val ArgAvatarUrl = "avatar_url"
        private const val ArgDetail = "detail"
        private const val ArgUserId = "user_id"
        private const val ArgChatType = "chat_type"
        private const val ArgGender = "gender"
        private const val ArgRegion = "region"
        private const val ArgSignature = "signature"
        private const val ArgNickname = "nickname"
        private const val ArgRemark = "remark"
    }
}

@Composable
private fun ContactProfileScreen(
    profile: AvatarProfile,
    mainViewModel: MainViewModel,
    profileViewModel: ProfileViewModel,
    onBack: () -> Unit,
    onOpenMoments: (AvatarProfile) -> Unit,
    onOpenChat: (AvatarProfile) -> Unit
) {
    val isGroup = profile.chatType == "group"
    val currentUser by profileViewModel.currentUser.collectAsState()
    val friends by profileViewModel.friends.collectAsState()
    val userProfiles by profileViewModel.userProfiles.collectAsState()
    val friend = friends.firstOrNull { it.userId == profile.userId }
    val remoteProfile = userProfiles[profile.userId]
    val displayProfile = profile.mergeWith(friend, remoteProfile, currentUser)
    val isCurrentUser = currentUser?.userId == profile.userId
    val isFriend = friend != null || isCurrentUser
    val canSendMessage = isGroup || isFriend
    val showAddFriend = androidx.compose.runtime.remember { mutableStateOf(false) }

    LaunchedEffect(Unit) {
        mainViewModel.setBottomNavigationVisible(false)
    }
    LaunchedEffect(profile.userId, isGroup) {
        if (!isGroup && profile.userId.isNotBlank()) {
            profileViewModel.requestUserProfile(profile.userId)
        }
    }
    DisposableEffect(Unit) {
        onDispose { mainViewModel.setBottomNavigationVisible(true) }
    }
    BackHandler(onBack = onBack)

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(Color(0xFFF5F7F5))
    ) {
        ProfileTopBar(title = if (isGroup) "群聊资料" else "联系人资料", onBack = onBack)
        ContactInfoCard(profile = displayProfile, isGroup = isGroup, isFriend = isFriend)
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 20.dp, vertical = 18.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            if (!isGroup && profile.userId.isNotBlank()) {
                Button(onClick = { onOpenMoments(displayProfile) }, modifier = Modifier.fillMaxWidth()) {
                    Text("朋友圈")
                }
            }
            Button(
                onClick = {
                    if (canSendMessage) {
                        onOpenChat(displayProfile)
                    } else {
                        showAddFriend.value = true
                    }
                },
                enabled = profile.userId.isNotBlank(),
                modifier = Modifier.fillMaxWidth()
            ) {
                Text(if (canSendMessage) "发消息" else "添加好友")
            }
        }
    }

    ContactActionDialogs(
        showAddFriend = showAddFriend.value,
        showCreateGroup = false,
        friends = friends,
        currentUser = currentUser,
        onDismissAddFriend = { showAddFriend.value = false },
        onDismissCreateGroup = {},
        onSendFriendRequest = profileViewModel::sendFriendRequest,
        onCreateGroup = { _, _ -> }
    )
}

@Composable
private fun ProfileTopBar(title: String, onBack: () -> Unit) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(top = 32.dp, bottom = 8.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        BackIconButton(onClick = onBack)
        Text(
            text = title,
            fontWeight = FontWeight.Bold,
            fontSize = 20.sp,
            modifier = Modifier.weight(1f)
        )
        Spacer(Modifier.width(72.dp))
    }
    HorizontalDivider(color = Color(0xFFDDE6DF))
}

@Composable
private fun ContactInfoCard(profile: AvatarProfile, isGroup: Boolean, isFriend: Boolean) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 20.dp, vertical = 18.dp),
        shape = RoundedCornerShape(8.dp),
        colors = CardDefaults.cardColors(containerColor = Color.White),
        elevation = CardDefaults.cardElevation(defaultElevation = 1.dp)
    ) {
        Row(
            modifier = Modifier.padding(18.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            IChatAvatar(
                avatarUrl = profile.avatarUrl,
                displayName = profile.title.ifBlank { profile.userId },
                size = 72.dp
            )
            Spacer(Modifier.width(16.dp))
            Column(modifier = Modifier.weight(1f)) {
                Text(profile.title.ifBlank { profile.userId }, fontWeight = FontWeight.SemiBold, fontSize = 19.sp)
                Spacer(Modifier.height(5.dp))
                if (isGroup) {
                    InfoLine("群号", profile.userId)
                    val note = profile.detail.ifBlank { "未设置" }
                    InfoLine("群信息", note)
                } else {
                    InfoLine("备注", if (isFriend) profile.remark.ifBlank { "未设置" } else "未添加好友")
                    InfoLine("用户昵称", profile.nickname.ifBlank { "未设置" })
                    InfoLine("账号", profile.userId)
                    InfoLine("性别", profile.gender.ifBlank { "未设置" })
                    InfoLine("地区", profile.region.ifBlank { "未设置" })
                    InfoLine("签名", profile.signature.ifBlank { "未设置" })
                }
            }
        }
    }
}

@Composable
private fun InfoLine(label: String, value: String) {
    Spacer(Modifier.height(3.dp))
    Text("$label：$value", color = Color(0xFF5E6A61))
}

private fun AvatarProfile.mergeWith(
    friend: FriendEntity?,
    remote: UserProfile?,
    currentUser: CurrentUser?
): AvatarProfile {
    if (chatType == "group") return this

    val isCurrentUser = currentUser?.userId == userId
    val resolvedRemark = friend?.remark.orEmpty().ifBlank { remark }
    val resolvedNickname = when {
        isCurrentUser -> currentUser?.nickname.orEmpty()
        !remote?.nickname.isNullOrBlank() -> remote?.nickname.orEmpty()
        !friend?.nickname.isNullOrBlank() -> friend?.nickname.orEmpty()
        else -> nickname
    }
    val resolvedAvatar = when {
        isCurrentUser && !currentUser?.avatarUrl.isNullOrBlank() -> currentUser?.avatarUrl.orEmpty()
        !remote?.avatarUrl.isNullOrBlank() -> remote?.avatarUrl.orEmpty()
        !friend?.avatarUrl.isNullOrBlank() -> friend?.avatarUrl.orEmpty()
        else -> avatarUrl
    }
    val resolvedGender = when {
        isCurrentUser -> currentUser?.gender.orEmpty()
        !remote?.gender.isNullOrBlank() -> remote?.gender.orEmpty()
        !friend?.gender.isNullOrBlank() -> friend?.gender.orEmpty()
        else -> gender
    }
    val resolvedRegion = when {
        isCurrentUser -> currentUser?.region.orEmpty()
        !remote?.region.isNullOrBlank() -> remote?.region.orEmpty()
        !friend?.region.isNullOrBlank() -> friend?.region.orEmpty()
        else -> region
    }
    val resolvedSignature = when {
        isCurrentUser -> currentUser?.signature.orEmpty()
        !remote?.signature.isNullOrBlank() -> remote?.signature.orEmpty()
        !friend?.signature.isNullOrBlank() -> friend?.signature.orEmpty()
        else -> signature.ifBlank { detail }
    }
    val displayName = resolvedRemark.ifBlank { resolvedNickname }.ifBlank { title }.ifBlank { userId }

    return copy(
        title = displayName,
        avatarUrl = resolvedAvatar,
        detail = resolvedSignature,
        gender = resolvedGender,
        region = resolvedRegion,
        signature = resolvedSignature,
        nickname = resolvedNickname,
        remark = resolvedRemark
    )
}

private fun Bundle.toAvatarProfile(): AvatarProfile {
    return AvatarProfile(
        title = getString("title").orEmpty(),
        subtitle = getString("subtitle").orEmpty(),
        avatarUrl = getString("avatar_url").orEmpty(),
        detail = getString("detail").orEmpty(),
        userId = getString("user_id").orEmpty(),
        chatType = getString("chat_type").orEmpty().ifBlank { "p2p" },
        gender = getString("gender").orEmpty(),
        region = getString("region").orEmpty(),
        signature = getString("signature").orEmpty(),
        nickname = getString("nickname").orEmpty(),
        remark = getString("remark").orEmpty()
    )
}
