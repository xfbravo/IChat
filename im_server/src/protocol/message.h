/**
 * @file message.h
 * @brief 消息协议定义
 *
 * 消息格式（解决粘包问题）：
 * +------------------+------------------+------------------+
 * |  消息类型 (2B)   |   消息长度 (4B)   |   消息内容 (N)   |
 * +------------------+------------------+------------------+
 *      uint16_t           uint32_t          bytes[N]
 *
 * 头部固定 6 字节：2字节类型 + 4字节长度（大端序）
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace im {

/**
 * @brief 消息类型枚举
 */
enum class MsgType : uint16_t {
    // 客户端 -> 服务端
    HEARTBEAT     = 0x0001,  // 心跳包
    LOGIN         = 0x0002,  // 登录请求
    REGISTER_REQ  = 0x0003,  // 注册请求
    LOGOUT        = 0x0004,  // 登出请求
    TEXT          = 0x0005,  // 文本消息
    IMAGE         = 0x0006,  // 图片消息
    FILE          = 0x0007,  // 文件消息
    VOICE         = 0x0008,  // 语音消息
    ACK           = 0x0009,  // 消息确认

    // 服务端 -> 客户端
    LOGIN_RSP     = 0x8002,  // 登录响应
    REGISTER_RSP   = 0x8003,  // 注册响应
    ERROR         = 0x800F,  // 错误响应
};

/**
 * @brief 消息头结构（网络字节序，大端序）
 *
 * 协议头部固定 6 字节：
 * - type: 2字节，标识消息类型
 * - length: 4字节，标识消息体长度（不包含头部）
 */
struct MessageHeader {
    uint16_t type;     // 消息类型
    uint32_t length;   // 消息体长度（大端序）

    MessageHeader() : type(0), length(0) {}
    MessageHeader(MsgType t, uint32_t len) : type(static_cast<uint16_t>(t)), length(len) {}
};

/**
 * @brief 消息结构
 */
struct Message {
    MsgType type;           // 消息类型
    std::string body;       // 消息体（JSON 格式）

    Message() : type(MsgType::HEARTBEAT), body() {}
    Message(MsgType t, const std::string& b) : type(t), body(b) {}
};

/**
 * @brief 消息指针类型
 */
using MessagePtr = std::shared_ptr<Message>;

/**
 * @brief 登录消息体
 */
struct LoginReq {
    std::string user_id;
    std::string token;
};

/**
 * @brief 登录响应
 */
struct LoginRsp {
    uint32_t code;         // 0: 成功，非0: 失败
    std::string message;
    std::string user_id;
};

/**
 * @brief 聊天消息体（文本/图片/文件/语音通用）
 */
struct ChatMsg {
    std::string msg_id;        // 消息唯一ID
    std::string from_user_id; // 发送者用户ID
    std::string to_user_id;   // 接收者用户ID（个人或群ID）
    uint32_t msg_type;        // 消息类型（1:文本, 2:图片, 3:文件, 4:语音）
    std::string content;      // 消息内容或文件路径/URL
    uint64_t timestamp;       // 时间戳（秒）
};

/**
 * @brief 心跳消息
 */
struct HeartbeatMsg {
    std::string user_id;
    uint64_t timestamp;
};

} // namespace im
