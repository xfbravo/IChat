/**
 * @file server.cpp
 * @brief TCP 服务器实现
 */

#include "server.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>

namespace im {

Server::Server(uint16_t port, std::size_t thread_count, DbPool& db_pool)
    : port_(port)
    , thread_pool_(thread_count)
    , acceptor_(thread_pool_.get_io_context())
    , db_pool_(db_pool)
    , user_service_(db_pool)
{
    std::cout << "[Server] 创建服务器，端口: " << port
              << ", 线程数: " << thread_count << std::endl;
}

void Server::start() {
    if (running_) {
        std::cout << "[Server] 服务器已在运行" << std::endl;
        return;
    }

    running_ = true;

    // 注册默认消息处理器
    register_default_handlers();

    // 启动线程池
    thread_pool_.start();

    // 打开接受器
    boost::system::error_code ec;
    acceptor_.open(boost::asio::ip::tcp::v4(), ec);
    if (ec) {
        std::cerr << "[Server] 打开接受器失败: " << ec.message() << std::endl;
        return;
    }

    // 设置 socket 选项
    acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true), ec);
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);

    // 绑定端口
    acceptor_.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port_), ec);
    if (ec) {
        std::cerr << "[Server] 绑定端口失败: " << ec.message() << std::endl;
        return;
    }

    // 开始监听
    acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec) {
        std::cerr << "[Server] 开始监听失败: " << ec.message() << std::endl;
        return;
    }

    std::cout << "[Server] 服务器监听端口: " << port_ << std::endl;

    // 开始接受连接
    start_accept();
}

void Server::stop() {
    if (!running_) {
        return;
    }

    std::cout << "[Server] 停止服务器..." << std::endl;
    running_ = false;

    // 关闭接受器
    boost::system::error_code ec;
    acceptor_.close(ec);

    // 关闭所有会话
    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        for (auto& [user_id, session] : sessions_) {
            session->close();
        }
        sessions_.clear();
        endpoints_.clear();
    }

    // 停止线程池
    thread_pool_.stop();

    std::cout << "[Server] 服务器已停止" << std::endl;
}

void Server::start_accept() {
    auto session = Session::create(
        boost::asio::ip::tcp::socket(thread_pool_.get_io_context()),
        *this
    );

    auto self = shared_from_this();

    acceptor_.async_accept(
        session->socket(),
        [this, self, session](const boost::system::error_code& ec) {
            handle_accept(ec, session);
        }
    );
}

void Server::handle_accept(const boost::system::error_code& ec, Session::Ptr session) {
    if (ec) {
        std::cerr << "[Server] 接受连接失败: " << ec.message() << std::endl;
        // 继续尝试接受其他连接
        if (running_) {
            start_accept();
        }
        return;
    }

    std::string endpoint = session->remote_endpoint();
    std::cout << "[Server] 新连接: " << endpoint << std::endl;

    // 注册会话
    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        endpoints_[endpoint] = session;
    }

    // 启动会话
    session->set_dispatcher(&dispatcher_);
    session->start();

    // 继续接受下一个连接
    if (running_) {
        start_accept();
    }
}

void Server::register_default_handlers() {
    // 设置默认处理器
    dispatcher_.set_default_handler([](std::shared_ptr<Session> session, const Message& msg) {
        (void)session; (void)msg;
        std::cout << "[Server] 未处理的消息: type=0x" << std::hex
                  << static_cast<uint16_t>(msg.type) << std::dec << std::endl;
    });

    // 心跳消息处理
    dispatcher_.register_handler(MsgType::HEARTBEAT, [](std::shared_ptr<Session> session, const Message& msg) {
        (void)msg;
        std::cout << "[Server] 收到心跳 from: " << session->user_id() << std::endl;
        session->send(MsgType::HEARTBEAT, "{\"server_time\":" + std::to_string(time(nullptr)) + "}");
    });

    // 登录消息处理（使用数据库验证）
    dispatcher_.register_handler(MsgType::LOGIN, [this](std::shared_ptr<Session> session, const Message& msg) {
        std::cout << "[Server] 收到登录请求: " << msg.body << std::endl;

        // 解析登录请求 JSON
        // 格式: {"user_id":"xxx","password":"xxx"}
        std::string user_id, password;

        // 简单解析 JSON（实际应用应使用 JSON 库）
        std::string body = msg.body;

        // 解析 user_id：先找 ":"user_id":" ，然后找值
        size_t uid_pos = body.find("\"user_id\":");
        if (uid_pos != std::string::npos) {
            size_t value_start = body.find("\"", uid_pos + 10);  // 10 = strlen("\"user_id\":")
            if (value_start != std::string::npos) {
                size_t value_end = body.find("\"", value_start + 1);
                if (value_end != std::string::npos) {
                    user_id = body.substr(value_start + 1, value_end - value_start - 1);
                }
            }
        }

        // 解析 password
        size_t pwd_pos = body.find("\"password\":");
        if (pwd_pos != std::string::npos) {
            size_t value_start = body.find("\"", pwd_pos + 10);  // 10 = strlen("\"password\":")
            if (value_start != std::string::npos) {
                size_t value_end = body.find("\"", value_start + 1);
                if (value_end != std::string::npos) {
                    password = body.substr(value_start + 1, value_end - value_start - 1);
                }
            }
        }

        // 调用用户服务验证登录
        LoginResult login_result = user_service_.login(user_id, password);

        // 构建响应 JSON
        std::ostringstream rsp;
        rsp << "{\"code\":" << login_result.code
            << ",\"message\":\"" << login_result.message << "\"";

        if (login_result.code == 0) {
            rsp << ",\"user_id\":\"" << login_result.user_id << "\""
                << ",\"nickname\":\"" << login_result.nickname << "\""
                << ",\"avatar_url\":\"" << login_result.avatar_url << "\""
                << ",\"token\":\"" << login_result.token << "\"";

            // 注册会话
            {
                std::lock_guard<std::mutex> lock(session_mutex_);
                // 移除旧会话（如果有）
                if (!session->user_id().empty()) {
                    sessions_.erase(session->user_id());
                }
                // 添加新会话
                session->set_user_id(login_result.user_id);
                sessions_[login_result.user_id] = session;
            }

            // 更新登录信息
            user_service_.update_login_info(login_result.user_id, "0.0.0.0");

            std::cout << "[Server] 用户登录成功: " << login_result.user_id << std::endl;
        }

        rsp << "}";

        session->send(MsgType::LOGIN_RSP, rsp.str());
    });

    // 注册消息处理
    dispatcher_.register_handler(MsgType::REGISTER_REQ, [this](std::shared_ptr<Session> session, const Message& msg) {
        std::cout << "[Server] 收到注册请求: " << msg.body << std::endl;

        // 解析注册请求 JSON
        // 格式: {"phone":"xxx","nickname":"xxx","password":"xxx"}
        std::string phone, nickname, password;

        std::string body = msg.body;

        // 解析 phone
        size_t phone_pos = body.find("\"phone\":");
        if (phone_pos != std::string::npos) {
            size_t value_start = body.find("\"", phone_pos + 7);
            if (value_start != std::string::npos) {
                size_t value_end = body.find("\"", value_start + 1);
                if (value_end != std::string::npos) {
                    phone = body.substr(value_start + 1, value_end - value_start - 1);
                }
            }
        }

        // 解析 nickname
        size_t name_pos = body.find("\"nickname\":");
        if (name_pos != std::string::npos) {
            size_t value_start = body.find("\"", name_pos + 9);
            if (value_start != std::string::npos) {
                size_t value_end = body.find("\"", value_start + 1);
                if (value_end != std::string::npos) {
                    nickname = body.substr(value_start + 1, value_end - value_start - 1);
                }
            }
        }

        // 解析 password
        size_t pwd_pos = body.find("\"password\":");
        if (pwd_pos != std::string::npos) {
            size_t value_start = body.find("\"", pwd_pos + 10);
            if (value_start != std::string::npos) {
                size_t value_end = body.find("\"", value_start + 1);
                if (value_end != std::string::npos) {
                    password = body.substr(value_start + 1, value_end - value_start - 1);
                }
            }
        }

        // 调用用户服务注册
        LoginResult reg_result = user_service_.register_user(phone, nickname, password);

        // 构建响应 JSON
        std::ostringstream rsp;
        rsp << "{\"code\":" << reg_result.code
            << ",\"message\":\"" << reg_result.message << "\"";

        if (reg_result.code == 0) {
            rsp << ",\"user_id\":\"" << reg_result.user_id << "\"";
            std::cout << "[Server] 用户注册成功: " << reg_result.user_id << std::endl;
        }

        rsp << "}";

        session->send(MsgType::REGISTER_RSP, rsp.str());
    });

    // 登出消息处理
    dispatcher_.register_handler(MsgType::LOGOUT, [this](std::shared_ptr<Session> session, const Message& msg) {
        (void)msg;
        std::cout << "[Server] 收到登出请求: " << session->user_id() << std::endl;

        // 移除会话
        {
            std::lock_guard<std::mutex> lock(session_mutex_);
            sessions_.erase(session->user_id());
        }

        session->close();
    });

    // 文本消息处理
    dispatcher_.register_handler(MsgType::TEXT, [this](std::shared_ptr<Session> session, const Message& msg) {
        std::cout << "[Server] 收到文本消息 from " << session->user_id() << ": " << msg.body << std::endl;

        // 简单解析 JSON 中的 to_user_id
        std::string to_user_id;
        size_t pos = msg.body.find("\"to_user_id\":\"");
        if (pos != std::string::npos) {
            pos += 14;
            size_t end = msg.body.find("\"", pos);
            if (end != std::string::npos) {
                to_user_id = msg.body.substr(pos, end - pos);
            }
        }

        if (!to_user_id.empty()) {
            // 转发消息到目标用户
            if (!send_to_user(to_user_id, MsgType::TEXT, msg.body)) {
                std::cout << "[Server] 用户不在线: " << to_user_id << std::endl;
            }
        }
    });

    // 图片/文件/语音消息处理（类似文本消息）
    dispatcher_.register_handler(MsgType::IMAGE, [this](std::shared_ptr<Session> session, const Message& msg) {
        std::cout << "[Server] 收到图片消息 from " << session->user_id() << std::endl;
        std::string to_user_id;
        size_t pos = msg.body.find("\"to_user_id\":\"");
        if (pos != std::string::npos) {
            pos += 14;
            size_t end = msg.body.find("\"", pos);
            if (end != std::string::npos) {
                to_user_id = msg.body.substr(pos, end - pos);
            }
        }
        if (!to_user_id.empty()) {
            send_to_user(to_user_id, MsgType::IMAGE, msg.body);
        }
    });

    dispatcher_.register_handler(MsgType::FILE, [this](std::shared_ptr<Session> session, const Message& msg) {
        std::cout << "[Server] 收到文件消息 from " << session->user_id() << std::endl;
        std::string to_user_id;
        size_t pos = msg.body.find("\"to_user_id\":\"");
        if (pos != std::string::npos) {
            pos += 14;
            size_t end = msg.body.find("\"", pos);
            if (end != std::string::npos) {
                to_user_id = msg.body.substr(pos, end - pos);
            }
        }
        if (!to_user_id.empty()) {
            send_to_user(to_user_id, MsgType::FILE, msg.body);
        }
    });

    dispatcher_.register_handler(MsgType::VOICE, [this](std::shared_ptr<Session> session, const Message& msg) {
        std::cout << "[Server] 收到语音消息 from " << session->user_id() << std::endl;
        std::string to_user_id;
        size_t pos = msg.body.find("\"to_user_id\":\"");
        if (pos != std::string::npos) {
            pos += 14;
            size_t end = msg.body.find("\"", pos);
            if (end != std::string::npos) {
                to_user_id = msg.body.substr(pos, end - pos);
            }
        }
        if (!to_user_id.empty()) {
            send_to_user(to_user_id, MsgType::VOICE, msg.body);
        }
    });

    std::cout << "[Server] 消息处理器注册完成" << std::endl;
}

std::size_t Server::session_count() const {
    std::lock_guard<std::mutex> lock(session_mutex_);
    return sessions_.size();
}

void Server::broadcast(MsgType type, const std::string& body) {
    std::lock_guard<std::mutex> lock(session_mutex_);
    for (auto& [user_id, session] : sessions_) {
        session->send(type, body);
    }
}

bool Server::send_to_user(const std::string& user_id, MsgType type, const std::string& body) {
    std::lock_guard<std::mutex> lock(session_mutex_);
    auto it = sessions_.find(user_id);
    if (it != sessions_.end()) {
        it->second->send(type, body);
        return true;
    }
    return false;
}

void Server::remove_session(Session::Ptr session) {
    std::lock_guard<std::mutex> lock(session_mutex_);
    if (!session->user_id().empty()) {
        sessions_.erase(session->user_id());
    }
    endpoints_.erase(session->remote_endpoint());
}

} // namespace im
