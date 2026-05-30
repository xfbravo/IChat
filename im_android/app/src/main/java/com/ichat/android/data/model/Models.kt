package com.ichat.android.data.model

data class CurrentUser(
    val userId: String,
    val nickname: String,
    val avatarUrl: String,
    val token: String,
    val gender: String = "",
    val region: String = "",
    val signature: String = ""
)

data class UserProfile(
    val userId: String,
    val nickname: String,
    val avatarUrl: String,
    val gender: String = "",
    val region: String = "",
    val signature: String = ""
)

data class ChatTarget(
    val peerId: String,
    val chatType: String,
    val title: String
) {
    val conversationKey: String = "$chatType:$peerId"
}

data class MessageDraft(
    val peerId: String,
    val chatType: String,
    val content: String
)

data class FriendRequest(
    val requestId: String,
    val fromUserId: String,
    val fromNickname: String,
    val fromAvatar: String,
    val remark: String,
    val createTime: String
)

data class MomentImage(
    val thumbUrl: String,
    val imageUrl: String
)

data class MomentPost(
    val momentId: String,
    val userId: String,
    val nickname: String,
    val avatarUrl: String,
    val content: String,
    val mediaType: String,
    val images: List<MomentImage>,
    val createTime: String,
    val createTimestamp: Long
)

data class LoginResult(
    val code: Int,
    val message: String,
    val user: CurrentUser? = null
)
