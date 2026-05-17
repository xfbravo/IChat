/**
 * @file dispatcher.cpp
 * @brief 消息分发器实现
 */

#include "dispatcher.h"
#include "session/session.h"
#include <iostream>

namespace im {

void Dispatcher::register_handler(MsgType type, MessageHandler handler) {
    handlers_[type] = std::move(handler);
    std::cout << "[Dispatcher] 注册处理器: type=0x" << std::hex
              << static_cast<uint16_t>(type) << std::dec << std::endl;
}

void Dispatcher::dispatch(std::shared_ptr<Session> session, const Message& msg) {
    try {
        auto it = handlers_.find(msg.type);
        if (it != handlers_.end()) {
            std::cout << "[Dispatcher] 分发消息: type=0x" << std::hex
                      << static_cast<uint16_t>(msg.type) << std::dec << std::endl;
            it->second(session, msg);
        } else if (default_handler_) {
            std::cout << "[Dispatcher] 使用默认处理器: type=0x" << std::hex
                      << static_cast<uint16_t>(msg.type) << std::dec << std::endl;
            default_handler_(session, msg);
        } else {
            std::cerr << "[Dispatcher] 未找到处理器: type=0x" << std::hex
                      << static_cast<uint16_t>(msg.type) << std::dec << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[Dispatcher] 处理器异常: type=0x" << std::hex
                  << static_cast<uint16_t>(msg.type) << std::dec
                  << ", error=" << e.what() << std::endl;
        if (session) {
            session->send(MsgType::ERROR, "{\"code\":500,\"message\":\"服务器处理消息失败\"}");
        }
    } catch (...) {
        std::cerr << "[Dispatcher] 处理器未知异常: type=0x" << std::hex
                  << static_cast<uint16_t>(msg.type) << std::dec << std::endl;
        if (session) {
            session->send(MsgType::ERROR, "{\"code\":500,\"message\":\"服务器处理消息失败\"}");
        }
    }
}

} // namespace im
