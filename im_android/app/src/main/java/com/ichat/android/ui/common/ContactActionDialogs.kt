package com.ichat.android.ui.common

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Checkbox
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.ichat.android.data.db.FriendEntity
import com.ichat.android.data.model.CurrentUser
import java.time.LocalDate
import java.time.format.DateTimeFormatter

@Composable
fun ContactActionDialogs(
    showAddFriend: Boolean,
    showCreateGroup: Boolean,
    friends: List<FriendEntity>,
    currentUser: CurrentUser?,
    onDismissAddFriend: () -> Unit,
    onDismissCreateGroup: () -> Unit,
    onSendFriendRequest: (String, String) -> Unit,
    onCreateGroup: (String, List<String>) -> Unit
) {
    if (showAddFriend) {
        AddFriendDialog(
            onDismiss = onDismissAddFriend,
            onSend = { account, remark ->
                onSendFriendRequest(account, remark)
                onDismissAddFriend()
            }
        )
    }

    if (showCreateGroup) {
        CreateGroupDialog(
            friends = friends,
            currentUser = currentUser,
            onDismiss = onDismissCreateGroup,
            onCreate = { groupName, memberIds ->
                onCreateGroup(groupName, memberIds)
                onDismissCreateGroup()
            }
        )
    }
}

@Composable
private fun AddFriendDialog(onDismiss: () -> Unit, onSend: (String, String) -> Unit) {
    var account by remember { mutableStateOf("") }
    var remark by remember { mutableStateOf("") }
    val canSend = account.trim().isNotBlank()

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("添加好友") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                OutlinedTextField(
                    value = account,
                    onValueChange = { account = it },
                    singleLine = true,
                    label = { Text("手机号") },
                    modifier = Modifier.fillMaxWidth()
                )
                OutlinedTextField(
                    value = remark,
                    onValueChange = { remark = it },
                    label = { Text("备注") },
                    modifier = Modifier.fillMaxWidth()
                )
            }
        },
        confirmButton = {
            Button(
                enabled = canSend,
                onClick = { onSend(account.trim(), remark.trim()) }
            ) {
                Text("发送")
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text("取消")
            }
        }
    )
}

@Composable
private fun CreateGroupDialog(
    friends: List<FriendEntity>,
    currentUser: CurrentUser?,
    onDismiss: () -> Unit,
    onCreate: (String, List<String>) -> Unit
) {
    val selectedIds = remember { mutableStateListOf<String>() }
    var groupName by remember(currentUser) { mutableStateOf(defaultGroupName(currentUser)) }
    val canCreate = groupName.trim().isNotBlank() && selectedIds.isNotEmpty()

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("发起群聊") },
        text = {
            Column {
                OutlinedTextField(
                    value = groupName,
                    onValueChange = { groupName = it },
                    singleLine = true,
                    label = { Text("群聊名称") },
                    modifier = Modifier.fillMaxWidth()
                )
                Spacer(Modifier.height(12.dp))
                if (friends.isEmpty()) {
                    Text("暂无可选择的联系人")
                } else {
                    Text("选择联系人", fontWeight = FontWeight.SemiBold)
                    Spacer(Modifier.height(6.dp))
                    LazyColumn(modifier = Modifier.heightIn(max = 320.dp)) {
                        items(friends, key = { it.userId }) { friend ->
                            val checked = selectedIds.contains(friend.userId)
                            Row(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .clickable {
                                        if (checked) {
                                            selectedIds.remove(friend.userId)
                                        } else {
                                            selectedIds.add(friend.userId)
                                        }
                                    }
                                    .padding(vertical = 8.dp),
                                verticalAlignment = Alignment.CenterVertically
                            ) {
                                Checkbox(
                                    checked = checked,
                                    onCheckedChange = { next ->
                                        if (next) {
                                            if (!selectedIds.contains(friend.userId)) {
                                                selectedIds.add(friend.userId)
                                            }
                                        } else {
                                            selectedIds.remove(friend.userId)
                                        }
                                    }
                                )
                                Spacer(Modifier.width(8.dp))
                                IChatAvatar(
                                    avatarUrl = friend.avatarUrl,
                                    displayName = friend.displayName(),
                                    size = 40.dp
                                )
                                Spacer(Modifier.width(10.dp))
                                Column {
                                    Text(friend.displayName(), fontWeight = FontWeight.SemiBold)
                                    Text(friend.userId)
                                }
                            }
                        }
                    }
                    Spacer(Modifier.height(6.dp))
                    Text("已选择 ${selectedIds.size} 人")
                }
            }
        },
        confirmButton = {
            Button(
                enabled = canCreate,
                onClick = { onCreate(groupName.trim(), selectedIds.toList()) }
            ) {
                Text("创建")
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text("取消")
            }
        }
    )
}

private fun FriendEntity.displayName(): String = remark.ifBlank { nickname }.ifBlank { userId }

private fun defaultGroupName(currentUser: CurrentUser?): String {
    val initiator = currentUser?.nickname?.ifBlank { currentUser.userId }.orEmpty().ifBlank { "我" }
    val date = LocalDate.now().format(DateTimeFormatter.ofPattern("yy-MM-dd"))
    return "${initiator}发起的群聊$date"
}
