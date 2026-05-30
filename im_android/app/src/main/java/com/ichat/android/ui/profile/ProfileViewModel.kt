package com.ichat.android.ui.profile

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.ichat.android.data.db.FriendEntity
import com.ichat.android.data.repository.IChatRepository
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch

class ProfileViewModel(
    private val repository: IChatRepository
) : ViewModel() {
    val currentUser = repository.currentUser
    val userProfiles = repository.userProfiles

    val friends: StateFlow<List<FriendEntity>> = repository.observeFriends()
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(5_000), emptyList())

    fun requestUserProfile(userId: String) {
        viewModelScope.launch {
            runCatching { repository.requestUserProfile(userId) }
        }
    }

    fun sendFriendRequest(account: String, remark: String) {
        viewModelScope.launch {
            runCatching { repository.sendFriendRequest(account, remark) }
        }
    }
}
