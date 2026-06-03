package com.ichat.android.ui.me

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.fragment.app.viewModels
import com.ichat.android.IChatApplication
import com.ichat.android.ui.common.BackIconButton
import com.ichat.android.ui.main.MainViewModel
import com.ichat.android.ui.main.ViewModelFactory
import com.ichat.android.ui.theme.IChatTheme

class SettingsFragment : Fragment() {
    private val factory: ViewModelFactory by lazy {
        ViewModelFactory((requireActivity().application as IChatApplication).container.repository)
    }
    private val viewModel: MeViewModel by viewModels { factory }
    private val mainViewModel: MainViewModel by activityViewModels { factory }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        return ComposeView(requireContext()).apply {
            setContent {
                IChatTheme {
                    SettingsScreen(
                        mainViewModel = mainViewModel,
                        onBack = { parentFragmentManager.popBackStack() },
                        onOpenAccountSecurity = ::openAccountSecurity,
                        onOpenProfileManagement = ::openProfileManagement,
                        onLogout = viewModel::logout
                    )
                }
            }
        }
    }

    private fun openAccountSecurity() {
        val hostId = (view?.parent as? ViewGroup)?.id ?: return
        parentFragmentManager.beginTransaction()
            .replace(hostId, AccountSecurityFragment())
            .addToBackStack("account_security")
            .commit()
    }

    private fun openProfileManagement() {
        val hostId = (view?.parent as? ViewGroup)?.id ?: return
        parentFragmentManager.beginTransaction()
            .replace(hostId, MyProfileFragment())
            .addToBackStack("profile_management")
            .commit()
    }
}

@Composable
private fun SettingsScreen(
    mainViewModel: MainViewModel,
    onBack: () -> Unit,
    onOpenAccountSecurity: () -> Unit,
    onOpenProfileManagement: () -> Unit,
    onLogout: () -> Unit
) {
    LaunchedEffect(Unit) {
        mainViewModel.setBottomNavigationVisible(false)
    }
    DisposableEffect(Unit) {
        onDispose { mainViewModel.setBottomNavigationVisible(true) }
    }
    BackHandler(onBack = onBack)

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(Color(0xFFF5F7F5))
    ) {
        SettingsTopBar(onBack = onBack)
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(horizontal = 20.dp, vertical = 18.dp)
        ) {
            SettingsItemCard(
                title = "账号安全",
                subtitle = "修改登录密码",
                onClick = onOpenAccountSecurity
            )
            Spacer(Modifier.height(12.dp))
            SettingsItemCard(
                title = "个人资料管理",
                subtitle = "头像、昵称、地区和签名",
                onClick = onOpenProfileManagement
            )
            Spacer(Modifier.weight(1f))
            Button(
                onClick = onLogout,
                modifier = Modifier.fillMaxWidth(),
                colors = ButtonDefaults.buttonColors(
                    containerColor = Color(0xFFD93025),
                    contentColor = Color.White
                )
            ) {
                Text("退出登录")
            }
        }
    }
}

@Composable
private fun SettingsTopBar(onBack: () -> Unit) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(top = 32.dp, bottom = 8.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        BackIconButton(onClick = onBack)
        Text(
            text = "设置",
            fontWeight = FontWeight.Bold,
            fontSize = 20.sp,
            modifier = Modifier.weight(1f)
        )
        Spacer(Modifier.width(72.dp))
    }
    HorizontalDivider(color = Color(0xFFDDE6DF))
}

@Composable
private fun SettingsItemCard(title: String, subtitle: String, onClick: () -> Unit) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(onClick = onClick),
        shape = androidx.compose.foundation.shape.RoundedCornerShape(8.dp),
        colors = CardDefaults.cardColors(containerColor = Color.White),
        elevation = CardDefaults.cardElevation(defaultElevation = 1.dp)
    ) {
        Column(modifier = Modifier.padding(horizontal = 18.dp, vertical = 16.dp)) {
            Text(title, fontWeight = FontWeight.SemiBold)
            Spacer(Modifier.height(4.dp))
            Text(subtitle, color = Color(0xFF5E6A61))
        }
    }
}
