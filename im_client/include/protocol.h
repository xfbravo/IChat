/**
 * @file protocol.h
 * @brief 消息协议定义（与服务器保持一致）
 */

#pragma once

#include <cstdint>
#include <string>
#include <QByteArray>
#include <QString>

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
    REGISTER_RSP  = 0x8003,  // 注册响应
    ERROR         = 0x800F,  // 错误响应
};

/**
 * @brief 消息结构
 */
struct Message {
    MsgType type;
    std::string body;
};

/**
 * @brief 登录请求
 */
struct LoginRequest {
    std::string user_id;
    std::string password;
};

/**
 * @brief 登录响应
 */
struct LoginResponse {
    int code;
    std::string message;
    std::string user_id;
    std::string nickname;
    std::string avatar_url;
    std::string token;
};

/**
 * @brief 聊天消息
 */
struct ChatMessage {
    std::string msg_id;
    std::string from_user_id;
    std::string to_user_id;
    std::string content_type;  // text/image/voice/file
    std::string content;
    int64_t client_time;
};

/**
 * @brief 协议工具类
 */
class Protocol {
public:
    /**
     * @brief 编码消息（添加包头）
     * @param type 消息类型
     * @param body 消息体
     * @return 编码后的字节数据
     */
    static QByteArray encode(MsgType type, const std::string& body);

    /**
     * @brief 编码消息（QString版本）
     */
    static QByteArray encode(MsgType type, const QString& body);

    /**
     * @brief 解码消息（会从data中移除已解码的数据）
     * @param data 原始数据（会被修改）
     * @param type 输出：消息类型
     * @param body 输出：消息体
     * @return true 解码成功
     */
    static bool decode(QByteArray& data, MsgType& type, QString& body);

    /**
     * @brief 生成唯一消息ID
     */
    static QString generateMsgId();

    /**
     * @brief 创建登录请求JSON
     */
    static QString makeLoginRequest(const QString& user_id, const QString& password);

    /**
     * @brief 创建聊天消息JSON
     */
    static QString makeChatMessage(const QString& from, const QString& to,
                                   const QString& content_type, const QString& content);

    /**
     * @brief 解析登录响应
     */
    static bool parseLoginResponse(const QString& body, LoginResponse& rsp);
};
