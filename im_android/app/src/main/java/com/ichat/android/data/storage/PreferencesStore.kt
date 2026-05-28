package com.ichat.android.data.storage

import android.content.Context
import com.ichat.android.data.model.CurrentUser

class PreferencesStore(context: Context) {
    private val prefs = context.getSharedPreferences("ichat", Context.MODE_PRIVATE)

    fun loadUser(): CurrentUser? {
        val userId = prefs.getString(KeyUserId, null)?.takeIf { it.isNotBlank() } ?: return null
        return CurrentUser(
            userId = userId,
            nickname = prefs.getString(KeyNickname, "") ?: "",
            avatarUrl = prefs.getString(KeyAvatarUrl, "") ?: "",
            token = prefs.getString(KeyToken, "") ?: "",
            gender = prefs.getString(KeyGender, "") ?: "",
            region = prefs.getString(KeyRegion, "") ?: "",
            signature = prefs.getString(KeySignature, "") ?: ""
        )
    }

    fun saveUser(user: CurrentUser) {
        prefs.edit()
            .putString(KeyUserId, user.userId)
            .putString(KeyNickname, user.nickname)
            .putString(KeyAvatarUrl, user.avatarUrl)
            .putString(KeyToken, user.token)
            .putString(KeyGender, user.gender)
            .putString(KeyRegion, user.region)
            .putString(KeySignature, user.signature)
            .apply()
    }

    fun clearUser() {
        // 退出登录只清账号态，不重置服务端地址等本地配置。
        prefs.edit()
            .remove(KeyUserId)
            .remove(KeyNickname)
            .remove(KeyAvatarUrl)
            .remove(KeyToken)
            .remove(KeyGender)
            .remove(KeyRegion)
            .remove(KeySignature)
            .apply()
    }

    fun serverHost(): String = prefs.getString("server_host", "61.184.13.118") ?: "61.184.13.118"

    fun serverPort(): Int = prefs.getInt("server_port", 8080)

    private companion object {
        const val KeyUserId = "user_id"
        const val KeyNickname = "nickname"
        const val KeyAvatarUrl = "avatar_url"
        const val KeyToken = "token"
        const val KeyGender = "gender"
        const val KeyRegion = "region"
        const val KeySignature = "signature"
    }
}
