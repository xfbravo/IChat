package com.ichat.android.ui.contacts

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
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
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
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
import androidx.compose.ui.unit.dp
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.fragment.app.viewModels
import com.ichat.android.IChatApplication
import com.ichat.android.data.db.FriendEntity
import com.ichat.android.data.db.GroupEntity
import com.ichat.android.ui.common.ContactActionDialogs
import com.ichat.android.ui.common.HomeTopBar
import com.ichat.android.ui.common.IChatAvatar
import com.ichat.android.ui.common.IChatSearchField
import com.ichat.android.ui.main.MainViewModel
import com.ichat.android.ui.main.ViewModelFactory
import com.ichat.android.ui.theme.IChatTheme
import java.nio.charset.Charset
import java.text.Collator
import java.util.Locale

class ContactsFragment : Fragment() {
    private val factory: ViewModelFactory by lazy {
        ViewModelFactory((requireActivity().application as IChatApplication).container.repository)
    }
    private val viewModel: ContactsViewModel by viewModels { factory }
    private val mainViewModel: MainViewModel by activityViewModels { factory }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        return ComposeView(requireContext()).apply {
            setContent {
                IChatTheme {
                    ContactsScreen(
                        viewModel = viewModel,
                        mainViewModel = mainViewModel,
                        onOpenFriendRequests = ::openFriendRequests
                    )
                }
            }
        }
    }

    private fun openFriendRequests() {
        val hostId = (view?.parent as? ViewGroup)?.id ?: return
        parentFragmentManager.beginTransaction()
            .replace(hostId, FriendRequestsFragment())
            .addToBackStack("friend_requests")
            .commit()
    }
}

@Composable
private fun ContactsScreen(
    viewModel: ContactsViewModel,
    mainViewModel: MainViewModel,
    onOpenFriendRequests: () -> Unit
) {
    val currentUser by viewModel.currentUser.collectAsState()
    val friends by viewModel.friends.collectAsState()
    val groups by viewModel.groups.collectAsState()
    var searchQuery by remember { mutableStateOf("") }
    var groupsExpanded by remember { mutableStateOf(false) }
    var showAddFriend by remember { mutableStateOf(false) }
    var showCreateGroup by remember { mutableStateOf(false) }
    val visibleGroups = remember(groups, searchQuery) {
        groups.filter { it.matches(searchQuery) }
            .sortedWith { left, right -> compareDisplayText(left.displayName(), right.displayName()) }
    }
    val contactSections = remember(friends, searchQuery) {
        groupedContactSections(friends.filter { it.matches(searchQuery) })
    }

    LaunchedEffect(Unit) {
        viewModel.refresh()
    }

    LazyColumn(Modifier.fillMaxSize()) {
        item {
            HomeTopBar(
                title = "联系人",
                onAddFriend = { showAddFriend = true },
                onCreateGroup = { showCreateGroup = true }
            )
            IChatSearchField(
                value = searchQuery,
                onValueChange = { searchQuery = it },
                placeholder = "搜索联系人"
            )
            NewFriendsRow(onClick = onOpenFriendRequests)
            GroupDropdownHeader(
                expanded = groupsExpanded,
                count = visibleGroups.size,
                onClick = { groupsExpanded = !groupsExpanded }
            )
        }
        if (groupsExpanded) {
            if (visibleGroups.isEmpty()) {
                item {
                    Text(
                        text = if (searchQuery.isBlank()) "暂无群聊" else "没有匹配的群聊",
                        color = Color(0xFF6B756E),
                        modifier = Modifier.padding(horizontal = 20.dp, vertical = 14.dp)
                    )
                }
            }
            items(visibleGroups, key = { it.groupId }) { group ->
                GroupRow(
                    group = group,
                    onClick = {
                        mainViewModel.openChat(group.groupId, "group", group.displayName())
                    }
                )
            }
        }
        item {
            Spacer(Modifier.height(12.dp))
            Text(
                text = "联系人",
                fontWeight = FontWeight.SemiBold,
                modifier = Modifier.padding(horizontal = 20.dp, vertical = 8.dp)
            )
        }
        if (contactSections.isEmpty()) {
            item {
                Text(
                    text = if (searchQuery.isBlank()) "暂无联系人" else "没有匹配的联系人",
                    color = Color(0xFF6B756E),
                    modifier = Modifier.padding(horizontal = 20.dp, vertical = 14.dp)
                )
            }
        }
        contactSections.forEach { section ->
            item(key = "section-${section.key}") {
                ContactSectionHeader(section)
            }
            items(section.friends, key = { it.userId }) { friend ->
                FriendRow(
                    friend = friend,
                    onClick = {
                        mainViewModel.openChat(
                            peerId = friend.userId,
                            chatType = "p2p",
                            title = friend.displayName()
                        )
                    }
                )
            }
        }
    }

    ContactActionDialogs(
        showAddFriend = showAddFriend,
        showCreateGroup = showCreateGroup,
        friends = friends,
        currentUser = currentUser,
        onDismissAddFriend = { showAddFriend = false },
        onDismissCreateGroup = { showCreateGroup = false },
        onSendFriendRequest = viewModel::sendFriendRequest,
        onCreateGroup = viewModel::createGroup
    )
}

@Composable
private fun NewFriendsRow(onClick: () -> Unit) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(onClick = onClick)
            .padding(horizontal = 20.dp, vertical = 14.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text("新的朋友", fontWeight = FontWeight.SemiBold, modifier = Modifier.weight(1f))
        Text("查看请求")
    }
    HorizontalDivider()
}

@Composable
private fun GroupDropdownHeader(expanded: Boolean, count: Int, onClick: () -> Unit) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(onClick = onClick)
            .padding(horizontal = 20.dp, vertical = 12.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text("群聊", fontWeight = FontWeight.SemiBold, modifier = Modifier.weight(1f))
        Text("$count 个")
        Spacer(Modifier.width(10.dp))
        Text(if (expanded) "▲" else "▼")
    }
    HorizontalDivider()
}

@Composable
private fun ContactSectionHeader(section: ContactSection) {
    Text(
        text = "${section.key}  ${section.friends.size} 位联系人",
        color = Color(0xFF425247),
        fontWeight = FontWeight.SemiBold,
        modifier = Modifier
            .fillMaxWidth()
            .background(Color(0xFFF1F4F1))
            .padding(horizontal = 20.dp, vertical = 8.dp)
    )
}

@Composable
private fun FriendRow(friend: FriendEntity, onClick: () -> Unit) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 6.dp)
            .clickable(onClick = onClick),
        shape = androidx.compose.foundation.shape.RoundedCornerShape(8.dp),
        colors = CardDefaults.cardColors(containerColor = Color.White),
        elevation = CardDefaults.cardElevation(defaultElevation = 1.dp)
    ) {
        Row(
            modifier = Modifier.padding(horizontal = 14.dp, vertical = 12.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            IChatAvatar(
                avatarUrl = friend.avatarUrl,
                displayName = friend.displayName(),
                size = 44.dp
            )
            Spacer(Modifier.width(12.dp))
            Column {
                Text(friend.displayName())
                Text(friend.userId)
            }
        }
    }
}

@Composable
private fun GroupRow(group: GroupEntity, onClick: () -> Unit) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 6.dp)
            .clickable(onClick = onClick),
        shape = androidx.compose.foundation.shape.RoundedCornerShape(8.dp),
        colors = CardDefaults.cardColors(containerColor = Color.White),
        elevation = CardDefaults.cardElevation(defaultElevation = 1.dp)
    ) {
        Row(
            modifier = Modifier.padding(horizontal = 14.dp, vertical = 12.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            IChatAvatar(
                avatarUrl = group.groupAvatar,
                displayName = group.displayName(),
                size = 44.dp
            )
            Spacer(Modifier.width(12.dp))
            Column {
                Text(group.displayName())
                Text("${group.memberCount} 人")
            }
        }
    }
}

private data class ContactSection(val key: String, val friends: List<FriendEntity>)

private fun FriendEntity.displayName(): String = remark.ifBlank { nickname }.ifBlank { userId }

private fun GroupEntity.displayName(): String = groupName.ifBlank { groupId }

private fun FriendEntity.matches(query: String): Boolean {
    val keyword = query.trim()
    if (keyword.isBlank()) return true
    return listOf(displayName(), userId, nickname, remark, region, signature)
        .any { it.contains(keyword, ignoreCase = true) }
}

private fun GroupEntity.matches(query: String): Boolean {
    val keyword = query.trim()
    if (keyword.isBlank()) return true
    return listOf(displayName(), groupId, ownerId)
        .any { it.contains(keyword, ignoreCase = true) }
}

private fun groupedContactSections(friends: List<FriendEntity>): List<ContactSection> {
    val grouped = friends.groupBy { contactSectionKey(it.displayName()) }
    val sectionKeys = ('A'..'Z')
        .map(Char::toString)
        .filter { grouped.containsKey(it) }
        .toMutableList()
    if (grouped.containsKey("#")) {
        sectionKeys += "#"
    }

    // 与 Qt 客户端一致：按分区展示，再用中文 Collator 排序同分区联系人。
    return sectionKeys.map { key ->
        ContactSection(
            key = key,
            friends = grouped.getValue(key).sortedWith { left, right ->
                val nameCompare = compareDisplayText(left.displayName(), right.displayName())
                if (nameCompare != 0) nameCompare else left.userId.compareTo(right.userId)
            }
        )
    }
}

private fun contactSectionKey(displayName: String): String {
    val first = displayName.trim().firstOrNull()?.uppercaseChar() ?: return "#"
    if (first in 'A'..'Z') return first.toString()
    val chineseInitial = chinesePinyinInitial(first)
    return chineseInitial.ifBlank { "#" }
}

private fun chinesePinyinInitial(character: Char): String {
    if (character.code !in 0x4E00..0x9FFF) return ""

    val encoded = runCatching {
        character.toString().toByteArray(Charset.forName("GB18030"))
    }.getOrNull() ?: return ""
    if (encoded.size < 2) return ""

    val code = ((encoded[0].toInt() and 0xFF) shl 8) or (encoded[1].toInt() and 0xFF)
    if (code < Gb2312InitialRanges.first().first || code > 0xD7F9) return ""

    var initial = 'Z'
    for (index in 0 until Gb2312InitialRanges.lastIndex) {
        if (code >= Gb2312InitialRanges[index].first && code < Gb2312InitialRanges[index + 1].first) {
            initial = Gb2312InitialRanges[index].second
            break
        }
    }
    return initial.toString()
}

private fun compareDisplayText(left: String, right: String): Int {
    val result = ChineseCollator.compare(left, right)
    return if (result != 0) result else left.compareTo(right)
}

private val ChineseCollator: Collator = Collator.getInstance(Locale.CHINA)

private val Gb2312InitialRanges = listOf(
    0xB0A1 to 'A',
    0xB0C5 to 'B',
    0xB2C1 to 'C',
    0xB4EE to 'D',
    0xB6EA to 'E',
    0xB7A2 to 'F',
    0xB8C1 to 'G',
    0xB9FE to 'H',
    0xBBF7 to 'J',
    0xBFA6 to 'K',
    0xC0AC to 'L',
    0xC2E8 to 'M',
    0xC4C3 to 'N',
    0xC5B6 to 'O',
    0xC5BE to 'P',
    0xC6DA to 'Q',
    0xC8BB to 'R',
    0xC8F6 to 'S',
    0xCBFA to 'T',
    0xCDDA to 'W',
    0xCEF4 to 'X',
    0xD1B9 to 'Y',
    0xD4D1 to 'Z'
)
