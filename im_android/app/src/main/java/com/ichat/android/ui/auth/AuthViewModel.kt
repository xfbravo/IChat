package com.ichat.android.ui.auth

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.ichat.android.data.repository.IChatRepository
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch

data class AuthUiState(
    val registerMode: Boolean = false,
    val userId: String = "",
    val phone: String = "",
    val nickname: String = "",
    val password: String = "",
    val confirmPassword: String = "",
    val loading: Boolean = false,
    val message: String = ""
)

class AuthViewModel(
    private val repository: IChatRepository
) : ViewModel() {
    private val _state = MutableStateFlow(AuthUiState())
    val state: StateFlow<AuthUiState> = _state.asStateFlow()

    fun toggleMode() {
        _state.update { it.copy(registerMode = !it.registerMode, message = "") }
    }

    fun updateUserId(value: String) = _state.update { it.copy(userId = value, message = "") }
    fun updatePhone(value: String) = _state.update { it.copy(phone = value, message = "") }
    fun updateNickname(value: String) = _state.update { it.copy(nickname = value, message = "") }
    fun updatePassword(value: String) = _state.update { it.copy(password = value, message = "") }
    fun updateConfirmPassword(value: String) = _state.update { it.copy(confirmPassword = value, message = "") }

    fun submit() {
        val snapshot = _state.value
        if (snapshot.loading) return
        if (snapshot.registerMode) {
            register(snapshot)
        } else {
            login(snapshot)
        }
    }

    private fun login(snapshot: AuthUiState) {
        if (snapshot.userId.isBlank() || snapshot.password.isBlank()) {
            _state.update { it.copy(message = "请输入账号和密码") }
            return
        }
        viewModelScope.launch {
            _state.update { it.copy(loading = true, message = "") }
            val result = runCatching {
                repository.login(snapshot.userId.trim(), snapshot.password)
            }.getOrElse { error ->
                _state.update { it.copy(loading = false, message = error.message ?: "登录失败") }
                return@launch
            }
            _state.update {
                it.copy(
                    loading = false,
                    message = if (result.code == 0) "" else result.message.ifBlank { "登录失败" }
                )
            }
        }
    }

    private fun register(snapshot: AuthUiState) {
        if (snapshot.phone.isBlank() || snapshot.nickname.isBlank() || snapshot.password.length < 6) {
            _state.update { it.copy(message = "请填写手机号、昵称和至少6位密码") }
            return
        }
        if (snapshot.password != snapshot.confirmPassword) {
            _state.update { it.copy(message = "两次密码不一致") }
            return
        }
        viewModelScope.launch {
            _state.update { it.copy(loading = true, message = "") }
            val result = runCatching {
                repository.register(snapshot.phone.trim(), snapshot.nickname.trim(), snapshot.password)
            }.getOrElse { error ->
                _state.update { it.copy(loading = false, message = error.message ?: "注册失败") }
                return@launch
            }
            _state.update {
                if (result.code == 0) {
                    AuthUiState(registerMode = false, userId = result.user?.userId ?: "", message = "注册成功，请登录")
                } else {
                    it.copy(loading = false, message = result.message.ifBlank { "注册失败" })
                }
            }
        }
    }
}
