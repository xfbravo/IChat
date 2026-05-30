package com.ichat.android.ui.me

import android.graphics.BitmapFactory
import android.net.Uri
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
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
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.fragment.app.viewModels
import com.ichat.android.IChatApplication
import com.ichat.android.data.model.CurrentUser
import com.ichat.android.ui.common.BackIconButton
import com.ichat.android.ui.common.IChatAvatar
import com.ichat.android.ui.main.MainViewModel
import com.ichat.android.ui.main.ViewModelFactory
import com.ichat.android.ui.theme.IChatTheme

class MyProfileFragment : Fragment() {
    private val factory: ViewModelFactory by lazy {
        ViewModelFactory((requireActivity().application as IChatApplication).container.repository)
    }
    private val viewModel: MeViewModel by viewModels { factory }
    private val mainViewModel: MainViewModel by activityViewModels { factory }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        return ComposeView(requireContext()).apply {
            setContent {
                IChatTheme {
                    MyProfileScreen(
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
private fun MyProfileScreen(
    viewModel: MeViewModel,
    mainViewModel: MainViewModel,
    onBack: () -> Unit
) {
    val user by viewModel.currentUser.collectAsState()
    val profileStatus by viewModel.profileStatus.collectAsState()
    val localStatus by viewModel.status.collectAsState()
    var avatarUri by remember { mutableStateOf<Uri?>(null) }
    var nickname by remember(user?.userId, user?.nickname) { mutableStateOf(user?.nickname.orEmpty()) }
    var gender by remember(user?.userId, user?.gender) {
        mutableStateOf(user?.gender?.takeIf { it == "男" || it == "女" } ?: "男")
    }
    var region by remember(user?.userId, user?.region) { mutableStateOf(user?.region.orEmpty()) }
    var signature by remember(user?.userId, user?.signature) { mutableStateOf(user?.signature.orEmpty()) }
    val avatarPicker = rememberLauncherForActivityResult(ActivityResultContracts.GetContent()) { uri ->
        if (uri != null) {
            avatarUri = uri
        }
    }

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
        MyProfileTopBar(
            onBack = onBack,
            onSave = {
                viewModel.saveProfile(
                    nickname = nickname,
                    gender = gender,
                    region = region,
                    signature = signature,
                    avatarUri = avatarUri
                )
                avatarUri = null
            }
        )
        MyProfileForm(
            user = user,
            avatarUri = avatarUri,
            nickname = nickname,
            gender = gender,
            region = region,
            signature = signature,
            status = profileStatus.ifBlank { localStatus },
            onPickAvatar = { avatarPicker.launch("image/*") },
            onNicknameChange = { nickname = it },
            onGenderChange = { gender = it },
            onRegionChange = { region = it },
            onSignatureChange = { signature = it }
        )
    }
}

@Composable
private fun MyProfileTopBar(onBack: () -> Unit, onSave: () -> Unit) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(top = 32.dp, bottom = 8.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        BackIconButton(onClick = onBack)
        Text(
            text = "个人信息",
            fontWeight = FontWeight.Bold,
            fontSize = 20.sp,
            modifier = Modifier.weight(1f)
        )
        TextButton(onClick = onSave, modifier = Modifier.width(72.dp)) {
            Text("Save")
        }
    }
    HorizontalDivider(color = Color(0xFFDDE6DF))
}

@Composable
private fun MyProfileForm(
    user: CurrentUser?,
    avatarUri: Uri?,
    nickname: String,
    gender: String,
    region: String,
    signature: String,
    status: String,
    onPickAvatar: () -> Unit,
    onNicknameChange: (String) -> Unit,
    onGenderChange: (String) -> Unit,
    onRegionChange: (String) -> Unit,
    onSignatureChange: (String) -> Unit
) {
    Column(
        modifier = Modifier.padding(horizontal = 20.dp, vertical = 18.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp)
    ) {
        Surface(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(8.dp),
            color = Color.White,
            tonalElevation = 1.dp
        ) {
            Row(
                modifier = Modifier.padding(18.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                EditableAvatar(
                    avatarUrl = user?.avatarUrl.orEmpty(),
                    displayName = nickname.ifBlank { user?.userId.orEmpty() },
                    pickedUri = avatarUri,
                    onClick = onPickAvatar
                )
                Spacer(Modifier.width(16.dp))
                Column(modifier = Modifier.weight(1f)) {
                    Text(nickname.ifBlank { user?.userId.orEmpty() }, fontWeight = FontWeight.SemiBold, fontSize = 19.sp)
                    Spacer(Modifier.height(5.dp))
                    Text("账号：${user?.userId.orEmpty()}", color = Color(0xFF5E6A61))
                    Text("点击头像更换", color = Color(0xFF2F6F3E), fontSize = 13.sp)
                }
            }
        }

        OutlinedTextField(
            value = nickname,
            onValueChange = onNicknameChange,
            label = { Text("昵称") },
            singleLine = true,
            modifier = Modifier.fillMaxWidth()
        )
        GenderSelector(value = gender, onChange = onGenderChange)
        OutlinedTextField(
            value = region,
            onValueChange = onRegionChange,
            label = { Text("地区") },
            singleLine = true,
            modifier = Modifier.fillMaxWidth()
        )
        OutlinedTextField(
            value = signature,
            onValueChange = onSignatureChange,
            label = { Text("签名") },
            minLines = 2,
            maxLines = 4,
            modifier = Modifier.fillMaxWidth()
        )
        Button(onClick = onPickAvatar, modifier = Modifier.fillMaxWidth()) {
            Text(if (avatarUri == null) "更换头像" else "重新选择头像")
        }
        if (status.isNotBlank()) {
            Text(status, color = Color(0xFF5E6A61))
        }
    }
}

@Composable
private fun EditableAvatar(
    avatarUrl: String,
    displayName: String,
    pickedUri: Uri?,
    onClick: () -> Unit
) {
    val context = LocalContext.current
    val preview = remember(pickedUri) {
        pickedUri?.let { decodeUriBitmap(context, it) }
    }
    Box(
        modifier = Modifier
            .size(88.dp)
            .clip(CircleShape)
            .background(Color(0xFFE4ECE6))
            .clickable(onClick = onClick),
        contentAlignment = Alignment.Center
    ) {
        if (preview != null) {
            Image(
                bitmap = preview,
                contentDescription = "已选择头像",
                modifier = Modifier.fillMaxSize(),
                contentScale = ContentScale.Crop
            )
        } else {
            IChatAvatar(
                avatarUrl = avatarUrl,
                displayName = displayName,
                size = 88.dp
            )
        }
    }
}

@Composable
private fun GenderSelector(value: String, onChange: (String) -> Unit) {
    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Text("性别", color = Color(0xFF5E6A61), fontSize = 13.sp)
        Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            listOf("男", "女").forEach { option ->
                if (value == option) {
                    Button(onClick = { onChange(option) }) {
                        Text(option)
                    }
                } else {
                    OutlinedButton(onClick = { onChange(option) }) {
                        Text(option)
                    }
                }
            }
        }
    }
}

private fun decodeUriBitmap(context: android.content.Context, uri: Uri): ImageBitmap? {
    return runCatching {
        context.contentResolver.openInputStream(uri)?.use { input ->
            BitmapFactory.decodeStream(input)
        }?.asImageBitmap()
    }.getOrNull()
}
