package com.ichat.android.ui.me

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material3.Button
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.PasswordVisualTransformation
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

class AccountSecurityFragment : Fragment() {
    private val factory: ViewModelFactory by lazy {
        ViewModelFactory((requireActivity().application as IChatApplication).container.repository)
    }
    private val viewModel: MeViewModel by viewModels { factory }
    private val mainViewModel: MainViewModel by activityViewModels { factory }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        return ComposeView(requireContext()).apply {
            setContent {
                IChatTheme {
                    AccountSecurityScreen(
                        viewModel = viewModel,
                        mainViewModel = mainViewModel,
                        onBack = { parentFragmentManager.popBackStack() }
                    )
                }
            }
        }
    }
}

@Composable
private fun AccountSecurityScreen(
    viewModel: MeViewModel,
    mainViewModel: MainViewModel,
    onBack: () -> Unit
) {
    val status by viewModel.status.collectAsState()
    var oldPassword by remember { mutableStateOf("") }
    var newPassword by remember { mutableStateOf("") }

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
        AccountSecurityTopBar(onBack = onBack)
        Column(modifier = Modifier.padding(horizontal = 20.dp, vertical = 18.dp)) {
            Text("修改密码", fontWeight = FontWeight.SemiBold)
            Spacer(Modifier.height(12.dp))
            OutlinedTextField(
                value = oldPassword,
                onValueChange = { oldPassword = it },
                label = { Text("旧密码") },
                visualTransformation = PasswordVisualTransformation(),
                modifier = Modifier.fillMaxWidth()
            )
            Spacer(Modifier.height(10.dp))
            OutlinedTextField(
                value = newPassword,
                onValueChange = { newPassword = it },
                label = { Text("新密码") },
                visualTransformation = PasswordVisualTransformation(),
                modifier = Modifier.fillMaxWidth()
            )
            Spacer(Modifier.height(12.dp))
            Button(
                onClick = { viewModel.changePassword(oldPassword, newPassword) },
                modifier = Modifier.fillMaxWidth()
            ) {
                Text("保存密码")
            }
            if (status.isNotBlank()) {
                Spacer(Modifier.height(10.dp))
                Text(status, color = Color(0xFF5E6A61))
            }
        }
    }
}

@Composable
private fun AccountSecurityTopBar(onBack: () -> Unit) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(top = 32.dp, bottom = 8.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        BackIconButton(onClick = onBack)
        Text(
            text = "账号安全",
            fontWeight = FontWeight.Bold,
            fontSize = 20.sp,
            modifier = Modifier.weight(1f)
        )
        Spacer(Modifier.width(72.dp))
    }
    HorizontalDivider(color = Color(0xFFDDE6DF))
}
