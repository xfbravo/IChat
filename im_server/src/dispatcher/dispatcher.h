/**
 * @file dispatcher.h
 * @brief 消息分发器
 *
 * 职责：
 * 1. 根据消息类型分发到对应的处理函数
 * 2. 隔离业务逻辑和网络层
 *
 * 设计模式：策略模式 + 工厂模式
 * 每种消息类型注册一个 Handler，收到消息时自动分发
 */

#pragma once

#include "protocol/message.h"
#include <functional>
#include <unordered_map>
#include <string>

namespace im {

// 前向声明
class Session;

/**
 * @brief 消息处理函数类型
 *
 * @param session 发送消息的会话
 * @param msg 接收到的消息
 */
using MessageHandler = std::function<void(std::shared_ptr<Session>, const Message&)>;

/**
 * @brief 消息分发器类
 *
 * 使用示例：
 * @code
 * dispatcher.register_handler(MsgType::TEXT, [](auto session, auto& msg) {
 *     // 处理文本消息
 * });
 * @endcode
 */
class Dispatcher {
public:
    Dispatcher() = default;

    // 禁止拷贝
    Dispatcher(const Dispatcher&) = delete;
    Dispatcher& operator=(const Dispatcher&) = delete;

    /**
     * @brief 注册消息处理器
     *
     * @param type 消息类型
     * @param handler 处理函数
     */
    void register_handler(MsgType type, MessageHandler handler);

    /**
     * @brief 分发消息到对应处理器
     *
     * @param session 发送消息的会话
     * @param msg 消息
     */
    void dispatch(std::shared_ptr<Session> session, const Message& msg);

    /**
     * @brief 获取默认（未处理）处理器
     */
    MessageHandler default_handler() const { return default_handler_; }

    /**
     * @brief 设置默认（未处理）处理器
     */
    void set_default_handler(MessageHandler handler) { default_handler_ = handler; }

private:
    std::unordered_map<MsgType, MessageHandler> handlers_;  // 消息类型 -> 处理器映射
    MessageHandler default_handler_;                         // 默认处理器
};

} // namespace im
