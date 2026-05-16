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
    CHAT_MESSAGE     = 0x0005,  // 统一聊天消息，content_type 区分 text/image/video/file/voice
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
    GET_USER_PROFILE  = 0x0016,  // 获取用户个人信息
    CREATE_MOMENT     = 0x0017,  // 发布朋友圈
    GET_MOMENTS       = 0x0018,  // 获取朋友圈时间流
    FILE_UPLOAD_START = 0x0019,  // 开始文件上传
    FILE_UPLOAD_CHUNK = 0x001A,  // 上传文件分片
    FILE_DOWNLOAD_REQ = 0x001B,  // 下载文件请求
    CREATE_GROUP      = 0x001C,  // 创建群聊
    GET_GROUP_LIST    = 0x001D,  // 获取群聊列表
    CALL_INVITE       = 0x001E,  // 发起一对一音视频通话
    CALL_ACCEPT       = 0x001F,  // 接听通话
    CALL_REJECT       = 0x0020,  // 拒绝通话
    CALL_CANCEL       = 0x0021,  // 取消呼叫
    CALL_HANGUP       = 0x0022,  // 挂断通话
    CALL_ICE          = 0x0023,  // WebRTC ICE 候选
    CALL_TIMEOUT      = 0x0024,  // 呼叫超时

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
    USER_PROFILE_RSP = 0x8016, // 用户个人信息响应
    CREATE_MOMENT_RSP = 0x8017, // 发布朋友圈响应
    MOMENTS_RSP       = 0x8018, // 朋友圈时间流响应
    FILE_UPLOAD_RSP   = 0x8019, // 文件上传响应
    FILE_DOWNLOAD_RSP = 0x801A, // 文件下载响应
    FILE_DOWNLOAD_CHUNK = 0x801B, // 文件下载分片
    CREATE_GROUP_RSP  = 0x801C, // 创建群聊响应
    GROUP_LIST_RSP    = 0x801D, // 群聊列表响应
    GROUP_LIST_UPDATE = 0x801E, // 群聊列表更新通知
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
