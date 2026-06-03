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
import java.io.IOException
import java.net.InetSocketAddress
import java.net.Socket
import java.util.concurrent.atomic.AtomicBoolean

/**
 * 进程级 TCP 长连接。它不绑定 Activity/Fragment 生命周期，所以应用切到后台后，
 * 只要进程仍存活，读循环仍可接收聊天消息并触发系统通知。
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
            try {
                val next = Socket()
                next.keepAlive = true
                next.tcpNoDelay = true
                next.connect(InetSocketAddress(host, port), 5_000)
                val nextSerial = ++connectionSerial
                socket = next
                _state.value = ConnectionState.Connected
                startReader(next, nextSerial)
                startHeartbeat()
            } catch (error: IOException) {
                disconnectInternal()
                _state.value = ConnectionState.Disconnected
                throw IOException("网络连接失败，请检查服务器地址或网络后重试", error)
            } catch (error: Throwable) {
                disconnectInternal()
                _state.value = ConnectionState.Disconnected
                throw error
            }
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
                val current = socket
                if (current == null || current.isClosed || current.isOutputShutdown || !current.isConnected) {
                    disconnectInternal()
                    _state.value = ConnectionState.Disconnected
                    throw IOException("网络连接已断开，请稍后重试")
                }

                val timedOut = AtomicBoolean(false)
                val watchdog = scope.launch(Dispatchers.IO) {
                    delay(WriteTimeoutMillis)
                    timedOut.set(true)
                    runCatching { current.close() }
                }
                try {
                    val output = current.getOutputStream()
                    output.write(data)
                    output.flush()
                } catch (error: IOException) {
                    disconnectInternal()
                    _state.value = ConnectionState.Disconnected
                    if (timedOut.get()) {
                        throw IOException("网络发送超时，连接已重置，请重试", error)
                    }
                    throw IOException("网络发送失败，请检查网络后重试", error)
                } finally {
                    watchdog.cancel()
                }
                if (timedOut.get()) {
                    disconnectInternal()
                    _state.value = ConnectionState.Disconnected
                    throw IOException("网络发送超时，连接已重置，请重试")
                }
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
                // 读取异常通常意味着对端断开或网络切换，交给上层重新登录/重连提示。
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

    private companion object {
        const val WriteTimeoutMillis = 15_000L
    }
}
