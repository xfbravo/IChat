package com.ichat.android.ui.theme

import androidx.compose.material3.ColorScheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

private val IChatColors: ColorScheme = lightColorScheme(
    primary = Color(0xFF20B15A),
    onPrimary = Color.White,
    secondary = Color(0xFF2D6CDF),
    surface = Color(0xFFF8FAF9),
    onSurface = Color(0xFF1F2A24)
)

@Composable
fun IChatTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = IChatColors,
        content = content
    )
}
