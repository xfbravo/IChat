package com.ichat.android.ui.me

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import androidx.fragment.app.Fragment
import androidx.fragment.app.viewModels
import com.ichat.android.IChatApplication
import com.ichat.android.ui.main.ViewModelFactory
import com.ichat.android.ui.theme.IChatTheme

class MeFragment : Fragment() {
    private val viewModel: MeViewModel by viewModels {
        ViewModelFactory((requireActivity().application as IChatApplication).container.repository)
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        return ComposeView(requireContext()).apply {
            setContent {
                IChatTheme {
                    MeScreen(viewModel)
                }
            }
        }
    }
}

@Composable
private fun MeScreen(viewModel: MeViewModel) {
    val user by viewModel.currentUser.collectAsState()
    val status by viewModel.status.collectAsState()
    var oldPassword by remember { mutableStateOf("") }
    var newPassword by remember { mutableStateOf("") }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(20.dp)
    ) {
        Text("我", fontWeight = FontWeight.Bold)
        Spacer(Modifier.height(20.dp))
        Text("账号：${user?.userId.orEmpty()}")
        Text("昵称：${user?.nickname.orEmpty()}")
        Text("地区：${user?.region.orEmpty()}")
        Text("签名：${user?.signature.orEmpty()}")

        Spacer(Modifier.height(28.dp))
        Text("修改密码", fontWeight = FontWeight.SemiBold)
        Spacer(Modifier.height(10.dp))
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
        Button(onClick = { viewModel.changePassword(oldPassword, newPassword) }) {
            Text("保存密码")
        }
        if (status.isNotBlank()) {
            Spacer(Modifier.height(10.dp))
            Text(status)
        }

        Spacer(Modifier.height(28.dp))
        Button(onClick = viewModel::logout) {
            Text("退出登录")
        }
    }
}
