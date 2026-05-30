package com.ichat.android

import android.app.Application
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.ProcessLifecycleOwner
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob

class IChatApplication : Application(), DefaultLifecycleObserver {
    private val appJob = SupervisorJob()
    private val appScope = CoroutineScope(appJob + Dispatchers.Default)

    lateinit var container: AppContainer
        private set

    override fun onCreate() {
        super<Application>.onCreate()
        container = AppContainer(this, appScope)
        ProcessLifecycleOwner.get().lifecycle.addObserver(this)
    }

    override fun onStart(owner: LifecycleOwner) {
        container.repository.setAppForeground(true)
    }

    override fun onStop(owner: LifecycleOwner) {
        container.repository.setAppForeground(false)
    }

    override fun onTerminate() {
        appJob.cancel()
        super<Application>.onTerminate()
    }
}
