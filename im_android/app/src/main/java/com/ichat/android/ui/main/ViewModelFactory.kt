package com.ichat.android.ui.main

import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import com.ichat.android.data.repository.IChatRepository
import com.ichat.android.ui.auth.AuthViewModel
import com.ichat.android.ui.chat.MessagesViewModel
import com.ichat.android.ui.contacts.ContactsViewModel
import com.ichat.android.ui.me.MeViewModel
import com.ichat.android.ui.moments.MomentsViewModel

class ViewModelFactory(
    private val repository: IChatRepository
) : ViewModelProvider.Factory {
    @Suppress("UNCHECKED_CAST")
    override fun <T : ViewModel> create(modelClass: Class<T>): T {
        return when {
            modelClass.isAssignableFrom(MainViewModel::class.java) -> MainViewModel(repository)
            modelClass.isAssignableFrom(AuthViewModel::class.java) -> AuthViewModel(repository)
            modelClass.isAssignableFrom(MessagesViewModel::class.java) -> MessagesViewModel(repository)
            modelClass.isAssignableFrom(ContactsViewModel::class.java) -> ContactsViewModel(repository)
            modelClass.isAssignableFrom(MomentsViewModel::class.java) -> MomentsViewModel(repository)
            modelClass.isAssignableFrom(MeViewModel::class.java) -> MeViewModel(repository)
            else -> error("Unknown ViewModel: ${modelClass.name}")
        } as T
    }
}
