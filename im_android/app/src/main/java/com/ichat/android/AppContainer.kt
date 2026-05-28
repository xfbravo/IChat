package com.ichat.android

import android.content.Context
import com.ichat.android.data.db.IChatDatabase
import com.ichat.android.data.network.IChatSocketClient
import com.ichat.android.data.repository.IChatRepository
import com.ichat.android.data.storage.AttachmentStore
import com.ichat.android.data.storage.PreferencesStore
import com.ichat.android.notification.ChatNotificationManager
import kotlinx.coroutines.CoroutineScope

class AppContainer(
    context: Context,
    appScope: CoroutineScope
) {
    private val appContext = context.applicationContext
    private val database = IChatDatabase.create(appContext)
    private val preferences = PreferencesStore(appContext)
    private val attachmentStore = AttachmentStore(appContext)
    private val socketClient = IChatSocketClient(appScope)
    private val notificationManager = ChatNotificationManager(appContext).also { it.createChannels() }

    val repository = IChatRepository(
        context = appContext,
        appScope = appScope,
        database = database,
        preferences = preferences,
        attachmentStore = attachmentStore,
        socketClient = socketClient,
        notificationManager = notificationManager
    )
}
