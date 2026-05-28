package com.ichat.android.data.network

import java.io.EOFException
import java.io.InputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.UUID

data class ImPacket(
    val type: MsgType,
    val body: String
)

/**
 * 服务端协议包头固定为 6 字节：2 字节 type + 4 字节 body length，均使用大端序。
 * 这里不引入额外版本字段，避免破坏现有 Windows 客户端和服务端约定。
 */
object ProtocolCodec {
    private const val HeaderSize = 6

    fun encode(type: MsgType, body: String): ByteArray {
        val bodyBytes = body.toByteArray(Charsets.UTF_8)
        val buffer = ByteBuffer.allocate(HeaderSize + bodyBytes.size).order(ByteOrder.BIG_ENDIAN)
        buffer.putShort(type.value.toShort())
        buffer.putInt(bodyBytes.size)
        buffer.put(bodyBytes)
        return buffer.array()
    }

    fun generateMsgId(): String = UUID.randomUUID().toString().replace("-", "")

    fun readPacket(input: InputStream): ImPacket? {
        val header = readExact(input, HeaderSize) ?: return null
        val headerBuffer = ByteBuffer.wrap(header).order(ByteOrder.BIG_ENDIAN)
        val typeValue = headerBuffer.short.toInt() and 0xFFFF
        val length = headerBuffer.int
        if (length < 0) {
            throw IllegalArgumentException("Invalid packet length: $length")
        }

        val bodyBytes = readExact(input, length) ?: throw EOFException("Body ended early")
        val type = MsgType.fromValue(typeValue)
            ?: throw IllegalArgumentException("Unknown message type: 0x${typeValue.toString(16)}")
        return ImPacket(type, bodyBytes.toString(Charsets.UTF_8))
    }

    private fun readExact(input: InputStream, size: Int): ByteArray? {
        val data = ByteArray(size)
        var offset = 0
        while (offset < size) {
            val read = input.read(data, offset, size - offset)
            if (read < 0) {
                return if (offset == 0) null else throw EOFException("Stream ended early")
            }
            offset += read
        }
        return data
    }
}
