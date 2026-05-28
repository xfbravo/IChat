package com.ichat.android.ui.chat

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
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
import androidx.compose.material3.Card
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.fragment.app.viewModels
import com.ichat.android.IChatApplication
import com.ichat.android.data.db.ChatMessageEntity
import com.ichat.android.data.db.ConversationEntity
import com.ichat.android.ui.main.MainViewModel
import com.ichat.android.ui.main.ViewModelFactory
import com.ichat.android.ui.theme.IChatTheme

class MessagesFragment : Fragment() {
    private val factory: ViewModelFactory by lazy {
        ViewModelFactory((requireActivity().application as IChatApplication).container.repository)
    }
    private val viewModel: MessagesViewModel by viewModels { factory }
    private val mainViewModel: MainViewModel by activityViewModels { factory }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        return ComposeView(requireContext()).apply {
            setContent {
                IChatTheme {
                    MessagesScreen(viewModel, mainViewModel)
                }
            }
        }
    }
}

@Composable
private fun MessagesScreen(viewModel: MessagesViewModel, mainViewModel: MainViewModel) {
    val pending by mainViewModel.pendingChatTarget.collectAsState()
    val conversations by viewModel.conversations.collectAsState()
    val selected by viewModel.selectedTarget.collectAsState()
    val messages by viewModel.messages.collectAsState()
    val draft by viewModel.draft.collectAsState()

    LaunchedEffect(pending) {
        val target = pending ?: return@LaunchedEffect
        viewModel.openConversation(target)
        mainViewModel.consumePendingChatTarget()
    }

    if (selected == null) {
        ConversationList(conversations, onOpen = viewModel::openConversation)
    } else {
        ChatPanel(
            title = selected.title,
            messages = messages,
            draft = draft,
            onDraftChange = viewModel::updateDraft,
            onSend = viewModel::sendText,
            onBack = viewModel::backToConversations
        )
    }
}

@Composable
private fun ConversationList(
    conversations: List<ConversationEntity>,
    onOpen: (ConversationEntity) -> Unit
) {
    Column(Modifier.fillMaxSize()) {
        Text(
            text = "消息",
            fontWeight = FontWeight.Bold,
            modifier = Modifier.padding(20.dp)
        )
        LazyColumn(Modifier.fillMaxSize()) {
            items(conversations, key = { it.conversationKey }) { conversation ->
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clickable { onOpen(conversation) }
                        .padding(horizontal = 20.dp, vertical = 14.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column(Modifier.weight(1f)) {
                        Text(conversation.title.ifBlank { conversation.peerId }, fontWeight = FontWeight.SemiBold)
                        Spacer(Modifier.height(4.dp))
                        Text(conversation.lastMessage)
                    }
                    if (conversation.unreadCount > 0) {
                        Text(conversation.unreadCount.toString())
                    }
                }
                HorizontalDivider()
            }
        }
    }
}

@Composable
private fun ChatPanel(
    title: String,
    messages: List<ChatMessageEntity>,
    draft: String,
    onDraftChange: (String) -> Unit,
    onSend: () -> Unit,
    onBack: () -> Unit
) {
    Column(Modifier.fillMaxSize()) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 12.dp, vertical = 8.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            TextButton(onClick = onBack) { Text("返回") }
            Text(title, fontWeight = FontWeight.Bold)
        }
        HorizontalDivider()
        LazyColumn(
            modifier = Modifier
                .weight(1f)
                .fillMaxWidth()
                .padding(horizontal = 12.dp, vertical = 8.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            items(messages, key = { it.msgId }) { message ->
                MessageBubble(message)
            }
        }
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(10.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            OutlinedTextField(
                value = draft,
                onValueChange = onDraftChange,
                modifier = Modifier.weight(1f),
                placeholder = { Text("输入消息") }
            )
            Spacer(Modifier.width(8.dp))
            Button(onClick = onSend) {
                Text("发送")
            }
        }
    }
}

@Composable
private fun MessageBubble(message: ChatMessageEntity) {
    val align = if (message.isMine) Alignment.End else Alignment.Start
    val text = when (message.contentType) {
        "image" -> "[图片]"
        "file" -> "[文件] ${message.content}"
        "voice" -> "[语音]"
        "video" -> "[视频暂未支持播放]"
        else -> message.content
    }
    Column(
        modifier = Modifier.fillMaxWidth(),
        horizontalAlignment = align
    ) {
        Card {
            Column(Modifier.padding(horizontal = 12.dp, vertical = 8.dp)) {
                Text(text)
                if (message.isMine && message.sendStatus.isNotBlank()) {
                    Text(message.sendStatus)
                }
            }
        }
    }
}
