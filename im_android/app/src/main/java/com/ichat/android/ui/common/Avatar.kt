package com.ichat.android.ui.common

import android.graphics.BitmapFactory
import android.net.Uri
import android.util.Base64
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

data class AvatarProfile(
    val title: String,
    val subtitle: String,
    val avatarUrl: String,
    val detail: String = "",
    val userId: String = "",
    val chatType: String = "p2p",
    val gender: String = "",
    val region: String = "",
    val signature: String = detail,
    val nickname: String = "",
    val remark: String = ""
)

@Composable
fun IChatAvatar(
    avatarUrl: String,
    displayName: String,
    size: Dp,
    modifier: Modifier = Modifier,
    onClick: (() -> Unit)? = null
) {
    val image = remember(avatarUrl) { decodeAvatarBitmap(avatarUrl) }
    val avatarModifier = modifier
        .size(size)
        .clip(CircleShape)
        .background(Color(0xFF2F6F3E))
        .border(1.dp, Color(0xFFD8DEE4), CircleShape)
        .let { base ->
            if (onClick == null) base else base.clickable(onClick = onClick)
        }

    androidx.compose.foundation.layout.Box(
        modifier = avatarModifier,
        contentAlignment = Alignment.Center
    ) {
        if (image != null) {
            Image(
                bitmap = image,
                contentDescription = displayName.ifBlank { "头像" },
                modifier = Modifier.fillMaxSize(),
                contentScale = ContentScale.Crop
            )
        } else {
            Text(
                text = avatarInitial(displayName),
                color = Color.White,
                fontWeight = FontWeight.Bold,
                fontSize = avatarFontSize(size)
            )
        }
    }
}

@Composable
fun AvatarProfileDialog(profile: AvatarProfile?, onDismiss: () -> Unit) {
    if (profile == null) return

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(profile.title.ifBlank { profile.subtitle }) },
        text = {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(16.dp)
            ) {
                IChatAvatar(
                    avatarUrl = profile.avatarUrl,
                    displayName = profile.title.ifBlank { profile.subtitle },
                    size = 64.dp
                )
                Column {
                    if (profile.subtitle.isNotBlank()) {
                        Text(profile.subtitle)
                    }
                    if (profile.detail.isNotBlank()) {
                        Spacer(Modifier.height(6.dp))
                        Text(profile.detail)
                    }
                }
            }
        },
        confirmButton = {
            TextButton(onClick = onDismiss) {
                Text("关闭")
            }
        }
    )
}

private fun decodeAvatarBitmap(avatarUrl: String): ImageBitmap? {
    val value = avatarUrl.trim()
    if (value.isBlank()) return null

    return runCatching {
        // 与 Qt 客户端保持一致：头像优先支持 data URL，其次支持本地文件路径。
        val bitmap = if (value.startsWith("data:image/", ignoreCase = true)) {
            val commaIndex = value.indexOf(',')
            if (commaIndex <= 0) return@runCatching null
            val bytes = Base64.decode(value.substring(commaIndex + 1), Base64.DEFAULT)
            BitmapFactory.decodeByteArray(bytes, 0, bytes.size)
        } else {
            val path = if (value.startsWith("file://", ignoreCase = true)) {
                Uri.parse(value).path.orEmpty()
            } else {
                value
            }
            BitmapFactory.decodeFile(path)
        }
        bitmap?.asImageBitmap()
    }.getOrNull()
}

private fun avatarInitial(displayName: String): String {
    return displayName.trim().take(1).uppercase().ifBlank { "I" }
}

private fun avatarFontSize(size: Dp) = when {
    size.value < 42f -> 14.sp
    size.value < 72f -> 22.sp
    else -> 30.sp
}
