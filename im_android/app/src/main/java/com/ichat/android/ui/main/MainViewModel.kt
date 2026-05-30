package com.ichat.android.ui.main

import androidx.lifecycle.ViewModel
import com.ichat.android.data.model.ChatTarget
import com.ichat.android.data.repository.IChatRepository
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

enum class MainTab(val title: String) {
    Messages("消息"),
    Contacts("联系人"),
    Moments("朋友圈"),
    Me("我")
}

class MainViewModel(
    repository: IChatRepository
) : ViewModel() {
    val currentUser = repository.currentUser
    val connectionState = repository.connectionState

    private val _selectedTab = MutableStateFlow(MainTab.Messages)
    val selectedTab: StateFlow<MainTab> = _selectedTab.asStateFlow()

    private val _bottomNavigationVisible = MutableStateFlow(true)
    val bottomNavigationVisible: StateFlow<Boolean> = _bottomNavigationVisible.asStateFlow()

    private val _pendingChatTarget = MutableStateFlow<ChatTarget?>(null)
    val pendingChatTarget: StateFlow<ChatTarget?> = _pendingChatTarget.asStateFlow()

    fun selectTab(tab: MainTab) {
        _bottomNavigationVisible.value = true
        _selectedTab.value = tab
    }

    fun setBottomNavigationVisible(visible: Boolean) {
        _bottomNavigationVisible.value = visible
    }

    fun openChat(peerId: String, chatType: String, title: String) {
        if (peerId.isBlank()) return
        _selectedTab.value = MainTab.Messages
        _pendingChatTarget.value = ChatTarget(peerId, chatType.ifBlank { "p2p" }, title.ifBlank { peerId })
    }

    fun openChatFromNotification(peerId: String, chatType: String, title: String) {
        openChat(peerId, chatType, title)
    }

    fun consumePendingChatTarget() {
        _pendingChatTarget.value = null
    }
}
