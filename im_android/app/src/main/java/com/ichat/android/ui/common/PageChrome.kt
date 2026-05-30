package com.ichat.android.ui.common

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.ichat.android.R

/**
 * 各首页复用的顶部标题和搜索框，保证 Android 端和桌面端页面层级一致。
 */
@Composable
fun PageTitle(text: String, modifier: Modifier = Modifier) {
    Text(
        text = text,
        fontWeight = FontWeight.Bold,
        fontSize = 20.sp,
        textAlign = TextAlign.Center,
        modifier = modifier
            .fillMaxWidth()
            .padding(top = 34.dp, bottom = 8.dp)
    )
}

@Composable
fun HomeTopBar(
    title: String,
    onAddFriend: () -> Unit,
    onCreateGroup: () -> Unit,
    modifier: Modifier = Modifier
) {
    var expanded by remember { mutableStateOf(false) }

    // 菜单放在右侧，左侧保留同等宽度，让标题仍保持视觉居中。
    Row(
        modifier = modifier
            .fillMaxWidth()
            .padding(top = 34.dp, bottom = 8.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Spacer(Modifier.width(56.dp))
        Text(
            text = title,
            fontWeight = FontWeight.Bold,
            fontSize = 20.sp,
            textAlign = TextAlign.Center,
            modifier = Modifier.weight(1f)
        )
        Box(modifier = Modifier.width(56.dp), contentAlignment = Alignment.CenterEnd) {
            IconButton(onClick = { expanded = true }) {
                Icon(
                    painter = painterResource(R.drawable.ic_more_menu),
                    contentDescription = "更多操作"
                )
            }
            DropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
                DropdownMenuItem(
                    text = { Text("添加好友") },
                    onClick = {
                        expanded = false
                        onAddFriend()
                    }
                )
                DropdownMenuItem(
                    text = { Text("发起群聊") },
                    onClick = {
                        expanded = false
                        onCreateGroup()
                    }
                )
            }
        }
    }
}

@Composable
fun BackIconButton(onClick: () -> Unit, modifier: Modifier = Modifier.width(72.dp)) {
    IconButton(onClick = onClick, modifier = modifier) {
        Icon(
            painter = painterResource(R.drawable.ic_arrow_back),
            contentDescription = "返回"
        )
    }
}

@Composable
fun IChatSearchField(
    value: String,
    onValueChange: (String) -> Unit,
    placeholder: String,
    modifier: Modifier = Modifier
) {
    OutlinedTextField(
        value = value,
        onValueChange = onValueChange,
        singleLine = true,
        shape = RoundedCornerShape(24.dp),
        leadingIcon = {
            Icon(
                painter = painterResource(R.drawable.ic_search),
                contentDescription = null
            )
        },
        placeholder = { Text(placeholder) },
        modifier = modifier
            .fillMaxWidth()
            .padding(horizontal = 20.dp, vertical = 6.dp)
    )
}
