/**
 * @file session.h
 * @brief 客户端会话管理
 *
 * 每个客户端连接对应一个 Session 对象
 *
 * 职责：
 * 1. 管理 TCP 连接（socket）
 * 2. 读写缓冲区管理
 * 3. 消息解析（处理粘包）
 * 4. 心跳检测
 * 5. 异步读写操作
 *
 * 线程安全：
 * - Session 对象由 shared_ptr 管理
 * - 读写操作都在 ASIO 事件循环中执行
 * - Session 之间的通信通过线程池
 */

#pragma once

#include "protocol/message.h"
#include "protocol/codec.h"
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <memory>
#include <string>
#include <functional>
#include <queue>

namespace im {

// 前向声明
class Dispatcher;
class Server;

/**
 * @brief 客户端会话类
 *
 * 使用 enable_shared_from_this 使得对象可以获取自己的 shared_ptr
 * 用于在异步回调中保持对象生命周期
 */
class Session : public std::enable_shared_from_this<Session> {
public:
    /**
     * @brief 会话状态
     */
    enum class State {
        CONNECTED,    // 已连接
        LOGGED_IN,    // 已登录
        CLOSING,      // 正在关闭
        CLOSED        // 已关闭
    };

    using Ptr = std::shared_ptr<Session>;

    /**
     * @brief 构造函数
     *
     * @param socket ASIO socket
     * @param server Server 引用（用于回调）
     */
    Session(boost::asio::ip::tcp::socket socket, Server& server);

    /**
     * @brief 静态工厂方法（必须使用此方法创建 Session）
     *
     * 由于 Session 继承自 enable_shared_from_this，
     * 必须通过 make_shared 配合私有构造函数来创建对象
     *
     * @param socket ASIO socket
     * @param server Server 引用
     * @return Ptr 新的 Session 指针
     */
    static Ptr create(boost::asio::ip::tcp::socket socket, Server& server) {
        return std::make_shared<Session>(std::move(socket), server);
    }

    // 禁止拷贝
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    /**
     * @brief 获取用户ID
     */
    const std::string& user_id() const { return user_id_; }

    /**
     * @brief 获取会话状态
     */
    State state() const { return state_; }

    /**
     * @brief 获取远程端点
     */
    std::string remote_endpoint() const;

    /**
     * @brief 启动会话
     *
     * 开始异步读取和心跳检测
     */
    void start();

    /**
     * @brief 关闭会话
     *
     * 取消心跳定时器，关闭 socket
     */
    void close();

    /**
     * @brief 发送消息
     *
     * @param type 消息类型
     * @param body 消息体
     */
    void send(MsgType type, const std::string& body);

    /**
     * @brief 发送消息（便捷接口）
     *
     * @param msg 消息结构
     */
    void send(const Message& msg);

    /**
     * @brief 设置消息分发器
     */
    void set_dispatcher(Dispatcher* dispatcher) { dispatcher_ = dispatcher; }

    /**
     * @brief 设置用户ID（登录成功后）
     */
    void set_user_id(const std::string& user_id) { user_id_ = user_id; state_ = State::LOGGED_IN; }

    /**
     * @brief 获取 socket 引用
     */
    boost::asio::ip::tcp::socket& socket() { return socket_; }

private:
    /**
     * @brief 开始异步读取
     */
    void do_read();

    /**
     * @brief 处理读取到的数据
     *
     * @param ec 错误码
     * @param bytes_read 读取字节数
     */
    void handle_read(const boost::system::error_code& ec, std::size_t bytes_read);

    /**
     * @brief 处理完整消息
     *
     * @param msg 消息
     */
    void handle_message(const Message& msg);

    /**
     * @brief 开始异步写入
     */
    void do_write();

    /**
     * @brief 处理写入完成
     *
     * @param ec 错误码
     */
    void handle_write(const boost::system::error_code& ec);

    /**
     * @brief 启动心跳检测
     */
    void start_heartbeat();

    /**
     * @brief 处理心跳超时
     */
    void handle_heartbeat(const boost::system::error_code& ec);

    /**
     * @brief 发送心跳响应
     */
    void send_heartbeat_rsp();

private:
    boost::asio::ip::tcp::socket socket_;       // TCP socket
    Server& server_;                              // Server 引用
    Dispatcher* dispatcher_;                     // 消息分发器

    // 读写缓冲区
    boost::asio::streambuf read_buf_;            // 读取缓冲区
    boost::asio::streambuf write_buf_;          // 写入缓冲区

    // 写入队列（支持多消息并发）
    std::queue<Message> write_queue_;
    bool writing_{false};                        // 是否正在写入

    // 编码器
    Codec codec_;

    // 心跳
    boost::asio::steady_timer heartbeat_timer_;  // 心跳定时器
    static constexpr int HEARTBEAT_INTERVAL = 30;  // 心跳间隔（秒）
    static constexpr int HEARTBEAT_TIMEOUT = 90;  // 心跳超时（秒）

    // 状态
    State state_{State::CONNECTED};
    std::string user_id_;                         // 用户ID（登录后设置）
};

} // namespace im
