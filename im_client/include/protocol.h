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
    HEARTBEAT        = 0x0001,  // 心跳包
    LOGIN            = 0x0002,  // 登录请求
    REGISTER_REQ     = 0x0003,  // 注册请求
    LOGOUT           = 0x0004,  // 登出请求
    CHAT_MESSAGE     = 0x0005,  // 统一聊天消息，content_type 区分 text/image/file/voice
    TEXT             = 0x0005,  // 兼容旧名称
    IMAGE            = 0x0006,  // 兼容旧媒体消息类型，请改用 CHAT_MESSAGE + content_type=image
    FILE             = 0x0007,  // 兼容旧媒体消息类型，请改用 CHAT_MESSAGE + content_type=file
    VOICE            = 0x0008,  // 兼容旧媒体消息类型，请改用 CHAT_MESSAGE + content_type=voice
    ACK              = 0x0009,  // 消息确认
    FRIEND_REQUEST   = 0x000A,  // 发送好友请求
    GET_FRIEND_LIST   = 0x000B,  // 获取好友列表
    GET_FRIEND_REQUESTS = 0x000C, // 获取好友请求列表
    FRIEND_REQUEST_RSP = 0x000D, // 响应好友请求（同意/拒绝）
    DELETE_FRIEND     = 0x000E,  // 删除好友
    GET_CHAT_HISTORY = 0x000F,  // 获取聊天记录
    GET_OFFLINE_MESSAGES = 0x0010, // 获取离线消息
    OFFLINE_MESSAGE_ACK = 0x0011,  // 离线消息确认
    UPDATE_FRIEND_REMARK = 0x0012, // 修改好友备注
    UPDATE_AVATAR     = 0x0013,  // 更新头像
    CHANGE_PASSWORD   = 0x0014,  // 修改密码
    UPDATE_PROFILE    = 0x0015,  // 更新个人信息

    // 服务端 -> 客户端
    LOGIN_RSP        = 0x8002,  // 登录响应
    REGISTER_RSP     = 0x8003,  // 注册响应
    ERROR            = 0x800F,  // 错误响应
    FRIEND_LIST_RSP  = 0x800A,  // 好友列表响应
    FRIEND_REQUEST_NEW = 0x800B, // 新好友请求通知
    FRIEND_LIST_UPDATE = 0x800C, // 好友列表更新通知
    CHAT_HISTORY_RSP = 0x8010,  // 聊天记录响应
    OFFLINE_MESSAGE   = 0x8011,  // 离线消息推送
    UPDATE_FRIEND_REMARK_RSP = 0x8012, // 修改好友备注响应
    UPDATE_AVATAR_RSP = 0x8013, // 更新头像响应
    CHANGE_PASSWORD_RSP = 0x8014, // 修改密码响应
    UPDATE_PROFILE_RSP = 0x8015, // 更新个人信息响应
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
    std::string gender;
    std::string region;
    std::string signature;
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
    static QString makeChatMessage(const QString& msg_id, const QString& from, const QString& to,
                                   const QString& content_type, const QString& content);

    /**
     * @brief 解析登录响应
     */
    static bool parseLoginResponse(const QString& body, LoginResponse& rsp);
};
