package com.ichat.android.ui.me

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.ichat.android.data.repository.IChatRepository
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

class MeViewModel(
    private val repository: IChatRepository
) : ViewModel() {
    val currentUser = repository.currentUser
    private val _status = MutableStateFlow("")
    val status = _status.asStateFlow()

    fun logout() {
        viewModelScope.launch {
            repository.logout()
        }
    }

    fun changePassword(oldPassword: String, newPassword: String) {
        viewModelScope.launch {
            runCatching {
                repository.changePassword(oldPassword, newPassword)
                _status.value = "修改密码请求已发送"
            }.onFailure { error ->
                _status.value = error.message ?: "修改失败"
            }
        }
    }
}
