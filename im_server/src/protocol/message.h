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
    HEARTBEAT           = 0x0001,  // 心跳包
    LOGIN               = 0x0002,  // 登录请求
    REGISTER_REQ        = 0x0003,  // 注册请求
    LOGOUT              = 0x0004,  // 登出请求
    CHAT_MESSAGE        = 0x0005,  // 统一聊天消息，content_type 区分 text/image/file/voice
    TEXT                = 0x0005,  // 兼容旧名称
    IMAGE               = 0x0006,  // 兼容旧媒体消息类型，请改用 CHAT_MESSAGE + content_type=image
    FILE                = 0x0007,  // 兼容旧媒体消息类型，请改用 CHAT_MESSAGE + content_type=file
    VOICE               = 0x0008,  // 兼容旧媒体消息类型，请改用 CHAT_MESSAGE + content_type=voice
    ACK                 = 0x0009,  // 消息确认
    FRIEND_REQUEST      = 0x000A,  // 发送好友请求
    GET_FRIEND_LIST     = 0x000B,  // 获取好友列表
    GET_FRIEND_REQUESTS = 0x000C,  // 获取好友请求列表
    FRIEND_REQUEST_RSP  = 0x000D,  // 响应好友请求（同意/拒绝）
    DELETE_FRIEND       = 0x000E,  // 删除好友
    GET_CHAT_HISTORY    = 0x000F,  // 获取聊天记录
    GET_OFFLINE_MESSAGES = 0x0010, // 获取离线消息
    OFFLINE_MESSAGE_ACK = 0x0011,  // 离线消息确认

    // 服务端 -> 客户端
    LOGIN_RSP           = 0x8002,  // 登录响应
    REGISTER_RSP        = 0x8003,  // 注册响应
    ERROR               = 0x800F,  // 错误响应
    FRIEND_LIST_RSP     = 0x800A,  // 好友列表响应
    FRIEND_REQUEST_NEW  = 0x800B,  // 新好友请求通知
    FRIEND_LIST_UPDATE  = 0x800C,  // 好友列表更新通知
    CHAT_HISTORY_RSP   = 0x8010,  // 聊天记录响应
    OFFLINE_MESSAGE     = 0x8011,  // 离线消息推送
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
