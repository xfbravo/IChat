package com.ichat.android.data.db

import android.content.Context
import androidx.room.Database
import androidx.room.Room
import androidx.room.RoomDatabase

@Database(
    entities = [
        ChatMessageEntity::class,
        ConversationEntity::class,
        FriendEntity::class,
        GroupEntity::class
    ],
    version = 1,
    exportSchema = false
)
abstract class IChatDatabase : RoomDatabase() {
    abstract fun messageDao(): MessageDao
    abstract fun conversationDao(): ConversationDao
    abstract fun contactDao(): ContactDao

    companion object {
        fun create(context: Context): IChatDatabase {
            return Room.databaseBuilder(
                context.applicationContext,
                IChatDatabase::class.java,
                "ichat.db"
            ).build()
        }
    }
}
