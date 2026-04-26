/**
 * @file codec.cpp
 * @brief 消息编解码器实现
 *
 * 处理粘包问题的核心逻辑：
 * 1. 发送端：先发送 6 字节头部（类型+长度），再发送数据
 * 2. 接收端：先读取 6 字节头部，解析出长度后再读取完整数据
 */

#include "codec.h"
#include <boost/asio/streambuf.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <boost/system/error_code.hpp>
#include <iostream>
#include <cstring>

namespace im {

MessagePtr Codec::decode(boost::asio::streambuf& buf, boost::system::error_code& ec) {
    ec.clear();

    std::cout << "[Codec] decode called, buffer size:" << buf.size() << std::endl;

    // 检查缓冲区数据是否 >= 6 字节（最小头部）
    if (buf.size() < 6) {
        std::cout << "[Codec] Buffer too small, need 6 bytes, got:" << buf.size() << std::endl;
        ec = boost::asio::error::would_block;  // 数据不足，需要继续接收
        return nullptr;
    }

    // 获取缓冲区数据
    // 使用迭代器方式访问 streambuf 中的数据
    const char* data = static_cast<const char*>(buf.data().data());

    // 解析头部（网络字节序 -> 主机字节序）
    uint16_t type = ntoh(*reinterpret_cast<const uint16_t*>(data));
    uint32_t length = ntoh(*reinterpret_cast<const uint32_t*>(data + 2));

    // 检查消息长度是否合理（防止恶意数据）
    constexpr uint32_t MAX_MESSAGE_LENGTH = 10 * 1024 * 1024;  // 10MB
    if (length > MAX_MESSAGE_LENGTH) {
        ec = boost::asio::error::message_size;
        std::cerr << "[Codec] 消息长度过大: " << length << std::endl;
        return nullptr;
    }

    // 检查缓冲区是否包含完整的消息体
    // 完整消息 = 6 字节头部 + length 字节数据
    if (buf.size() < 6 + length) {
        ec = boost::asio::error::would_block;  // 数据不足，需要继续接收
        return nullptr;
    }

    // 移除头部（6字节）
    buf.consume(6);

    // 读取消息体
    std::string body;
    if (length > 0) {
        body.resize(length);
        // 从缓冲区复制数据到 body
        const char* body_data = static_cast<const char*>(buf.data().data());
        std::memcpy(body.data(), body_data, length);
        buf.consume(length);  // 移除已读取的数据
    }

    // 构建消息
    auto msg = std::make_shared<Message>();
    msg->type = static_cast<MsgType>(type);
    msg->body = std::move(body);

    std::cout << "[Codec] 解析消息: type=0x" << std::hex << static_cast<uint16_t>(msg->type)
              << std::dec << ", length=" << length << std::endl;

    return msg;
}

void Codec::encode(MsgType type, const std::string& body, boost::asio::streambuf& buf) {
    // 预留 6 字节空间存储头部
    auto header_buf = buf.prepare(6);

    // 写入头部（主机字节序 -> 网络字节序）
    // 使用 data() 获取指针
    void* header_data = header_buf.data();
    char* data = static_cast<char*>(header_data);
    *reinterpret_cast<uint16_t*>(data) = hton(static_cast<uint16_t>(type));
    *reinterpret_cast<uint32_t*>(data + 2) = hton(static_cast<uint32_t>(body.size()));

    // 确认写入头部
    buf.commit(6);

    // 写入消息体
    if (!body.empty()) {
        auto body_buf = buf.prepare(body.size());
        void* body_data_ptr = body_buf.data();
        std::memcpy(body_data_ptr, body.data(), body.size());
        buf.commit(body.size());
    }
}

void Codec::encode(const Message& msg, boost::asio::streambuf& buf) {
    encode(msg.type, msg.body, buf);
}

bool Codec::hasCompleteMessage(const boost::asio::streambuf& buf) const {
    if (buf.size() < 6) {
        return false;
    }

    const char* data = static_cast<const char*>(buf.data().data());
    uint32_t length = ntoh(*reinterpret_cast<const uint32_t*>(data + 2));

    return buf.size() >= 6 + length;
}

// ==================== 字节序转换 ====================

uint16_t Codec::hton(uint16_t v) {
#if defined(__BYTE_ORDER) && __BYTE_ORDER == __ORDER_LITTLE_ENDIAN__
    return ((v & 0x00FF) << 8) | ((v & 0xFF00) >> 8);
#else
    return v;
#endif
}

uint32_t Codec::hton(uint32_t v) {
#if defined(__BYTE_ORDER) && __BYTE_ORDER == __ORDER_LITTLE_ENDIAN__
    return ((v & 0x000000FF) << 24) |
           ((v & 0x0000FF00) << 8)  |
           ((v & 0x00FF0000) >> 8)  |
           ((v & 0xFF000000) >> 24);
#else
    return v;
#endif
}

uint16_t Codec::ntoh(uint16_t v) {
    return hton(v);  // 对称操作
}

uint32_t Codec::ntoh(uint32_t v) {
    return hton(v);
}

} // namespace im
