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
#include <array>
#include <cstring>

namespace im {

namespace {

std::array<char, 6> copy_header(const boost::asio::streambuf& buf) {
    std::array<char, 6> header{};
    boost::asio::buffer_copy(boost::asio::buffer(header), buf.data(), header.size());
    return header;
}

uint16_t read_u16_be(const char* data) {
    return static_cast<uint16_t>((static_cast<unsigned char>(data[0]) << 8) |
                                 static_cast<unsigned char>(data[1]));
}

uint32_t read_u32_be(const char* data) {
    return (static_cast<uint32_t>(static_cast<unsigned char>(data[0])) << 24) |
           (static_cast<uint32_t>(static_cast<unsigned char>(data[1])) << 16) |
           (static_cast<uint32_t>(static_cast<unsigned char>(data[2])) << 8) |
           static_cast<uint32_t>(static_cast<unsigned char>(data[3]));
}

} // namespace

MessagePtr Codec::decode(boost::asio::streambuf& buf, boost::system::error_code& ec) {
    ec.clear();

    std::cout << "[Codec] decode called, buffer size:" << buf.size() << std::endl;

    // 检查缓冲区数据是否 >= 6 字节（最小头部）
    if (buf.size() < 6) {
        std::cout << "[Codec] Buffer too small, need 6 bytes, got:" << buf.size() << std::endl;
        ec = boost::asio::error::would_block;  // 数据不足，需要继续接收
        return nullptr;
    }

    // streambuf 的 data() 是 buffer 序列，跨网络分包后不保证连续。
    // 先复制固定头部再解析，避免大文件分片在不同机器间传输时错读长度或正文。
    const auto header = copy_header(buf);
    const uint16_t type = read_u16_be(header.data());
    const uint32_t length = read_u32_be(header.data() + 2);

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
        boost::asio::buffer_copy(boost::asio::buffer(body.data(), body.size()), buf.data(), length);
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
    const uint16_t msg_type = static_cast<uint16_t>(type);
    const uint32_t length = static_cast<uint32_t>(body.size());
    const std::array<char, 6> header = {
        static_cast<char>((msg_type >> 8) & 0xFF),
        static_cast<char>(msg_type & 0xFF),
        static_cast<char>((length >> 24) & 0xFF),
        static_cast<char>((length >> 16) & 0xFF),
        static_cast<char>((length >> 8) & 0xFF),
        static_cast<char>(length & 0xFF),
    };

    auto header_buf = buf.prepare(header.size());
    boost::asio::buffer_copy(header_buf, boost::asio::buffer(header));
    buf.commit(header.size());

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

    const auto header = copy_header(buf);
    const uint32_t length = read_u32_be(header.data() + 2);

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
