package com.ichat.android.data.network

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.net.InetSocketAddress
import java.net.Socket

/**
 * 进程级 TCP 长连接。它不绑定 Activity/Fragment 生命周期，因此用户按 Home 退出界面后，
 * 只要进程仍存活，读循环仍能收到聊天消息并触发通知。
 */
class IChatSocketClient(private val scope: CoroutineScope) {
    private val writeMutex = Mutex()
    private var socket: Socket? = null
    private var readerJob: Job? = null
    private var heartbeatJob: Job? = null
    private var host: String = ""
    private var port: Int = 0
    private var connectionSerial: Long = 0L

    private val _state = MutableStateFlow(ConnectionState.Disconnected)
    val state: StateFlow<ConnectionState> = _state

    private val _incoming = MutableSharedFlow<ImPacket>(extraBufferCapacity = 128)
    val incoming: SharedFlow<ImPacket> = _incoming

    suspend fun connect(host: String, port: Int) {
        this.host = host
        this.port = port
        withContext(Dispatchers.IO) {
            disconnectInternal()
            _state.value = ConnectionState.Connecting
            val next = Socket()
            next.keepAlive = true
            next.tcpNoDelay = true
            next.connect(InetSocketAddress(host, port), 5_000)
            val nextSerial = ++connectionSerial
            socket = next
            _state.value = ConnectionState.Connected
            startReader(next, nextSerial)
            startHeartbeat()
        }
    }

    suspend fun disconnect() {
        withContext(Dispatchers.IO) {
            disconnectInternal()
            _state.value = ConnectionState.Disconnected
        }
    }

    fun markLoggedIn() {
        _state.value = ConnectionState.LoggedIn
    }

    suspend fun send(type: MsgType, body: String) {
        withContext(Dispatchers.IO) {
            val data = ProtocolCodec.encode(type, body)
            writeMutex.withLock {
                val current = socket ?: error("Socket is not connected")
                current.getOutputStream().write(data)
                current.getOutputStream().flush()
            }
        }
    }

    suspend fun sendJson(type: MsgType, json: JSONObject) {
        send(type, json.toString())
    }

    private fun startReader(activeSocket: Socket, activeSerial: Long) {
        readerJob?.cancel()
        readerJob = scope.launch(Dispatchers.IO) {
            try {
                val input = activeSocket.getInputStream()
                while (isActive && !activeSocket.isClosed) {
                    val packet = ProtocolCodec.readPacket(input) ?: break
                    _incoming.emit(packet.copy(connectionSerial = activeSerial))
                }
            } catch (_: Throwable) {
                // 读取异常通常意味着对端断开或网络切换，交给上层做重新登录/重连提示。
            } finally {
                disconnectInternal()
                _state.value = ConnectionState.Disconnected
            }
        }
    }

    private fun startHeartbeat() {
        heartbeatJob?.cancel()
        heartbeatJob = scope.launch(Dispatchers.IO) {
            while (isActive) {
                delay(15_000)
                runCatching {
                    val heartbeat = JSONObject()
                        .put("timestamp", System.currentTimeMillis() / 1000)
                    sendJson(MsgType.HEARTBEAT, heartbeat)
                }
            }
        }
    }

    private fun disconnectInternal() {
        heartbeatJob?.cancel()
        readerJob?.cancel()
        runCatching { socket?.close() }
        socket = null
    }
}
