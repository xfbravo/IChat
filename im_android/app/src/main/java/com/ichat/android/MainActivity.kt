package com.ichat.android

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.view.View
import android.widget.FrameLayout
import android.widget.LinearLayout
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.material3.Icon
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.res.painterResource
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentActivity
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import com.ichat.android.notification.NotificationRoutes
import com.ichat.android.ui.auth.AuthFragment
import com.ichat.android.ui.chat.MessagesFragment
import com.ichat.android.ui.contacts.ContactsFragment
import com.ichat.android.ui.main.MainTab
import com.ichat.android.ui.main.MainViewModel
import com.ichat.android.ui.main.ViewModelFactory
import com.ichat.android.ui.me.MeFragment
import com.ichat.android.ui.moments.MomentsFragment
import com.ichat.android.ui.theme.IChatTheme
import kotlinx.coroutines.launch

class MainActivity : FragmentActivity() {
    private lateinit var viewModel: MainViewModel
    private var containerId: Int = View.NO_ID
    private lateinit var bottomBar: ComposeView

    private val requestNotificationPermission = registerForActivityResult(
        ActivityResultContracts.RequestPermission()
    ) { /* 用户拒绝时仍可继续使用 IM，只是不弹系统通知。 */ }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val repository = (application as IChatApplication).container.repository
        viewModel = ViewModelProvider(this, ViewModelFactory(repository))[MainViewModel::class.java]

        containerId = View.generateViewId()
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
        }
        val container = FrameLayout(this).apply {
            id = containerId
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                0,
                1f
            )
        }
        bottomBar = ComposeView(this).apply {
            setContent {
                IChatTheme {
                    BottomNavigation(viewModel)
                }
            }
        }
        root.addView(container)
        root.addView(bottomBar)
        setContentView(root)

        maybeAskNotificationPermission()
        observeMainState()
        handleIntent(intent)
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        handleIntent(intent)
    }

    private fun observeMainState() {
        lifecycleScope.launch {
            repeatOnLifecycle(Lifecycle.State.STARTED) {
                launch {
                    viewModel.currentUser.collect { user ->
                        updateBottomBarVisibility()
                        if (user == null) {
                            viewModel.setBottomNavigationVisible(true)
                            viewModel.clearActiveChatTarget()
                            showAuth()
                        } else {
                            showTab(viewModel.selectedTab.value)
                        }
                    }
                }
                launch {
                    viewModel.selectedTab.collect { tab ->
                        if (viewModel.currentUser.value != null) {
                            showTab(tab)
                        }
                    }
                }
                launch {
                    viewModel.bottomNavigationVisible.collect {
                        updateBottomBarVisibility()
                    }
                }
            }
        }
    }

    private fun updateBottomBarVisibility() {
        val visible = viewModel.currentUser.value != null && viewModel.bottomNavigationVisible.value
        bottomBar.visibility = if (visible) View.VISIBLE else View.GONE
    }

    private fun showTab(tab: MainTab) {
        if (isShowingTab(tab)) return
        val fragment: Fragment = when (tab) {
            MainTab.Messages -> MessagesFragment()
            MainTab.Contacts -> ContactsFragment()
            MainTab.Moments -> MomentsFragment()
            MainTab.Me -> MeFragment()
        }
        showFragment(fragment)
    }

    private fun showAuth() {
        if (supportFragmentManager.findFragmentById(containerId) is AuthFragment) return
        showFragment(AuthFragment())
    }

    private fun isShowingTab(tab: MainTab): Boolean {
        val current = supportFragmentManager.findFragmentById(containerId) ?: return false
        return when (tab) {
            MainTab.Messages -> current is MessagesFragment
            MainTab.Contacts -> current is ContactsFragment
            MainTab.Moments -> current is MomentsFragment
            MainTab.Me -> current is MeFragment
        }
    }

    private fun showFragment(fragment: Fragment) {
        supportFragmentManager.beginTransaction()
            .replace(containerId, fragment)
            .commitAllowingStateLoss()
    }

    private fun handleIntent(intent: Intent?) {
        val peerId = intent?.getStringExtra(NotificationRoutes.ExtraPeerId).orEmpty()
        if (peerId.isBlank()) return
        viewModel.openChatFromNotification(
            peerId = peerId,
            chatType = intent?.getStringExtra(NotificationRoutes.ExtraChatType).orEmpty(),
            title = intent?.getStringExtra(NotificationRoutes.ExtraTitle).orEmpty()
        )
    }

    private fun maybeAskNotificationPermission() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) return
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.POST_NOTIFICATIONS) == PackageManager.PERMISSION_GRANTED) {
            return
        }
        requestNotificationPermission.launch(Manifest.permission.POST_NOTIFICATIONS)
    }
}

@Composable
private fun BottomNavigation(viewModel: MainViewModel) {
    val selected by viewModel.selectedTab.collectAsState()
    NavigationBar(modifier = Modifier.fillMaxWidth()) {
        MainTab.entries.forEach { tab ->
            NavigationBarItem(
                selected = selected == tab,
                onClick = { viewModel.selectTab(tab) },
                label = { Text(tab.title) },
                icon = {
                    Icon(
                        painter = painterResource(tab.iconRes()),
                        contentDescription = tab.title
                    )
                },
                alwaysShowLabel = true
            )
        }
    }
}

// 底部导航统一使用图片资源，避免图标位置退化成文字首字。
private fun MainTab.iconRes(): Int = when (this) {
    MainTab.Messages -> R.drawable.ic_tab_messages
    MainTab.Contacts -> R.drawable.ic_tab_contacts
    MainTab.Moments -> R.drawable.ic_tab_moments
    MainTab.Me -> R.drawable.ic_tab_me
}
