package com.ichat.android.ui.contacts

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.fragment.app.viewModels
import androidx.compose.ui.platform.ComposeView
import com.ichat.android.IChatApplication
import com.ichat.android.data.db.FriendEntity
import com.ichat.android.data.db.GroupEntity
import com.ichat.android.ui.main.MainViewModel
import com.ichat.android.ui.main.ViewModelFactory
import com.ichat.android.ui.theme.IChatTheme

class ContactsFragment : Fragment() {
    private val factory: ViewModelFactory by lazy {
        ViewModelFactory((requireActivity().application as IChatApplication).container.repository)
    }
    private val viewModel: ContactsViewModel by viewModels { factory }
    private val mainViewModel: MainViewModel by activityViewModels { factory }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        return ComposeView(requireContext()).apply {
            setContent {
                IChatTheme {
                    ContactsScreen(viewModel, mainViewModel)
                }
            }
        }
    }
}

@Composable
private fun ContactsScreen(viewModel: ContactsViewModel, mainViewModel: MainViewModel) {
    val friends by viewModel.friends.collectAsState()
    val groups by viewModel.groups.collectAsState()
    LaunchedEffect(Unit) {
        viewModel.refresh()
    }

    LazyColumn(Modifier.fillMaxSize()) {
        item {
            Text("联系人", fontWeight = FontWeight.Bold, modifier = Modifier.padding(20.dp))
            Text("群聊", fontWeight = FontWeight.SemiBold, modifier = Modifier.padding(horizontal = 20.dp, vertical = 8.dp))
        }
        items(groups, key = { it.groupId }) { group ->
            GroupRow(group) {
                mainViewModel.openChatFromNotification(group.groupId, "group", group.groupName)
            }
        }
        item {
            Spacer(Modifier.height(12.dp))
            Text("好友", fontWeight = FontWeight.SemiBold, modifier = Modifier.padding(horizontal = 20.dp, vertical = 8.dp))
        }
        items(friends, key = { it.userId }) { friend ->
            FriendRow(friend) {
                mainViewModel.openChatFromNotification(
                    peerId = friend.userId,
                    chatType = "p2p",
                    title = friend.remark.ifBlank { friend.nickname }.ifBlank { friend.userId }
                )
            }
        }
    }
}

@Composable
private fun FriendRow(friend: FriendEntity, onClick: () -> Unit) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(onClick = onClick)
            .padding(horizontal = 20.dp, vertical = 14.dp)
    ) {
        Column {
            Text(friend.remark.ifBlank { friend.nickname }.ifBlank { friend.userId })
            Text(friend.userId)
        }
    }
    HorizontalDivider()
}

@Composable
private fun GroupRow(group: GroupEntity, onClick: () -> Unit) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(onClick = onClick)
            .padding(horizontal = 20.dp, vertical = 14.dp)
    ) {
        Column {
            Text(group.groupName.ifBlank { group.groupId })
            Text("${group.memberCount} 人")
        }
    }
    HorizontalDivider()
}
