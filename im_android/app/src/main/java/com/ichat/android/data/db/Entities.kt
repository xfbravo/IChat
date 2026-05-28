package com.ichat.android.data.db

import androidx.room.Entity
import androidx.room.Index
import androidx.room.PrimaryKey

/**
 * 本地消息表是 Android 端离线体验的核心：收到、发出、离线补偿和媒体消息都先落库。
 * content 对 text 是纯文本，对 image/file/voice/video 保留服务端约定的 JSON 字符串。
 */
@Entity(
    tableName = "chat_messages",
    indices = [
        Index(value = ["conversationKey", "serverTimestamp"]),
        Index(value = ["peerId", "chatType"])
    ]
)
data class ChatMessageEntity(
    @PrimaryKey val msgId: String,
    val conversationKey: String,
    val peerId: String,
    val chatType: String,
    val fromUserId: String,
    val toUserId: String,
    val contentType: String,
    val content: String,
    val localPath: String?,
    val transferId: String?,
    val transferStatus: String,
    val sendStatus: String,
    val clientTime: Long,
    val serverTimestamp: Long,
    val serverTime: String,
    val isMine: Boolean
)

@Entity(tableName = "conversations")
data class ConversationEntity(
    @PrimaryKey val conversationKey: String,
    val peerId: String,
    val chatType: String,
    val title: String,
    val avatarUrl: String,
    val lastMessage: String,
    val lastTimestamp: Long,
    val unreadCount: Int
)

@Entity(tableName = "friends")
data class FriendEntity(
    @PrimaryKey val userId: String,
    val nickname: String,
    val remark: String,
    val avatarUrl: String,
    val gender: String,
    val region: String,
    val signature: String
)

@Entity(tableName = "groups")
data class GroupEntity(
    @PrimaryKey val groupId: String,
    val groupName: String,
    val groupAvatar: String,
    val ownerId: String,
    val memberCount: Int
)
