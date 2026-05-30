package com.ichat.android.ui.chat

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.ichat.android.data.db.ChatMessageEntity
import com.ichat.android.data.db.ConversationEntity
import com.ichat.android.data.db.FriendEntity
import com.ichat.android.data.db.GroupEntity
import com.ichat.android.data.model.ChatTarget
import com.ichat.android.data.model.MessageDraft
import com.ichat.android.data.repository.IChatRepository
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.flatMapLatest
import kotlinx.coroutines.flow.flowOf
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch

@OptIn(ExperimentalCoroutinesApi::class)
class MessagesViewModel(
    private val repository: IChatRepository
) : ViewModel() {
    private companion object {
        const val MessagePageSize = 30
    }

    val currentUser = repository.currentUser

    val conversations: StateFlow<List<ConversationEntity>> = repository.observeConversations()
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(5_000), emptyList())

    val friends: StateFlow<List<FriendEntity>> = repository.observeFriends()
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(5_000), emptyList())

    val groups: StateFlow<List<GroupEntity>> = repository.observeGroups()
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(5_000), emptyList())

    private val _selectedTarget = MutableStateFlow<ChatTarget?>(null)
    val selectedTarget: StateFlow<ChatTarget?> = _selectedTarget.asStateFlow()

    private val _messageLimit = MutableStateFlow(MessagePageSize)
    private var lastOlderRequestKey = ""

    val messages: StateFlow<List<ChatMessageEntity>> = _selectedTarget
        .flatMapLatest { target: ChatTarget? ->
            target?.let { chatTarget ->
                _messageLimit.flatMapLatest { limit ->
                    repository.observeMessages(chatTarget, limit)
                }
            } ?: flowOf(emptyList())
        }
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(5_000), emptyList())

    private val _draft = MutableStateFlow("")
    val draft: StateFlow<String> = _draft.asStateFlow()

    private val _actionStatus = MutableStateFlow("")
    val actionStatus: StateFlow<String> = _actionStatus.asStateFlow()

    fun updateDraft(value: String) {
        _draft.value = value
    }

    fun openConversation(conversation: ConversationEntity) {
        openConversation(
            ChatTarget(
                peerId = conversation.peerId,
                chatType = conversation.chatType,
                title = conversation.title.ifBlank { conversation.peerId }
            )
        )
    }

    fun openConversation(target: ChatTarget) {
        _messageLimit.value = MessagePageSize
        lastOlderRequestKey = ""
        _selectedTarget.value = target
        viewModelScope.launch {
            repository.openConversation(target)
        }
    }

    fun backToConversations() {
        _selectedTarget.value = null
    }

    fun sendText() {
        val target = _selectedTarget.value ?: return
        val text = _draft.value.trim()
        if (text.isBlank()) return
        _draft.value = ""
        viewModelScope.launch {
            repository.sendTextMessage(
                MessageDraft(
                    peerId = target.peerId,
                    chatType = target.chatType,
                    content = text
                )
            )
        }
    }

    fun loadOlderMessages() {
        val target = _selectedTarget.value ?: return
        val oldest = messages.value
            .filter { it.conversationKey == target.conversationKey }
            .firstOrNull() ?: return
        val requestKey = "${target.conversationKey}:${oldest.msgId}"
        if (requestKey == lastOlderRequestKey) return

        // 先扩大本地可见窗口；若本地没有更早缓存，再向服务端按最早消息时间分页补齐。
        lastOlderRequestKey = requestKey
        _messageLimit.value += MessagePageSize
        viewModelScope.launch {
            repository.loadOlderMessages(target, oldest, MessagePageSize)
        }
    }

    fun sendFriendRequest(account: String, remark: String) {
        viewModelScope.launch {
            runCatching { repository.sendFriendRequest(account, remark) }
                .onSuccess { _actionStatus.value = "好友请求已发送" }
                .onFailure { _actionStatus.value = it.message ?: "好友请求发送失败" }
        }
    }

    fun createGroup(groupName: String, memberIds: List<String>) {
        viewModelScope.launch {
            runCatching { repository.createGroup(groupName, memberIds) }
                .onSuccess { _actionStatus.value = "群聊创建请求已发送" }
                .onFailure { _actionStatus.value = it.message ?: "群聊创建失败" }
        }
    }
}
