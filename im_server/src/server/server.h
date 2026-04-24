/**
 * @file server.h
 * @brief TCP 服务器主类
 *
 * 职责：
 * 1. 监听端口，接受新连接
 * 2. 管理所有客户端 Session
 * 3. 管理线程池
 * 4. 管理消息分发器
 * 5. 提供广播消息功能
 *
 * 网络模型：Reactor 模型
 * - 主 reactor（io_context）处理 accept 事件
 * - 每个 Session 独立处理读写事件
 * - 业务逻辑在线程池中执行
 */

#pragma once

#include "session/session.h"
#include "thread/thread_pool.h"
#include "dispatcher/dispatcher.h"
#include "db/db_pool.h"
#include "db/user_service.h"
#include <boost/asio.hpp>
#include <memory>
#include <unordered_map>
#include <string>
#include <mutex>

namespace im {

/**
 * @brief TCP 服务器类
 */
class Server : public std::enable_shared_from_this<Server> {
public:
    using Ptr = std::shared_ptr<Server>;

    /**
     * @brief 构造函数
     *
     * @param port 监听端口
     * @param thread_count 工作线程数量
     * @param db_pool 数据库连接池引用
     */
    Server(uint16_t port, std::size_t thread_count, DbPool& db_pool);

    // 禁止拷贝
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    /**
     * @brief 启动服务器
     */
    void start();

    /**
     * @brief 停止服务器
     */
    void stop();

    /**
     * @brief 获取监听端口
     */
    uint16_t port() const { return port_; }

    /**
     * @brief 获取会话数量
     */
    std::size_t session_count() const;

    /**
     * @brief 广播消息给所有登录的客户端
     *
     * @param type 消息类型
     * @param body 消息体
     */
    void broadcast(MsgType type, const std::string& body);

    /**
     * @brief 发送消息给指定用户
     *
     * @param user_id 用户ID
     * @param type 消息类型
     * @param body 消息体
     * @return true 发送成功，false 用户不在线
     */
    bool send_to_user(const std::string& user_id, MsgType type, const std::string& body);

    /**
     * @brief 获取线程池
     */
    ThreadPool& thread_pool() { return thread_pool_; }

    /**
     * @brief 获取消息分发器
     */
    Dispatcher& dispatcher() { return dispatcher_; }

    /**
     * @brief 获取用户服务
     */
    UserService& user_service() { return user_service_; }

private:
    /**
     * @brief 开始接受新连接
     */
    void start_accept();

    /**
     * @brief 处理接受连接结果
     *
     * @param ec 错误码
     * @param session 新会话
     */
    void handle_accept(const boost::system::error_code& ec, Session::Ptr session);

    /**
     * @brief 注册默认消息处理器
     */
    void register_default_handlers();

    /**
     * @brief 移除会话
     *
     * @param session 会话
     */
    void remove_session(Session::Ptr session);

private:
    uint16_t port_;                              // 监听端口
    ThreadPool thread_pool_;                     // 线程池（必须在 acceptor_ 之前初始化）
    boost::asio::ip::tcp::acceptor acceptor_;    // 接受器
    Dispatcher dispatcher_;                       // 消息分发器
    DbPool& db_pool_;                           // 数据库连接池引用
    UserService user_service_;                    // 用户服务

    // Session 管理
    std::unordered_map<std::string, Session::Ptr> sessions_;  // user_id -> session
    std::unordered_map<std::string, Session::Ptr> endpoints_; // endpoint -> session
    mutable std::mutex session_mutex_;                       // 保护 sessions_

    bool running_{false};                                    // 运行状态
};

} // namespace im
