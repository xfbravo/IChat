package com.ichat.android.ui.me

import android.net.Uri
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
    val profileStatus = repository.profileStatus
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

    fun saveProfile(
        nickname: String,
        gender: String,
        region: String,
        signature: String,
        avatarUri: Uri?
    ) {
        viewModelScope.launch {
            runCatching {
                repository.updateProfile(
                    nickname = nickname.trim(),
                    gender = gender.ifBlank { "男" },
                    region = region.trim(),
                    signature = signature.trim()
                )
                if (avatarUri != null) {
                    repository.updateAvatar(avatarUri)
                }
            }.onFailure { error ->
                _status.value = error.message ?: "资料保存失败"
            }
        }
    }
}
