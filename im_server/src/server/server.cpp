/**
 * @file server.cpp
 * @brief TCP 服务器实现
 */

#include "server.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <ctime>
#include <stdexcept>
#include <vector>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <random>
#include <cctype>
#include <boost/json.hpp>

namespace im {

namespace {

namespace json = boost::json;

int64_t current_time_millis() {
    const auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

json::object parse_json_object(const std::string& body) {
    json::value value = json::parse(body);
    if (!value.is_object()) {
        throw std::runtime_error("JSON body must be an object");
    }
    return value.as_object();
}

std::string json_string(const json::object& obj, const char* key, const std::string& default_value = "") {
    auto it = obj.find(key);
    if (it == obj.end() || !it->value().is_string()) {
        return default_value;
    }

    const auto& value = it->value().as_string();
    return std::string(value.data(), value.size());
}

int64_t json_int64(const json::object& obj, const char* key, int64_t default_value = 0) {
    auto it = obj.find(key);
    if (it == obj.end()) {
        return default_value;
    }

    const json::value& value = it->value();
    if (value.is_int64()) return value.as_int64();
    if (value.is_uint64()) return static_cast<int64_t>(value.as_uint64());
    if (value.is_double()) return static_cast<int64_t>(value.as_double());
    return default_value;
}

bool json_bool(const json::object& obj, const char* key, bool default_value = false) {
    auto it = obj.find(key);
    if (it == obj.end() || !it->value().is_bool()) {
        return default_value;
    }
    return it->value().as_bool();
}

uint64_t json_uint64(const json::object& obj, const char* key, uint64_t default_value = 0) {
    auto it = obj.find(key);
    if (it == obj.end()) {
        return default_value;
    }

    const json::value& value = it->value();
    if (value.is_uint64()) return value.as_uint64();
    if (value.is_int64() && value.as_int64() >= 0) return static_cast<uint64_t>(value.as_int64());
    if (value.is_double() && value.as_double() >= 0) return static_cast<uint64_t>(value.as_double());
    return default_value;
}

json::array json_array_value(const json::object& obj, const char* key) {
    auto it = obj.find(key);
    if (it == obj.end() || !it->value().is_array()) {
        return json::array();
    }
    return it->value().as_array();
}

std::string json_response(int code, const std::string& message) {
    json::object rsp;
    rsp["code"] = code;
    rsp["message"] = message;
    return json::serialize(rsp);
}

std::string random_hex_id(std::size_t bytes = 16) {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, 255);

    std::ostringstream out;
    for (std::size_t i = 0; i < bytes; ++i) {
        out << std::hex << std::setw(2) << std::setfill('0') << dist(rng);
    }
    return out.str();
}

std::string safe_file_name(const std::string& file_name) {
    std::string safe;
    for (unsigned char ch : file_name) {
        if (std::isalnum(ch) || ch == '.' || ch == '_' || ch == '-') {
            safe.push_back(static_cast<char>(ch));
        } else {
            safe.push_back('_');
        }
    }
    return safe.empty() ? "file" : safe;
}

const std::string& base64_chars() {
    static const std::string chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    return chars;
}

std::string base64_encode(const std::string& input) {
    const std::string& chars = base64_chars();
    std::string output;
    int val = 0;
    int valb = -6;
    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            output.push_back(chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) {
        output.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    while (output.size() % 4) {
        output.push_back('=');
    }
    return output;
}

std::string base64_decode(const std::string& input) {
    std::vector<int> table(256, -1);
    const std::string& chars = base64_chars();
    for (int i = 0; i < static_cast<int>(chars.size()); ++i) {
        table[static_cast<unsigned char>(chars[i])] = i;
    }

    std::string output;
    int val = 0;
    int valb = -8;
    for (unsigned char c : input) {
        if (std::isspace(c)) continue;
        if (c == '=') break;
        if (table[c] == -1) {
            throw std::runtime_error("invalid base64 data");
        }
        val = (val << 6) + table[c];
        valb += 6;
        if (valb >= 0) {
            output.push_back(static_cast<char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return output;
}

std::filesystem::path file_storage_dir() {
    return std::filesystem::current_path() / "storage" / "files";
}

std::string message_ack(const std::string& msg_id, const std::string& status,
                        int code, const std::string& message) {
    json::object rsp;
    rsp["msg_id"] = msg_id;
    rsp["status"] = status;
    rsp["code"] = code;
    rsp["message"] = message;
    return json::serialize(rsp);
}

std::string sql_escape(MYSQL* mysql, const std::string& value) {
    std::string escaped;
    escaped.resize(value.size() * 2 + 1);
    unsigned long length = mysql_real_escape_string(
        mysql, escaped.data(), value.data(), static_cast<unsigned long>(value.size()));
    escaped.resize(length);
    return escaped;
}

int chat_content_type_code(const std::string& content_type) {
    if (content_type == "image") return 2;
    if (content_type == "file") return 3;
    if (content_type == "voice") return 4;
    if (content_type == "video") return 5;
    return 1;
}

std::string default_content_type(MsgType type) {
    switch (type) {
        case MsgType::IMAGE: return "image";
        case MsgType::FILE: return "file";
        case MsgType::VOICE: return "voice";
        default: return "text";
    }
}

} // namespace

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

    // 关闭所有会话。先移出 map，再逐个 close，避免 close() 回调 remove_session() 时自锁。
    std::vector<Session::Ptr> sessions_to_close;
    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        for (auto& [user_id, session] : sessions_) {
            sessions_to_close.push_back(session);
        }
        for (auto& [endpoint, session] : endpoints_) {
            sessions_to_close.push_back(session);
        }
        sessions_.clear();
        endpoints_.clear();
    }
    for (auto& session : sessions_to_close) {
        session->close();
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

        std::string user_id, password;
        try {
            json::object req = parse_json_object(msg.body);
            user_id = json_string(req, "user_id");
            password = json_string(req, "password");
        } catch (const std::exception& e) {
            session->send(MsgType::LOGIN_RSP, json_response(400, std::string("无效 JSON: ") + e.what()));
            return;
        }

        // 调用用户服务验证登录
        LoginResult login_result = user_service_.login(user_id, password);

        json::object rsp;
        rsp["code"] = login_result.code;
        rsp["message"] = login_result.message;

        if (login_result.code == 0) {
            rsp["user_id"] = login_result.user_id;
            rsp["nickname"] = login_result.nickname;
            rsp["avatar_url"] = login_result.avatar_url;
            rsp["gender"] = login_result.gender;
            rsp["region"] = login_result.region;
            rsp["signature"] = login_result.signature;
            rsp["token"] = login_result.token;

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
            const std::string rsp_body = json::serialize(rsp);
            std::cout << "[Server] 登录响应: " << rsp_body << std::endl;
            session->send(MsgType::LOGIN_RSP, rsp_body);

            // 发送离线消息
            std::string offline_msgs = user_service_.get_offline_messages(login_result.user_id);
            if (!offline_msgs.empty() && offline_msgs != "[]") {
                std::cout << "[Server] 发送离线消息给: " << login_result.user_id << std::endl;
                session->send(MsgType::OFFLINE_MESSAGE, offline_msgs);
            }
            return;
        }

        session->send(MsgType::LOGIN_RSP, json::serialize(rsp));
    });

    // 注册消息处理
    dispatcher_.register_handler(MsgType::REGISTER_REQ, [this](std::shared_ptr<Session> session, const Message& msg) {
        std::cout << "[Server] 收到注册请求: " << msg.body << std::endl;

        std::string phone, nickname, password;
        try {
            json::object req = parse_json_object(msg.body);
            phone = json_string(req, "phone");
            nickname = json_string(req, "nickname");
            password = json_string(req, "password");
        } catch (const std::exception& e) {
            session->send(MsgType::REGISTER_RSP, json_response(400, std::string("无效 JSON: ") + e.what()));
            return;
        }

        // 调用用户服务注册
        LoginResult reg_result = user_service_.register_user(phone, nickname, password);

        json::object rsp;
        rsp["code"] = reg_result.code;
        rsp["message"] = reg_result.message;

        if (reg_result.code == 0) {
            rsp["user_id"] = reg_result.user_id;
            std::cout << "[Server] 用户注册成功: " << reg_result.user_id << std::endl;
        }

        session->send(MsgType::REGISTER_RSP, json::serialize(rsp));
    });

    // 获取好友列表
    dispatcher_.register_handler(MsgType::GET_FRIEND_LIST, [this](std::shared_ptr<Session> session, const Message& msg) {
        (void)msg;
        std::cout << "[Server] 收到获取好友列表请求: " << session->user_id() << std::endl;
        std::string friend_list = user_service_.get_friend_list(session->user_id());
        session->send(MsgType::FRIEND_LIST_RSP, friend_list);
    });

    // 获取用户个人信息
    dispatcher_.register_handler(MsgType::GET_USER_PROFILE, [this](std::shared_ptr<Session> session, const Message& msg) {
        if (session->user_id().empty()) {
            session->send(MsgType::USER_PROFILE_RSP, json_response(401, "未登录"));
            return;
        }

        std::string target_user_id;
        std::string client_user_id;
        std::string local_nickname;
        std::string local_gender;
        std::string local_region;
        std::string local_signature;
        bool has_local_profile = false;
        try {
            json::object req = parse_json_object(msg.body);
            target_user_id = json_string(req, "user_id");
            client_user_id = json_string(req, "client_user_id");
            auto local_it = req.find("local_profile");
            if (local_it != req.end() && local_it->value().is_object()) {
                const json::object& local_profile = local_it->value().as_object();
                local_nickname = json_string(local_profile, "nickname");
                local_gender = json_string(local_profile, "gender");
                local_region = json_string(local_profile, "region");
                local_signature = json_string(local_profile, "signature");
                has_local_profile = true;
            }
        } catch (const std::exception& e) {
            session->send(MsgType::USER_PROFILE_RSP, json_response(400, std::string("无效 JSON: ") + e.what()));
            return;
        }

        if (target_user_id.empty()) {
            session->send(MsgType::USER_PROFILE_RSP, json_response(400, "用户ID不能为空"));
            return;
        }

        UserInfo profile = user_service_.get_user_by_id(target_user_id);
        if (profile.user_id.empty() || profile.status != 1) {
            session->send(MsgType::USER_PROFILE_RSP, json_response(404, "用户不存在"));
            return;
        }

        const bool same_account_sync =
            has_local_profile
            && target_user_id == session->user_id()
            && client_user_id == session->user_id();
        const bool local_profile_same =
            same_account_sync
            && local_nickname == profile.nickname
            && local_gender == profile.gender
            && local_region == profile.region
            && local_signature == profile.signature;

        json::object rsp;
        rsp["code"] = 0;
        rsp["user_id"] = profile.user_id;
        if (local_profile_same) {
            rsp["message"] = "资料已同步";
            rsp["sync_status"] = "same";
            const std::string rsp_body = json::serialize(rsp);
            std::cout << "[Server] 用户资料一致，仅返回ACK: " << rsp_body << std::endl;
            session->send(MsgType::USER_PROFILE_RSP, rsp_body);
            return;
        }

        rsp["message"] = "获取成功";
        rsp["sync_status"] = same_account_sync ? "server_newer" : "full";
        rsp["nickname"] = profile.nickname;
        rsp["avatar_url"] = profile.avatar_url;
        rsp["gender"] = profile.gender;
        rsp["region"] = profile.region;
        rsp["signature"] = profile.signature;
        const std::string rsp_body = json::serialize(rsp);
        std::cout << "[Server] 用户资料响应: " << rsp_body << std::endl;
        session->send(MsgType::USER_PROFILE_RSP, rsp_body);
    });

    // 获取好友请求列表
    dispatcher_.register_handler(MsgType::GET_FRIEND_REQUESTS, [this](std::shared_ptr<Session> session, const Message& msg) {
        (void)msg;
        std::cout << "[Server] 收到获取好友请求列表: " << session->user_id() << std::endl;
        std::string requests = user_service_.get_friend_requests(session->user_id(), 0);
        session->send(MsgType::FRIEND_REQUEST_NEW, requests);
    });

    // 获取聊天记录
    dispatcher_.register_handler(MsgType::GET_CHAT_HISTORY, [this](std::shared_ptr<Session> session, const Message& msg) {
        std::cout << "[Server] 收到获取聊天记录请求: " << session->user_id() << std::endl;

        std::string friend_id;
        int limit = 20;
        int64_t before_time = 0;
        try {
            json::object req = parse_json_object(msg.body);
            friend_id = json_string(req, "friend_id");
            limit = static_cast<int>(json_int64(req, "limit", limit));
            before_time = json_int64(req, "before_time", before_time);
        } catch (const std::exception& e) {
            session->send(MsgType::CHAT_HISTORY_RSP, json_response(400, std::string("无效 JSON: ") + e.what()));
            return;
        }

        std::string history = user_service_.get_chat_history(session->user_id(), friend_id, limit, before_time);
        session->send(MsgType::CHAT_HISTORY_RSP, history);
    });

    dispatcher_.register_handler(MsgType::CREATE_MOMENT, [this](std::shared_ptr<Session> session, const Message& msg) {
        std::cout << "[Server] 收到朋友圈发布请求 from " << session->user_id()
                  << ", body_size=" << msg.body.size() << std::endl;

        if (session->user_id().empty()) {
            session->send(MsgType::CREATE_MOMENT_RSP, json_response(401, "未登录"));
            return;
        }

        std::string content;
        json::array images;
        try {
            json::object req = parse_json_object(msg.body);
            content = json_string(req, "content");
            images = json_array_value(req, "images");
            if (!json_string(req, "video_url").empty()) {
                session->send(MsgType::CREATE_MOMENT_RSP, json_response(400, "朋友圈不支持发布视频"));
                return;
            }
        } catch (const std::exception& e) {
            session->send(MsgType::CREATE_MOMENT_RSP, json_response(400, std::string("无效 JSON: ") + e.what()));
            return;
        }

        if (images.size() > 9) {
            session->send(MsgType::CREATE_MOMENT_RSP, json_response(400, "图片最多九张"));
            return;
        }

        std::string media_type = "text";
        std::string media_json;
        if (!images.empty()) {
            media_type = "image";
            media_json = json::serialize(images);
        }

        LoginResult result = user_service_.create_moment(
            session->user_id(), content, media_type, media_json);
        session->send(MsgType::CREATE_MOMENT_RSP, json_response(result.code, result.message));
    });

    dispatcher_.register_handler(MsgType::GET_MOMENTS, [this](std::shared_ptr<Session> session, const Message& msg) {
        if (session->user_id().empty()) {
            session->send(MsgType::MOMENTS_RSP, "[]");
            return;
        }

        int limit = 50;
        std::string target_user_id;
        try {
            json::object req = parse_json_object(msg.body);
            limit = static_cast<int>(json_int64(req, "limit", limit));
            target_user_id = json_string(req, "target_user_id");
        } catch (const std::exception&) {
            limit = 50;
        }

        const std::string feed = target_user_id.empty()
            ? user_service_.get_moments_feed(session->user_id(), limit)
            : user_service_.get_user_moments(session->user_id(), target_user_id, limit);
        session->send(MsgType::MOMENTS_RSP, feed);
    });

    // 发送好友请求
    dispatcher_.register_handler(MsgType::FRIEND_REQUEST, [this](std::shared_ptr<Session> session, const Message& msg) {
        std::cout << "[Server] 收到好友请求 from " << session->user_id() << ": " << msg.body << std::endl;

        std::string phone, remark;
        try {
            json::object req = parse_json_object(msg.body);
            phone = json_string(req, "phone");
            remark = json_string(req, "remark");
        } catch (const std::exception& e) {
            session->send(MsgType::FRIEND_REQUEST_RSP, json_response(400, std::string("无效 JSON: ") + e.what()));
            return;
        }

        // 根据手机号查找目标用户
        std::cout << "[Server] 查找用户 phone=" << phone << std::endl;
        UserInfo target_user = user_service_.get_user_by_phone(phone);
        std::cout << "[Server] 查找结果: user_id=" << target_user.user_id << std::endl;
        if (target_user.user_id.empty()) {
            session->send(MsgType::FRIEND_REQUEST_RSP, json_response(1, "用户不存在"));
            return;
        }

        // 获取当前用户信息
        UserInfo current_user = user_service_.get_user_by_id(session->user_id());

        std::cout << "[Server] 添加好友请求: from=" << session->user_id()
                  << ", to=" << target_user.user_id << ", remark=" << remark << std::endl;
        LoginResult result = user_service_.add_friend_request(
            session->user_id(), target_user.user_id, remark, current_user.nickname);

        std::cout << "[Server] 添加结果: code=" << result.code << ", message=" << result.message << std::endl;
        session->send(MsgType::FRIEND_REQUEST_RSP, json_response(result.code, result.message));
    });

    // 响应好友请求（同意/拒绝）
    dispatcher_.register_handler(MsgType::FRIEND_REQUEST_RSP, [this](std::shared_ptr<Session> session, const Message& msg) {
        std::cout << "[Server] 收到好友请求响应 from " << session->user_id() << ": " << msg.body << std::endl;

        std::string request_id;
        bool accept = false;
        try {
            json::object req = parse_json_object(msg.body);
            request_id = json_string(req, "request_id");
            accept = json_bool(req, "accept");
        } catch (const std::exception& e) {
            session->send(MsgType::FRIEND_REQUEST_RSP, json_response(400, std::string("无效 JSON: ") + e.what()));
            return;
        }

        std::cout << "[Server] 解析结果: request_id=" << request_id << ", accept=" << accept << std::endl;

        LoginResult result = user_service_.handle_friend_request(request_id, accept, session->user_id());

        session->send(MsgType::FRIEND_REQUEST_RSP, json_response(result.code, result.message));
    });

    // 删除好友
    dispatcher_.register_handler(MsgType::DELETE_FRIEND, [this](std::shared_ptr<Session> session, const Message& msg) {
        std::cout << "[Server] 收到删除好友请求 from " << session->user_id() << ": " << msg.body << std::endl;

        std::string friend_id;
        try {
            json::object req = parse_json_object(msg.body);
            friend_id = json_string(req, "friend_id");
        } catch (const std::exception& e) {
            session->send(MsgType::FRIEND_REQUEST_RSP, json_response(400, std::string("无效 JSON: ") + e.what()));
            return;
        }

        LoginResult result = user_service_.delete_friend(session->user_id(), friend_id);

        session->send(MsgType::FRIEND_REQUEST_RSP, json_response(result.code, result.message));
    });

    // 修改好友备注
    dispatcher_.register_handler(MsgType::UPDATE_FRIEND_REMARK, [this](std::shared_ptr<Session> session, const Message& msg) {
        std::cout << "[Server] 收到修改好友备注请求 from " << session->user_id() << ": " << msg.body << std::endl;

        if (session->user_id().empty()) {
            session->send(MsgType::UPDATE_FRIEND_REMARK_RSP, json_response(401, "未登录"));
            return;
        }

        std::string friend_id;
        std::string remark;
        try {
            json::object req = parse_json_object(msg.body);
            friend_id = json_string(req, "friend_id");
            remark = json_string(req, "remark");
        } catch (const std::exception& e) {
            session->send(MsgType::UPDATE_FRIEND_REMARK_RSP, json_response(400, std::string("无效 JSON: ") + e.what()));
            return;
        }

        LoginResult result = user_service_.update_friend_remark(session->user_id(), friend_id, remark);

        json::object rsp;
        rsp["code"] = result.code;
        rsp["message"] = result.message;
        rsp["friend_id"] = friend_id;
        rsp["remark"] = remark;
        session->send(MsgType::UPDATE_FRIEND_REMARK_RSP, json::serialize(rsp));
    });

    // 更新头像
    dispatcher_.register_handler(MsgType::UPDATE_AVATAR, [this](std::shared_ptr<Session> session, const Message& msg) {
        std::cout << "[Server] 收到更新头像请求 from " << session->user_id()
                  << ", body_size=" << msg.body.size() << std::endl;

        if (session->user_id().empty()) {
            session->send(MsgType::UPDATE_AVATAR_RSP, json_response(401, "未登录"));
            return;
        }

        std::string avatar_url;
        try {
            json::object req = parse_json_object(msg.body);
            avatar_url = json_string(req, "avatar_url");
        } catch (const std::exception& e) {
            session->send(MsgType::UPDATE_AVATAR_RSP, json_response(400, std::string("无效 JSON: ") + e.what()));
            return;
        }

        LoginResult result = user_service_.update_avatar(session->user_id(), avatar_url);

        json::object rsp;
        rsp["code"] = result.code;
        rsp["message"] = result.message;
        if (result.code == 0) {
            rsp["avatar_url"] = result.avatar_url;
        }
        session->send(MsgType::UPDATE_AVATAR_RSP, json::serialize(rsp));
    });

    // 更新个人信息
    dispatcher_.register_handler(MsgType::UPDATE_PROFILE, [this](std::shared_ptr<Session> session, const Message& msg) {
        std::cout << "[Server] 收到更新个人信息请求 from " << session->user_id() << std::endl;

        if (session->user_id().empty()) {
            session->send(MsgType::UPDATE_PROFILE_RSP, json_response(401, "未登录"));
            return;
        }

        std::string nickname;
        std::string gender;
        std::string region;
        std::string signature;
        try {
            json::object req = parse_json_object(msg.body);
            nickname = json_string(req, "nickname");
            gender = json_string(req, "gender");
            region = json_string(req, "region");
            signature = json_string(req, "signature");
        } catch (const std::exception& e) {
            session->send(MsgType::UPDATE_PROFILE_RSP, json_response(400, std::string("无效 JSON: ") + e.what()));
            return;
        }

        LoginResult result = user_service_.update_profile(session->user_id(), nickname, gender, region, signature);

        json::object rsp;
        rsp["code"] = result.code;
        rsp["message"] = result.message;
        if (result.code == 0) {
            rsp["nickname"] = result.nickname;
            rsp["gender"] = result.gender;
            rsp["region"] = result.region;
            rsp["signature"] = result.signature;
        }
        const std::string rsp_body = json::serialize(rsp);
        std::cout << "[Server] 更新个人信息响应: " << rsp_body << std::endl;
        session->send(MsgType::UPDATE_PROFILE_RSP, rsp_body);
    });

    // 修改密码
    dispatcher_.register_handler(MsgType::CHANGE_PASSWORD, [this](std::shared_ptr<Session> session, const Message& msg) {
        std::cout << "[Server] 收到修改密码请求 from " << session->user_id() << std::endl;

        if (session->user_id().empty()) {
            session->send(MsgType::CHANGE_PASSWORD_RSP, json_response(401, "未登录"));
            return;
        }

        std::string old_password;
        std::string new_password;
        try {
            json::object req = parse_json_object(msg.body);
            old_password = json_string(req, "old_password");
            new_password = json_string(req, "new_password");
        } catch (const std::exception& e) {
            session->send(MsgType::CHANGE_PASSWORD_RSP, json_response(400, std::string("无效 JSON: ") + e.what()));
            return;
        }

        LoginResult result = user_service_.change_password(session->user_id(), old_password, new_password);
        session->send(MsgType::CHANGE_PASSWORD_RSP, json_response(result.code, result.message));
    });

    dispatcher_.register_handler(MsgType::OFFLINE_MESSAGE_ACK, [this](std::shared_ptr<Session> session, const Message& msg) {
        if (session->user_id().empty()) {
            session->send(MsgType::ERROR, json_response(401, "未登录"));
            return;
        }

        std::string msg_id;
        try {
            json::object req = parse_json_object(msg.body);
            msg_id = json_string(req, "msg_id");
        } catch (const std::exception& e) {
            session->send(MsgType::ERROR, json_response(400, std::string("无效 JSON: ") + e.what()));
            return;
        }

        if (msg_id.empty()) {
            session->send(MsgType::ERROR, json_response(400, "缺少 msg_id"));
            return;
        }

        user_service_.mark_offline_messages_pushed(session->user_id(), msg_id);
    });

    dispatcher_.register_handler(MsgType::FILE_UPLOAD_START, [this](std::shared_ptr<Session> session, const Message& msg) {
        json::object rsp;
        rsp["status"] = "failed";

        if (session->user_id().empty()) {
            rsp["code"] = 401;
            rsp["message"] = "未登录";
            session->send(MsgType::FILE_UPLOAD_RSP, json::serialize(rsp));
            return;
        }

        json::object req;
        try {
            req = parse_json_object(msg.body);
        } catch (const std::exception& e) {
            rsp["code"] = 400;
            rsp["message"] = std::string("无效 JSON: ") + e.what();
            session->send(MsgType::FILE_UPLOAD_RSP, json::serialize(rsp));
            return;
        }

        constexpr uint64_t max_file_size = 200ULL * 1024ULL * 1024ULL;
        const std::string transfer_id = json_string(req, "transfer_id");
        const std::string to_user_id = json_string(req, "to_user_id");
        const std::string file_name = json_string(req, "file_name", "file");
        const std::string mime_type = json_string(req, "mime_type", "application/octet-stream");
        const uint64_t file_size = json_uint64(req, "file_size");
        const uint64_t total_chunks_64 = json_uint64(req, "total_chunks");

        rsp["transfer_id"] = transfer_id;
        if (transfer_id.empty() || to_user_id.empty() || file_size == 0 || total_chunks_64 == 0) {
            rsp["code"] = 400;
            rsp["message"] = "缺少文件上传参数";
            session->send(MsgType::FILE_UPLOAD_RSP, json::serialize(rsp));
            return;
        }
        if (file_size > max_file_size) {
            rsp["code"] = 413;
            rsp["message"] = "单个文件不能超过200MB";
            session->send(MsgType::FILE_UPLOAD_RSP, json::serialize(rsp));
            return;
        }
        if (!user_service_.user_exists(to_user_id) || !user_service_.are_friends(session->user_id(), to_user_id)) {
            rsp["code"] = 403;
            rsp["message"] = "只能给好友发送文件";
            session->send(MsgType::FILE_UPLOAD_RSP, json::serialize(rsp));
            return;
        }

        try {
            std::filesystem::create_directories(file_storage_dir());
        } catch (const std::exception& e) {
            rsp["code"] = 500;
            rsp["message"] = std::string("创建文件目录失败: ") + e.what();
            session->send(MsgType::FILE_UPLOAD_RSP, json::serialize(rsp));
            return;
        }

        const std::string file_id = random_hex_id();
        const std::filesystem::path temp_path = file_storage_dir() / (file_id + ".part");
        const std::filesystem::path final_path = file_storage_dir() / (file_id + ".bin");
        {
            std::ofstream create_file(temp_path, std::ios::binary | std::ios::trunc);
            if (!create_file) {
                rsp["code"] = 500;
                rsp["message"] = "创建临时文件失败";
                session->send(MsgType::FILE_UPLOAD_RSP, json::serialize(rsp));
                return;
            }
        }

        FileUploadState state;
        state.transfer_id = transfer_id;
        state.file_id = file_id;
        state.from_user_id = session->user_id();
        state.to_user_id = to_user_id;
        state.file_name = file_name;
        state.mime_type = mime_type;
        state.file_size = file_size;
        state.total_chunks = static_cast<uint32_t>(total_chunks_64);
        state.temp_path = temp_path.string();
        state.final_path = final_path.string();

        {
            std::lock_guard<std::mutex> lock(file_upload_mutex_);
            file_uploads_[transfer_id] = state;
        }

        rsp["code"] = 0;
        rsp["status"] = "ready";
        rsp["message"] = "可以上传";
        rsp["transfer_id"] = transfer_id;
        rsp["file_id"] = file_id;
        rsp["next_chunk_index"] = 0;
        session->send(MsgType::FILE_UPLOAD_RSP, json::serialize(rsp));
    });

    dispatcher_.register_handler(MsgType::FILE_UPLOAD_CHUNK, [this](std::shared_ptr<Session> session, const Message& msg) {
        json::object rsp;
        rsp["status"] = "failed";

        json::object req;
        try {
            req = parse_json_object(msg.body);
        } catch (const std::exception& e) {
            rsp["code"] = 400;
            rsp["message"] = std::string("无效 JSON: ") + e.what();
            session->send(MsgType::FILE_UPLOAD_RSP, json::serialize(rsp));
            return;
        }

        const std::string transfer_id = json_string(req, "transfer_id");
        const uint64_t chunk_index_64 = json_uint64(req, "chunk_index");
        rsp["transfer_id"] = transfer_id;

        FileUploadState state;
        {
            std::lock_guard<std::mutex> lock(file_upload_mutex_);
            auto it = file_uploads_.find(transfer_id);
            if (it == file_uploads_.end()) {
                rsp["code"] = 404;
                rsp["message"] = "上传任务不存在";
                session->send(MsgType::FILE_UPLOAD_RSP, json::serialize(rsp));
                return;
            }
            state = it->second;
        }

        if (state.from_user_id != session->user_id()) {
            rsp["code"] = 403;
            rsp["message"] = "无权上传该文件";
            session->send(MsgType::FILE_UPLOAD_RSP, json::serialize(rsp));
            return;
        }
        if (chunk_index_64 != state.next_chunk_index) {
            rsp["code"] = 409;
            rsp["message"] = "分片顺序不正确";
            rsp["next_chunk_index"] = state.next_chunk_index;
            session->send(MsgType::FILE_UPLOAD_RSP, json::serialize(rsp));
            return;
        }

        std::string chunk;
        try {
            chunk = base64_decode(json_string(req, "data"));
        } catch (const std::exception& e) {
            rsp["code"] = 400;
            rsp["message"] = std::string("分片解码失败: ") + e.what();
            session->send(MsgType::FILE_UPLOAD_RSP, json::serialize(rsp));
            return;
        }

        if (state.received_size + chunk.size() > state.file_size) {
            rsp["code"] = 413;
            rsp["message"] = "上传内容超过声明大小";
            session->send(MsgType::FILE_UPLOAD_RSP, json::serialize(rsp));
            return;
        }

        {
            std::ofstream out(state.temp_path, std::ios::binary | std::ios::app);
            if (!out) {
                rsp["code"] = 500;
                rsp["message"] = "写入文件失败";
                session->send(MsgType::FILE_UPLOAD_RSP, json::serialize(rsp));
                return;
            }
            out.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
        }

        state.received_size += chunk.size();
        ++state.next_chunk_index;

        const bool complete = state.next_chunk_index >= state.total_chunks
            && state.received_size == state.file_size;
        if (complete) {
            try {
                std::filesystem::rename(state.temp_path, state.final_path);
            } catch (const std::exception& e) {
                rsp["code"] = 500;
                rsp["message"] = std::string("保存文件失败: ") + e.what();
                session->send(MsgType::FILE_UPLOAD_RSP, json::serialize(rsp));
                return;
            }

            {
                std::lock_guard<std::mutex> lock(file_upload_mutex_);
                file_uploads_.erase(transfer_id);
            }

            rsp["code"] = 0;
            rsp["status"] = "complete";
            rsp["message"] = "上传完成";
            rsp["file_id"] = state.file_id;
            rsp["file_name"] = state.file_name;
            rsp["file_size"] = state.file_size;
            rsp["mime_type"] = state.mime_type;
            session->send(MsgType::FILE_UPLOAD_RSP, json::serialize(rsp));
            return;
        }

        {
            std::lock_guard<std::mutex> lock(file_upload_mutex_);
            file_uploads_[transfer_id] = state;
        }

        rsp["code"] = 0;
        rsp["status"] = "chunk";
        rsp["message"] = "分片已接收";
        rsp["next_chunk_index"] = state.next_chunk_index;
        rsp["received_size"] = state.received_size;
        session->send(MsgType::FILE_UPLOAD_RSP, json::serialize(rsp));
    });

    dispatcher_.register_handler(MsgType::FILE_DOWNLOAD_REQ, [this](std::shared_ptr<Session> session, const Message& msg) {
        json::object rsp;
        rsp["status"] = "failed";

        if (session->user_id().empty()) {
            rsp["code"] = 401;
            rsp["message"] = "未登录";
            session->send(MsgType::FILE_DOWNLOAD_RSP, json::serialize(rsp));
            return;
        }

        json::object req;
        try {
            req = parse_json_object(msg.body);
        } catch (const std::exception& e) {
            rsp["code"] = 400;
            rsp["message"] = std::string("无效 JSON: ") + e.what();
            session->send(MsgType::FILE_DOWNLOAD_RSP, json::serialize(rsp));
            return;
        }

        const std::string transfer_id = json_string(req, "transfer_id", random_hex_id());
        const std::string file_id = safe_file_name(json_string(req, "file_id"));
        const std::string file_name = json_string(req, "file_name", "file");
        const std::filesystem::path path = file_storage_dir() / (file_id + ".bin");
        rsp["transfer_id"] = transfer_id;
        rsp["file_id"] = file_id;

        if (file_id.empty() || !std::filesystem::exists(path)) {
            rsp["code"] = 404;
            rsp["message"] = "文件不存在或已过期";
            session->send(MsgType::FILE_DOWNLOAD_RSP, json::serialize(rsp));
            return;
        }

        std::ifstream in(path, std::ios::binary);
        if (!in) {
            rsp["code"] = 500;
            rsp["message"] = "打开文件失败";
            session->send(MsgType::FILE_DOWNLOAD_RSP, json::serialize(rsp));
            return;
        }

        constexpr std::size_t chunk_size = 256 * 1024;
        const uint64_t file_size = static_cast<uint64_t>(std::filesystem::file_size(path));
        const uint64_t total_chunks = (file_size + chunk_size - 1) / chunk_size;

        rsp["code"] = 0;
        rsp["status"] = "ready";
        rsp["message"] = "开始下载";
        rsp["file_name"] = file_name;
        rsp["file_size"] = file_size;
        rsp["total_chunks"] = total_chunks;
        session->send(MsgType::FILE_DOWNLOAD_RSP, json::serialize(rsp));

        std::vector<char> buffer(chunk_size);
        uint64_t chunk_index = 0;
        while (in && chunk_index < total_chunks) {
            in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const std::streamsize bytes_read = in.gcount();
            if (bytes_read <= 0) break;

            json::object chunk_rsp;
            chunk_rsp["code"] = 0;
            chunk_rsp["transfer_id"] = transfer_id;
            chunk_rsp["file_id"] = file_id;
            chunk_rsp["file_name"] = file_name;
            chunk_rsp["file_size"] = file_size;
            chunk_rsp["chunk_index"] = chunk_index;
            chunk_rsp["total_chunks"] = total_chunks;
            chunk_rsp["data"] = base64_encode(std::string(buffer.data(), static_cast<std::size_t>(bytes_read)));
            session->send(MsgType::FILE_DOWNLOAD_CHUNK, json::serialize(chunk_rsp));
            ++chunk_index;
        }

        json::object done_rsp;
        done_rsp["code"] = 0;
        done_rsp["status"] = "complete";
        done_rsp["message"] = "下载完成";
        done_rsp["transfer_id"] = transfer_id;
        done_rsp["file_id"] = file_id;
        done_rsp["file_name"] = file_name;
        session->send(MsgType::FILE_DOWNLOAD_RSP, json::serialize(done_rsp));
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

    auto chat_handler = [this](std::shared_ptr<Session> session, const Message& msg) {
        std::cout << "[Server] 收到聊天消息 type=0x" << std::hex
                  << static_cast<uint16_t>(msg.type) << std::dec
                  << " from " << session->user_id() << ": " << msg.body << std::endl;

        json::object payload;
        try {
            payload = parse_json_object(msg.body);
        } catch (const std::exception& e) {
            session->send(MsgType::ACK, message_ack("", "failed", 400, std::string("无效 JSON: ") + e.what()));
            return;
        }

        std::string msg_id = json_string(payload, "msg_id");
        std::string to_user_id = json_string(payload, "to_user_id");
        std::string content_type = json_string(payload, "content_type", default_content_type(msg.type));
        std::string content = json_string(payload, "content");
        int64_t client_time = json_int64(payload, "client_time", static_cast<int64_t>(time(nullptr)));

        if (session->user_id().empty()) {
            session->send(MsgType::ACK, message_ack(msg_id, "failed", 401, "未登录"));
            return;
        }

        if (content.empty()) {
            auto content_it = payload.find("content");
            if (content_it != payload.end() && !content_it->value().is_null()) {
                content = json::serialize(content_it->value());
            } else {
                content = json::serialize(payload);
            }
        }

        if (msg_id.empty() || to_user_id.empty()) {
            session->send(MsgType::ACK, message_ack(msg_id, "failed", 400, "缺少 msg_id 或 to_user_id"));
            return;
        }

        if (!user_service_.user_exists(to_user_id)) {
            session->send(MsgType::ACK, message_ack(msg_id, "failed", 404, "目标用户不存在"));
            return;
        }

        if (!user_service_.are_friends(session->user_id(), to_user_id)) {
            session->send(MsgType::ACK, message_ack(msg_id, "failed", 403, "只能给好友发送消息"));
            return;
        }

        if (user_service_.is_blocked(to_user_id, session->user_id())
            || user_service_.is_blocked(session->user_id(), to_user_id)) {
            session->send(MsgType::ACK, message_ack(msg_id, "failed", 403, "消息被拦截"));
            return;
        }

        const int64_t server_timestamp = current_time_millis();
        payload["from_user_id"] = session->user_id();
        payload["content_type"] = content_type;
        payload["client_time"] = client_time;
        payload["server_timestamp"] = server_timestamp;
        const std::string forward_body = json::serialize(payload);
        const int msg_type_code = chat_content_type_code(content_type);

        if (!user_service_.save_message(
                msg_id, msg_type_code, 1, session->user_id(), to_user_id, content_type, content, client_time)) {
            session->send(MsgType::ACK, message_ack(msg_id, "failed", 500, "消息保存失败"));
            return;
        }

        // 聊天消息统一使用 CHAT_MESSAGE(0x0005) 传输，具体媒体类型由 content_type 区分。
        if (send_to_user(to_user_id, MsgType::CHAT_MESSAGE, forward_body)) {
            session->send(MsgType::ACK, message_ack(msg_id, "delivered", 0, "消息已送达"));
            return;
        }

        std::cout << "[Server] 用户不在线，保存离线消息: " << to_user_id << std::endl;
        auto conn_guard = db_pool_.get_connection();
        MYSQL* mysql = conn_guard.get();
        if (mysql) {
            std::ostringstream sql;
            sql << "INSERT IGNORE INTO im_offline_message (user_id, msg_id, msg_type, chat_type, "
                << "from_user_id, to_user_id, content, client_time, server_time) "
                << "VALUES ('" << sql_escape(mysql, to_user_id) << "', "
                << "'" << sql_escape(mysql, msg_id) << "', " << msg_type_code << ", 1, "
                << "'" << sql_escape(mysql, session->user_id()) << "', "
                << "'" << sql_escape(mysql, to_user_id) << "', "
                << "'" << sql_escape(mysql, content) << "', "
                << "FROM_UNIXTIME(" << client_time << "), NOW())";
            if (mysql_query(mysql, sql.str().c_str())) {
                std::cerr << "[Server] 保存离线消息失败: " << mysql_error(mysql) << std::endl;
                session->send(MsgType::ACK, message_ack(msg_id, "failed", 500, "离线消息保存失败"));
                return;
            }
        } else {
            session->send(MsgType::ACK, message_ack(msg_id, "failed", 500, "数据库连接失败"));
            return;
        }

        session->send(MsgType::ACK, message_ack(msg_id, "sent", 0, "已保存为离线消息"));
    };

    dispatcher_.register_handler(MsgType::CHAT_MESSAGE, chat_handler);
    dispatcher_.register_handler(MsgType::IMAGE, chat_handler);
    dispatcher_.register_handler(MsgType::FILE, chat_handler);
    dispatcher_.register_handler(MsgType::VOICE, chat_handler);

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
        auto it = sessions_.find(session->user_id());
        if (it != sessions_.end() && it->second == session) {
            sessions_.erase(it);
        }
    }
    for (auto it = endpoints_.begin(); it != endpoints_.end(); ) {
        if (it->second == session) {
            it = endpoints_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace im
