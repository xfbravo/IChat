/**
 * @file session.cpp
 * @brief 客户端会话实现
 */

#include "session.h"
#include "server/server.h"
#include <iostream>

namespace im {

Session::Session(boost::asio::ip::tcp::socket socket, Server& server)
    : socket_(std::move(socket))
    , server_(server)
    , dispatcher_(nullptr)
    , heartbeat_timer_(socket_.get_executor())
{
    std::cout << "[Session] 新会话创建: " << remote_endpoint() << std::endl;
}

std::string Session::remote_endpoint() const {
    try {
        auto ep = socket_.remote_endpoint();
        return ep.address().to_string() + ":" + std::to_string(ep.port());
    } catch (const std::exception& e) {
        return "unknown";
    }
}

void Session::start() {
    std::cout << "[Session] 会话启动: " << remote_endpoint() << std::endl;
    state_ = State::CONNECTED;

    // 启动心跳检测
    start_heartbeat();

    // 开始异步读取
    do_read();
}

void Session::close() {
    if (state_ == State::CLOSED || state_ == State::CLOSING) {
        return;
    }

    std::cout << "[Session] 会话关闭: " << remote_endpoint() << std::endl;
    state_ = State::CLOSING;

    // 取消心跳定时器（返回被取消的异步操作数量）
    heartbeat_timer_.cancel();

    // 关闭 socket
    boost::system::error_code close_ec;
    socket_.close(close_ec);

    state_ = State::CLOSED;
}

void Session::send(MsgType type, const std::string& body) {
    auto self = shared_from_this();
    auto msg = std::make_shared<Message>(type, body);

    // 使用 boost::asio::post 确保线程安全
    boost::asio::post(socket_.get_executor(), [this, self, msg]() {
        if (state_ == State::CLOSING || state_ == State::CLOSED) {
            return;
        }

        // 构建消息并加入发送队列
        write_queue_.push(*msg);

        // 如果当前没有正在进行的写入，开始写入
        if (!writing_) {
            do_write();
        }
    });
}

void Session::send(const Message& msg) {
    send(msg.type, msg.body);
}

void Session::do_read() {
    auto self = shared_from_this();

    // 使用 async_read_some 读取数据
    // Streambuf 会自动累积数据，直到我们消费它
    socket_.async_read_some(
        read_buf_.prepare(8192),  // 预留 8KB 读取空间
        [this, self](const boost::system::error_code& ec, std::size_t bytes_read) {
            handle_read(ec, bytes_read);
        }
    );
}

void Session::handle_read(const boost::system::error_code& ec, std::size_t bytes_read) {
    if (ec) {
        if (ec == boost::asio::error::eof) {
            // 客户端关闭连接
            std::cout << "[Session] 客户端关闭连接: " << remote_endpoint() << std::endl;
        } else if (ec == boost::asio::error::operation_aborted) {
            // 操作被取消（可能是定时器取消或 socket 关闭）
            std::cout << "[Session] 读取操作取消: " << remote_endpoint() << std::endl;
        } else {
            std::cerr << "[Session] 读取错误: " << ec.message() << std::endl;
        }
        close();
        return;
    }

    // 提交读取到的数据到缓冲区
    read_buf_.commit(bytes_read);

    // 确认读取的数据
    std::cout << "[Session] 读取 " << bytes_read << " 字节 from " << remote_endpoint()
              << ", 缓冲区大小: " << read_buf_.size() << std::endl;
    std::cout << "[Session] 读取 " << bytes_read << " 字节 from " << remote_endpoint()
              << ", 缓冲区大小: " << read_buf_.size() << std::endl;

    // 循环解析消息（处理粘包）
    // 不断从缓冲区中取出完整的消息，直到缓冲区数据不足
    while (true) {
        boost::system::error_code decode_ec;
        MessagePtr msg = codec_.decode(read_buf_, decode_ec);

        if (decode_ec) {
            if (decode_ec == boost::asio::error::would_block) {
                // 数据不足，需要继续接收
                std::cout << "[Session] 数据不足，等待更多数据..." << std::endl;
            } else {
                std::cerr << "[Session] 解码错误: " << decode_ec.message() << std::endl;
                close();
            }
            break;
        }

        // 成功解析消息，处理它
        if (msg) {
            handle_message(*msg);
        }
    }

    // 继续读取
    if (state_ != State::CLOSED && state_ != State::CLOSING) {
        do_read();
    }
}

void Session::handle_message(const Message& msg) {
    std::cout << "[Session] 处理消息: type=0x" << std::hex << static_cast<uint16_t>(msg.type)
              << std::dec << ", body=" << msg.body << ", from=" << remote_endpoint() << std::endl;

    // 重置心跳定时器（收到任何消息都认为连接活跃）
    start_heartbeat();

    // 如果有分发器，分发消息到业务逻辑
    if (dispatcher_) {
        dispatcher_->dispatch(shared_from_this(), msg);
    } else {
        std::cerr << "[Session] 未设置分发器，无法处理消息" << std::endl;
    }
}

void Session::do_write() {
    if (writing_ || write_queue_.empty()) {
        return;
    }

    writing_ = true;

    auto self = shared_from_this();

    // 从队列取出消息并编码到写缓冲区
    Message& msg = write_queue_.front();
    write_buf_.consume(write_buf_.size());  // 清空写缓冲区
    codec_.encode(msg.type, msg.body, write_buf_);
    write_queue_.pop();

    // 异步写入
    boost::asio::async_write(
        socket_,
        write_buf_.data(),
        [this, self](const boost::system::error_code& ec, std::size_t /*bytes_write*/) {
            handle_write(ec);
        }
    );
}

void Session::handle_write(const boost::system::error_code& ec) {
    writing_ = false;

    if (ec) {
        std::cerr << "[Session] 写入错误: " << ec.message() << std::endl;
        close();
        return;
    }

    // 清空已发送的数据
    write_buf_.consume(write_buf_.size());

    // 如果队列中还有消息，继续发送
    if (!write_queue_.empty()) {
        do_write();
    }
}

void Session::start_heartbeat() {
    auto self = shared_from_this();

    // 取消之前的定时器（返回被取消的异步操作数量）
    heartbeat_timer_.cancel();

    // 设置新的定时器
    heartbeat_timer_.expires_after(std::chrono::seconds(HEARTBEAT_INTERVAL));
    heartbeat_timer_.async_wait([this, self](const boost::system::error_code& ec) {
        handle_heartbeat(ec);
    });
}

void Session::handle_heartbeat(const boost::system::error_code& ec) {
    if (ec) {
        if (ec == boost::asio::error::operation_aborted) {
            // 定时器被取消（正常情况）
        } else {
            std::cerr << "[Session] 心跳定时器错误: " << ec.message() << std::endl;
        }
        return;
    }

    // 心跳超时
    std::cout << "[Session] 心跳超时，关闭连接: " << remote_endpoint() << std::endl;
    close();
}

void Session::send_heartbeat_rsp() {
    if (state_ == State::CLOSED || state_ == State::CLOSING) {
        return;
    }

    // 心跳响应可以很简单，只发送一个空消息
    // 实际应用中可以根据协议设计具体内容
    send(MsgType::HEARTBEAT, "{}");
}

} // namespace im
