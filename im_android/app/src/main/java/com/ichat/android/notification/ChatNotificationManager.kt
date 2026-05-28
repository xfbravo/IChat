package com.ichat.android.notification

import android.Manifest
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import androidx.core.content.ContextCompat
import com.ichat.android.MainActivity
import com.ichat.android.R

/**
 * 仅负责本地通知展示和点击路由。消息存储、未读数和页面状态仍由 Repository/ViewModel 管理。
 */
class ChatNotificationManager(private val context: Context) {
    private val channelId = "chat_messages"

    fun createChannels() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return
        val channel = NotificationChannel(
            channelId,
            "聊天消息",
            NotificationManager.IMPORTANCE_HIGH
        ).apply {
            description = "联系人和群聊的新消息提醒"
        }
        context.getSystemService(NotificationManager::class.java).createNotificationChannel(channel)
    }

    fun showMessage(peerId: String, chatType: String, title: String, preview: String) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU &&
            ContextCompat.checkSelfPermission(context, Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED
        ) {
            return
        }

        val intent = Intent(context, MainActivity::class.java).apply {
            flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_SINGLE_TOP or Intent.FLAG_ACTIVITY_CLEAR_TOP
            putExtra(NotificationRoutes.ExtraPeerId, peerId)
            putExtra(NotificationRoutes.ExtraChatType, chatType)
            putExtra(NotificationRoutes.ExtraTitle, title)
        }
        val pendingIntent = PendingIntent.getActivity(
            context,
            (chatType + peerId).hashCode(),
            intent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )

        val notification = NotificationCompat.Builder(context, channelId)
            .setSmallIcon(R.drawable.ic_stat_message)
            .setContentTitle(title)
            .setContentText(preview)
            .setStyle(NotificationCompat.BigTextStyle().bigText(preview))
            .setAutoCancel(true)
            .setContentIntent(pendingIntent)
            .setPriority(NotificationCompat.PRIORITY_HIGH)
            .build()

        NotificationManagerCompat.from(context)
            .notify((chatType + peerId).hashCode(), notification)
    }
}
