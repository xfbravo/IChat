package com.ichat.android.ui.moments

import android.net.Uri
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.ichat.android.data.repository.IChatRepository
import kotlinx.coroutines.launch

class MomentsViewModel(
    private val repository: IChatRepository
) : ViewModel() {
    val moments = repository.moments
    val refreshing = repository.momentsRefreshing
    val publishing = repository.momentPublishing
    val status = repository.momentsStatus

    fun refresh(targetUserId: String = "") {
        viewModelScope.launch {
            runCatching { repository.refreshMoments(targetUserId = targetUserId) }
        }
    }

    fun createMoment(content: String, imageUris: List<Uri>) {
        viewModelScope.launch {
            runCatching { repository.createMoment(content, imageUris) }
        }
    }
}
