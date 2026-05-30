package com.ichat.android.data.db

import androidx.room.Entity
import androidx.room.Index

/**
 * Local message cache. Every cached row is scoped by ownerUserId so a newly
 * signed-in account can never read another account's conversations from Room.
 */
@Entity(
    tableName = "chat_messages",
    primaryKeys = ["ownerUserId", "msgId"],
    indices = [
        Index(value = ["ownerUserId", "conversationKey", "serverTimestamp"]),
        Index(value = ["ownerUserId", "peerId", "chatType"])
    ]
)
data class ChatMessageEntity(
    val ownerUserId: String,
    val msgId: String,
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

@Entity(
    tableName = "conversations",
    primaryKeys = ["ownerUserId", "conversationKey"],
    indices = [Index(value = ["ownerUserId", "lastTimestamp"])]
)
data class ConversationEntity(
    val ownerUserId: String,
    val conversationKey: String,
    val peerId: String,
    val chatType: String,
    val title: String,
    val avatarUrl: String,
    val lastMessage: String,
    val lastTimestamp: Long,
    val unreadCount: Int
)

@Entity(
    tableName = "friends",
    primaryKeys = ["ownerUserId", "userId"],
    indices = [Index(value = ["ownerUserId", "userId"])]
)
data class FriendEntity(
    val ownerUserId: String,
    val userId: String,
    val nickname: String,
    val remark: String,
    val avatarUrl: String,
    val gender: String,
    val region: String,
    val signature: String
)

@Entity(
    tableName = "groups",
    primaryKeys = ["ownerUserId", "groupId"],
    indices = [Index(value = ["ownerUserId", "groupId"])]
)
data class GroupEntity(
    val ownerUserId: String,
    val groupId: String,
    val groupName: String,
    val groupAvatar: String,
    val ownerId: String,
    val memberCount: Int
)
