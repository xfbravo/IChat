package com.ichat.android.ui.chat

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.ichat.android.data.db.ChatMessageEntity
import com.ichat.android.data.db.ConversationEntity
import com.ichat.android.data.model.ChatTarget
import com.ichat.android.data.model.MessageDraft
import com.ichat.android.data.repository.IChatRepository
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.flatMapLatest
import kotlinx.coroutines.flow.flowOf
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch

class MessagesViewModel(
    private val repository: IChatRepository
) : ViewModel() {
    val conversations: StateFlow<List<ConversationEntity>> = repository.observeConversations()
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(5_000), emptyList())

    private val _selectedTarget = MutableStateFlow<ChatTarget?>(null)
    val selectedTarget: StateFlow<ChatTarget?> = _selectedTarget.asStateFlow()

    val messages: StateFlow<List<ChatMessageEntity>> = _selectedTarget
        .flatMapLatest { target: ChatTarget? ->
            target?.let { repository.observeMessages(it) } ?: flowOf(emptyList())
        }
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(5_000), emptyList())

    private val _draft = MutableStateFlow("")
    val draft: StateFlow<String> = _draft.asStateFlow()

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
}
