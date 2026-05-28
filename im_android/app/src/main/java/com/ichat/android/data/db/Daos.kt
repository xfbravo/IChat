package com.ichat.android.data.db

import androidx.room.Dao
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.Query
import kotlinx.coroutines.flow.Flow

@Dao
interface MessageDao {
    @Query("SELECT * FROM chat_messages WHERE conversationKey = :conversationKey ORDER BY serverTimestamp ASC, clientTime ASC")
    fun observeMessages(conversationKey: String): Flow<List<ChatMessageEntity>>

    @Query("SELECT * FROM chat_messages WHERE conversationKey = :conversationKey ORDER BY serverTimestamp DESC, clientTime DESC LIMIT :limit")
    suspend fun latestMessages(conversationKey: String, limit: Int): List<ChatMessageEntity>

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun upsert(message: ChatMessageEntity)

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun upsertAll(messages: List<ChatMessageEntity>)

    @Query("UPDATE chat_messages SET sendStatus = :status WHERE msgId = :msgId")
    suspend fun updateSendStatus(msgId: String, status: String)

    @Query("UPDATE chat_messages SET localPath = :path, transferStatus = :status WHERE msgId = :msgId")
    suspend fun updateLocalFile(msgId: String, path: String?, status: String)
}

@Dao
interface ConversationDao {
    @Query("SELECT * FROM conversations ORDER BY lastTimestamp DESC")
    fun observeConversations(): Flow<List<ConversationEntity>>

    @Query("SELECT * FROM conversations WHERE conversationKey = :conversationKey LIMIT 1")
    suspend fun find(conversationKey: String): ConversationEntity?

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun upsert(conversation: ConversationEntity)

    @Query("UPDATE conversations SET unreadCount = 0 WHERE conversationKey = :conversationKey")
    suspend fun clearUnread(conversationKey: String)
}

@Dao
interface ContactDao {
    @Query("SELECT * FROM friends ORDER BY COALESCE(NULLIF(remark, ''), nickname) COLLATE NOCASE")
    fun observeFriends(): Flow<List<FriendEntity>>

    @Query("SELECT * FROM groups ORDER BY groupName COLLATE NOCASE")
    fun observeGroups(): Flow<List<GroupEntity>>

    @Query("SELECT * FROM friends WHERE userId = :userId LIMIT 1")
    suspend fun findFriend(userId: String): FriendEntity?

    @Query("SELECT * FROM groups WHERE groupId = :groupId LIMIT 1")
    suspend fun findGroup(groupId: String): GroupEntity?

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun upsertFriends(friends: List<FriendEntity>)

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun upsertGroups(groups: List<GroupEntity>)
}
