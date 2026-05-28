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

data class LoginResult(
    val code: Int,
    val message: String,
    val user: CurrentUser? = null
)
