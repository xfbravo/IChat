package com.ichat.android.ui.contacts

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Button
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.fragment.app.Fragment
import androidx.fragment.app.viewModels
import com.ichat.android.IChatApplication
import com.ichat.android.data.model.FriendRequest
import com.ichat.android.ui.common.BackIconButton
import com.ichat.android.ui.common.IChatAvatar
import com.ichat.android.ui.main.ViewModelFactory
import com.ichat.android.ui.theme.IChatTheme

class FriendRequestsFragment : Fragment() {
    private val viewModel: ContactsViewModel by viewModels {
        ViewModelFactory((requireActivity().application as IChatApplication).container.repository)
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        return ComposeView(requireContext()).apply {
            setContent {
                IChatTheme {
                    FriendRequestsScreen(
                        viewModel = viewModel,
                        onBack = { parentFragmentManager.popBackStack() }
                    )
                }
            }
        }
    }
}

@Composable
private fun FriendRequestsScreen(viewModel: ContactsViewModel, onBack: () -> Unit) {
    val requests by viewModel.friendRequests.collectAsState()
    val status by viewModel.actionStatus.collectAsState()

    LaunchedEffect(Unit) {
        viewModel.refreshFriendRequests()
    }
    BackHandler(onBack = onBack)

    Column(Modifier.fillMaxSize()) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(top = 34.dp, bottom = 8.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            BackIconButton(onClick = onBack)
            Text(
                text = "新的朋友",
                fontWeight = FontWeight.Bold,
                fontSize = 20.sp,
                textAlign = TextAlign.Center,
                modifier = Modifier.weight(1f)
            )
            Spacer(Modifier.width(72.dp))
        }
        HorizontalDivider()
        if (status.isNotBlank()) {
            Text(
                text = status,
                color = Color(0xFF6B756E),
                modifier = Modifier.padding(horizontal = 20.dp, vertical = 10.dp)
            )
        }
        LazyColumn(Modifier.fillMaxSize()) {
            if (requests.isEmpty()) {
                item {
                    Text(
                        text = "暂无新的好友请求",
                        color = Color(0xFF6B756E),
                        modifier = Modifier.padding(horizontal = 20.dp, vertical = 18.dp)
                    )
                }
            }
            items(requests, key = { it.requestId }) { request ->
                FriendRequestRow(
                    request = request,
                    onAccept = { viewModel.respondFriendRequest(request.requestId, true) },
                    onReject = { viewModel.respondFriendRequest(request.requestId, false) }
                )
            }
        }
    }
}

@Composable
private fun FriendRequestRow(request: FriendRequest, onAccept: () -> Unit, onReject: () -> Unit) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 20.dp, vertical = 14.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        IChatAvatar(
            avatarUrl = request.fromAvatar,
            displayName = request.displayName(),
            size = 44.dp
        )
        Spacer(Modifier.width(12.dp))
        Column(modifier = Modifier.weight(1f)) {
            Text(request.displayName(), fontWeight = FontWeight.SemiBold)
            Spacer(Modifier.height(3.dp))
            Text("账号：${request.fromUserId}", color = Color(0xFF6B756E))
            if (request.remark.isNotBlank()) {
                Spacer(Modifier.height(3.dp))
                Text("备注：${request.remark}", color = Color(0xFF6B756E))
            }
            if (request.createTime.isNotBlank()) {
                Spacer(Modifier.height(3.dp))
                Text(request.createTime, color = Color(0xFF8A948D))
            }
        }
        Column {
            Button(onClick = onAccept) {
                Text("同意")
            }
            Spacer(Modifier.height(8.dp))
            OutlinedButton(onClick = onReject) {
                Text("拒绝")
            }
        }
    }
    HorizontalDivider()
}

private fun FriendRequest.displayName(): String = fromNickname.ifBlank { fromUserId }.ifBlank { "好友请求" }
