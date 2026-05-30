package com.ichat.android.data.db

import androidx.room.Dao
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.Query
import kotlinx.coroutines.flow.Flow

@Dao
interface MessageDao {
    @Query(
        """
        SELECT * FROM (
            SELECT * FROM chat_messages
            WHERE ownerUserId = :ownerUserId AND conversationKey = :conversationKey
            ORDER BY serverTimestamp DESC, clientTime DESC
            LIMIT :limit
        )
        ORDER BY serverTimestamp ASC, clientTime ASC
        """
    )
    fun observeRecentMessages(ownerUserId: String, conversationKey: String, limit: Int): Flow<List<ChatMessageEntity>>

    @Query(
        """
        SELECT * FROM chat_messages
        WHERE ownerUserId = :ownerUserId AND conversationKey = :conversationKey
        ORDER BY serverTimestamp DESC, clientTime DESC
        LIMIT :limit
        """
    )
    suspend fun latestMessages(ownerUserId: String, conversationKey: String, limit: Int): List<ChatMessageEntity>

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun upsert(message: ChatMessageEntity)

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun upsertAll(messages: List<ChatMessageEntity>)

    @Query("UPDATE chat_messages SET sendStatus = :status WHERE ownerUserId = :ownerUserId AND msgId = :msgId")
    suspend fun updateSendStatus(ownerUserId: String, msgId: String, status: String)

    @Query("UPDATE chat_messages SET localPath = :path, transferStatus = :status WHERE ownerUserId = :ownerUserId AND msgId = :msgId")
    suspend fun updateLocalFile(ownerUserId: String, msgId: String, path: String?, status: String)

    @Query("DELETE FROM chat_messages WHERE ownerUserId = :ownerUserId")
    suspend fun clearForOwner(ownerUserId: String)

    @Query("DELETE FROM chat_messages")
    suspend fun clearAll()
}

@Dao
interface ConversationDao {
    @Query("SELECT * FROM conversations WHERE ownerUserId = :ownerUserId ORDER BY lastTimestamp DESC")
    fun observeConversations(ownerUserId: String): Flow<List<ConversationEntity>>

    @Query("SELECT * FROM conversations WHERE ownerUserId = :ownerUserId AND conversationKey = :conversationKey LIMIT 1")
    suspend fun find(ownerUserId: String, conversationKey: String): ConversationEntity?

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun upsert(conversation: ConversationEntity)

    @Query("UPDATE conversations SET unreadCount = 0 WHERE ownerUserId = :ownerUserId AND conversationKey = :conversationKey")
    suspend fun clearUnread(ownerUserId: String, conversationKey: String)

    @Query("DELETE FROM conversations WHERE ownerUserId = :ownerUserId")
    suspend fun clearForOwner(ownerUserId: String)

    @Query("DELETE FROM conversations")
    suspend fun clearAll()
}

@Dao
interface ContactDao {
    @Query(
        """
        SELECT * FROM friends
        WHERE ownerUserId = :ownerUserId
        ORDER BY COALESCE(NULLIF(remark, ''), nickname) COLLATE NOCASE
        """
    )
    fun observeFriends(ownerUserId: String): Flow<List<FriendEntity>>

    @Query("SELECT * FROM groups WHERE ownerUserId = :ownerUserId ORDER BY groupName COLLATE NOCASE")
    fun observeGroups(ownerUserId: String): Flow<List<GroupEntity>>

    @Query("SELECT * FROM friends WHERE ownerUserId = :ownerUserId AND userId = :userId LIMIT 1")
    suspend fun findFriend(ownerUserId: String, userId: String): FriendEntity?

    @Query("SELECT * FROM groups WHERE ownerUserId = :ownerUserId AND groupId = :groupId LIMIT 1")
    suspend fun findGroup(ownerUserId: String, groupId: String): GroupEntity?

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun upsertFriends(friends: List<FriendEntity>)

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun upsertGroups(groups: List<GroupEntity>)

    @Query("DELETE FROM friends WHERE ownerUserId = :ownerUserId")
    suspend fun clearFriends(ownerUserId: String)

    @Query("DELETE FROM groups WHERE ownerUserId = :ownerUserId")
    suspend fun clearGroups(ownerUserId: String)
}
