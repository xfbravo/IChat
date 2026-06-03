package com.ichat.android.ui.contacts

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.ichat.android.data.db.FriendEntity
import com.ichat.android.data.db.GroupEntity
import com.ichat.android.data.model.FriendRequest
import com.ichat.android.data.repository.IChatRepository
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch

class ContactsViewModel(
    private val repository: IChatRepository
) : ViewModel() {
    val currentUser = repository.currentUser

    val friends: StateFlow<List<FriendEntity>> = repository.observeFriends()
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(5_000), emptyList())

    val groups: StateFlow<List<GroupEntity>> = repository.observeGroups()
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(5_000), emptyList())

    val friendRequests: StateFlow<List<FriendRequest>> = repository.friendRequests
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(5_000), emptyList())

    private val _actionStatus = MutableStateFlow("")
    val actionStatus: StateFlow<String> = _actionStatus.asStateFlow()

    fun refresh() {
        viewModelScope.launch {
            runCatching { repository.refreshContacts() }
                .onFailure { _actionStatus.value = it.message ?: "联系人刷新失败" }
        }
    }

    fun refreshFriendRequests() {
        viewModelScope.launch {
            runCatching { repository.refreshFriendRequests() }
                .onFailure { _actionStatus.value = it.message ?: "好友请求刷新失败" }
        }
    }

    fun respondFriendRequest(requestId: String, accept: Boolean) {
        viewModelScope.launch {
            runCatching { repository.respondFriendRequest(requestId, accept) }
                .onSuccess { _actionStatus.value = if (accept) "已同意好友请求" else "已拒绝好友请求" }
                .onFailure { _actionStatus.value = it.message ?: "处理好友请求失败" }
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
