/**
 * @file user_service.cpp
 * @brief 用户服务实现
 */

#include "user_service.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>
#include <chrono>
#include <cstring>
#include <boost/json.hpp>

namespace im {

namespace {

namespace json = boost::json;

std::string row_string(MYSQL_ROW row, int index, const std::string& default_value = "") {
    return row[index] ? row[index] : default_value;
}

int row_int(MYSQL_ROW row, int index, int default_value = 0) {
    return row[index] ? std::stoi(row[index]) : default_value;
}

std::string sql_escape(MYSQL* mysql, const std::string& value) {
    std::string escaped;
    escaped.resize(value.size() * 2 + 1);
    unsigned long length = mysql_real_escape_string(
        mysql, escaped.data(), value.data(), static_cast<unsigned long>(value.size()));
    escaped.resize(length);
    return escaped;
}

} // namespace

std::string UserService::generate_token(const std::string& user_id) {
    // 简单的 token 生成，实际应用应使用 JWT
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();

    std::ostringstream oss;
    oss << user_id << "_" << timestamp << "_"
        << std::hex << std::setw(16) << std::setfill('0')
        << std::hash<std::string>{}(user_id + std::to_string(timestamp));

    return oss.str();
}

std::string UserService::generate_salt() {
    static const char kChars[] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(kChars) - 2);

    std::ostringstream oss;
    for (int i = 0; i < 32; ++i) {
        oss << kChars[dis(gen)];
    }
    return oss.str();
}

std::string UserService::hash_password(const std::string& password,
                                      const std::string& salt) {
    // 简单哈希，实际应用应使用 bcrypt 或 Argon2
    std::string combined = password + salt;

    // 简化：使用简单的字符串哈希
    size_t h = 0;
    for (unsigned char c : combined) {
        h = h * 31 + c;
    }

    std::ostringstream result;
    result << std::hex << std::setw(8) << std::setfill('0') << h;
    result << salt;  // 附加盐值

    return result.str();
}

std::string UserService::generate_user_id() {
    static int counter = 0;
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    std::ostringstream oss;
    oss << "u" << timestamp << "_" << (++counter);
    return oss.str();
}

LoginResult UserService::login(const std::string& user_id,
                               const std::string& password) {
    LoginResult result;

    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();

    if (!mysql) {
        result.code = 5001;
        result.message = "数据库连接失败";
        return result;
    }

    // 查询用户
    const std::string escaped_user_id = sql_escape(mysql, user_id);
    std::ostringstream sql;
    sql << "SELECT user_id, nickname, avatar_url, password_hash, salt, status "
        << "FROM im_user WHERE user_id = '" << escaped_user_id << "' OR phone = '" << escaped_user_id << "'";

    if (mysql_query(mysql, sql.str().c_str())) {
        result.code = 5001;
        result.message = "查询失败: " + std::string(mysql_error(mysql));
        return result;
    }

    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res) {
        result.code = 5001;
        result.message = "获取结果失败";
        return result;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row) {
        result.code = 1001;
        result.message = "用户不存在";
        mysql_free_result(res);
        return result;
    }

    std::string db_user_id = row[0] ? row[0] : "";
    std::string db_nickname = row[1] ? row[1] : "";
    std::string db_avatar = row[2] ? row[2] : "";
    std::string db_password_hash = row[3] ? row[3] : "";
    std::string db_salt = row[4] ? row[4] : "";
    int db_status = row[5] ? std::stoi(row[5]) : 0;

    mysql_free_result(res);

    // 检查用户状态
    if (db_status == 0) {
        result.code = 1001;
        result.message = "用户已被禁用";
        return result;
    }

    // 验证密码
    std::string input_hash = hash_password(password, db_salt);
    if (input_hash != db_password_hash) {
        result.code = 1001;
        result.message = "密码错误";
        return result;
    }

    // 登录成功
    result.code = 0;
    result.message = "登录成功";
    result.user_id = db_user_id;
    result.nickname = db_nickname;
    result.avatar_url = db_avatar;
    result.token = generate_token(db_user_id);

    std::cout << "[UserService] 用户登录成功: " << db_user_id << std::endl;

    return result;
}

LoginResult UserService::register_user(const std::string& phone,
                                       const std::string& nickname,
                                       const std::string& password) {
    LoginResult result;

    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();

    if (!mysql) {
        result.code = 5001;
        result.message = "数据库连接失败";
        return result;
    }

    // 检查手机号是否已注册
    const std::string escaped_phone = sql_escape(mysql, phone);
    std::ostringstream check_sql;
    check_sql << "SELECT user_id FROM im_user WHERE phone = '" << escaped_phone << "'";

    if (mysql_query(mysql, check_sql.str().c_str())) {
        result.code = 5001;
        result.message = "查询失败";
        return result;
    }

    MYSQL_RES* res = mysql_store_result(mysql);
    if (res && mysql_num_rows(res) > 0) {
        result.code = 1004;
        result.message = "手机号已被注册";
        mysql_free_result(res);
        return result;
    }
    if (res) mysql_free_result(res);

    // 生成用户
    // 优先使用手机号作为 user_id，否则生成随机 ID
    std::string user_id = phone.empty() ? generate_user_id() : phone;
    std::string salt = generate_salt();
    std::string password_hash = hash_password(password, salt);

    // 插入用户
    const std::string escaped_user_id = sql_escape(mysql, user_id);
    const std::string escaped_nickname = sql_escape(mysql, nickname);
    const std::string escaped_password_hash = sql_escape(mysql, password_hash);
    const std::string escaped_salt = sql_escape(mysql, salt);
    std::ostringstream insert_sql;
    insert_sql << "INSERT INTO im_user (user_id, phone, nickname, password_hash, salt, status) "
               << "VALUES ('" << escaped_user_id << "', '" << escaped_phone << "', '" << escaped_nickname << "', '"
               << escaped_password_hash << "', '" << escaped_salt << "', 1)";

    if (mysql_query(mysql, insert_sql.str().c_str())) {
        result.code = 5001;
        result.message = "注册失败: " + std::string(mysql_error(mysql));
        return result;
    }

    // 初始化用户统计
    std::ostringstream stats_sql;
    stats_sql << "INSERT INTO im_user_stats (user_id) VALUES ('" << escaped_user_id << "')";
    mysql_query(mysql, stats_sql.str().c_str());

    result.code = 0;
    result.message = "注册成功";
    result.user_id = user_id;
    result.nickname = nickname;
    result.token = generate_token(user_id);

    std::cout << "[UserService] 用户注册成功: " << user_id << std::endl;

    return result;
}

UserInfo UserService::get_user_by_id(const std::string& user_id) {
    UserInfo info;

    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();

    if (!mysql) return info;

    std::ostringstream sql;
    sql << "SELECT user_id, phone, email, nickname, avatar_url, status, user_type, create_time "
        << "FROM im_user WHERE user_id = '" << sql_escape(mysql, user_id) << "'";

    if (mysql_query(mysql, sql.str().c_str())) return info;

    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res) return info;

    MYSQL_ROW row = mysql_fetch_row(res);
    if (row) {
        info.user_id = row[0] ? row[0] : "";
        info.phone = row[1] ? row[1] : "";
        info.email = row[2] ? row[2] : "";
        info.nickname = row[3] ? row[3] : "";
        info.avatar_url = row[4] ? row[4] : "";
        info.status = row[5] ? std::stoi(row[5]) : 0;
        info.user_type = row[6] ? std::stoi(row[6]) : 1;
        info.create_time = row[7] ? row[7] : "";
    }

    mysql_free_result(res);
    return info;
}

UserInfo UserService::get_user_by_phone(const std::string& phone) {
    UserInfo info;

    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();

    if (!mysql) return info;

    std::ostringstream sql;
    sql << "SELECT user_id, phone, email, nickname, avatar_url, status, user_type, create_time "
        << "FROM im_user WHERE phone = '" << sql_escape(mysql, phone) << "'";

    if (mysql_query(mysql, sql.str().c_str())) return info;

    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res) return info;

    MYSQL_ROW row = mysql_fetch_row(res);
    if (row) {
        info.user_id = row[0] ? row[0] : "";
        info.phone = row[1] ? row[1] : "";
        info.email = row[2] ? row[2] : "";
        info.nickname = row[3] ? row[3] : "";
        info.avatar_url = row[4] ? row[4] : "";
        info.status = row[5] ? std::stoi(row[5]) : 0;
        info.user_type = row[6] ? std::stoi(row[6]) : 1;
        info.create_time = row[7] ? row[7] : "";
    }

    mysql_free_result(res);
    return info;
}

bool UserService::user_exists(const std::string& user_id) {
    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();

    if (!mysql) return false;

    std::ostringstream sql;
    sql << "SELECT 1 FROM im_user WHERE user_id = '" << sql_escape(mysql, user_id) << "' LIMIT 1";

    if (mysql_query(mysql, sql.str().c_str())) return false;

    MYSQL_RES* res = mysql_store_result(mysql);
    bool exists = res && mysql_num_rows(res) > 0;

    if (res) mysql_free_result(res);
    return exists;
}

bool UserService::are_friends(const std::string& user_id, const std::string& friend_id) {
    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();

    if (!mysql) return false;

    std::ostringstream sql;
    sql << "SELECT 1 FROM im_friend WHERE user_id = '" << sql_escape(mysql, user_id)
        << "' AND friend_id = '" << sql_escape(mysql, friend_id)
        << "' AND status = 1 LIMIT 1";

    if (mysql_query(mysql, sql.str().c_str())) return false;

    MYSQL_RES* res = mysql_store_result(mysql);
    bool exists = res && mysql_num_rows(res) > 0;

    if (res) mysql_free_result(res);
    return exists;
}

bool UserService::is_blocked(const std::string& user_id, const std::string& block_user_id) {
    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();

    if (!mysql) return true;

    std::ostringstream sql;
    sql << "SELECT 1 FROM im_blacklist WHERE user_id = '" << sql_escape(mysql, user_id)
        << "' AND block_user_id = '" << sql_escape(mysql, block_user_id)
        << "' LIMIT 1";

    if (mysql_query(mysql, sql.str().c_str())) return true;

    MYSQL_RES* res = mysql_store_result(mysql);
    bool blocked = res && mysql_num_rows(res) > 0;

    if (res) mysql_free_result(res);
    return blocked;
}

void UserService::update_login_info(const std::string& user_id,
                                    const std::string& ip) {
    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();

    if (!mysql) return;

    std::ostringstream sql;
    sql << "UPDATE im_user SET last_login_time = NOW(), last_login_ip = '" << sql_escape(mysql, ip)
        << "' WHERE user_id = '" << sql_escape(mysql, user_id) << "'";

    mysql_query(mysql, sql.str().c_str());
}

std::string UserService::get_friend_list(const std::string& user_id) {
    json::array result;

    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();
    if (!mysql) return json::serialize(result);

    std::ostringstream query;
    const std::string escaped_user_id = sql_escape(mysql, user_id);
    query << "SELECT f.friend_id, f.remark, f.friend_nickname, f.friend_avatar, u.status, "
          << "(SELECT m.content FROM im_message m "
          << " WHERE ((m.from_user_id = '" << escaped_user_id << "' AND m.to_user_id = f.friend_id) "
          << " OR (m.from_user_id = f.friend_id AND m.to_user_id = '" << escaped_user_id << "')) "
          << " ORDER BY m.server_time DESC LIMIT 1) AS last_msg_content, "
          << "(SELECT DATE_FORMAT(m.server_time, '%Y-%m-%d %H:%i:%s') FROM im_message m "
          << " WHERE ((m.from_user_id = '" << escaped_user_id << "' AND m.to_user_id = f.friend_id) "
          << " OR (m.from_user_id = f.friend_id AND m.to_user_id = '" << escaped_user_id << "')) "
          << " ORDER BY m.server_time DESC LIMIT 1) AS last_msg_time "
          << "FROM im_friend f "
          << "LEFT JOIN im_user u ON f.friend_id = u.user_id "
          << "WHERE f.user_id = '" << escaped_user_id << "' AND f.status = 1 "
          << "ORDER BY last_msg_time DESC, f.friend_nickname ASC";

    if (mysql_query(mysql, query.str().c_str())) {
        return json::serialize(result);
    }

    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res) return json::serialize(result);

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        json::object item;
        item["friend_id"] = row_string(row, 0);
        item["remark"] = row_string(row, 1);
        item["nickname"] = row_string(row, 2);
        item["avatar_url"] = row_string(row, 3);
        item["status"] = row_int(row, 4);
        item["last_msg_content"] = row_string(row, 5);
        item["last_msg_time"] = row_string(row, 6);
        result.push_back(std::move(item));
    }

    mysql_free_result(res);
    return json::serialize(result);
}

std::string UserService::get_friend_requests(const std::string& user_id, int status) {
    json::array result;

    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();
    if (!mysql) return json::serialize(result);

    std::ostringstream query;
    query << "SELECT request_id, from_user_id, from_nickname, from_avatar, remark, create_time "
          << "FROM im_friend_request "
          << "WHERE to_user_id = '" << sql_escape(mysql, user_id) << "' AND status = " << status
          << " ORDER BY create_time DESC";

    if (mysql_query(mysql, query.str().c_str())) {
        return json::serialize(result);
    }

    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res) return json::serialize(result);

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        json::object item;
        item["request_id"] = row_string(row, 0);
        item["from_user_id"] = row_string(row, 1);
        item["from_nickname"] = row_string(row, 2);
        item["from_avatar"] = row_string(row, 3);
        item["remark"] = row_string(row, 4);
        item["create_time"] = row_string(row, 5);
        result.push_back(std::move(item));
    }

    mysql_free_result(res);
    return json::serialize(result);
}

LoginResult UserService::add_friend_request(const std::string& from_user_id,
                                             const std::string& to_user_id,
                                             const std::string& remark,
                                             const std::string& from_nickname) {
    LoginResult result;

    // 不能添加自己为好友
    if (from_user_id == to_user_id) {
        result.code = 1;
        result.message = "不能添加自己为好友";
        return result;
    }

    // 检查目标用户是否存在
    if (!user_exists(to_user_id)) {
        result.code = 2;
        result.message = "用户不存在";
        return result;
    }

    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();
    if (!mysql) {
        std::cerr << "[UserService] add_friend_request: 数据库连接失败" << std::endl;
        result.code = 5001;
        result.message = "数据库连接失败";
        return result;
    }

    // 生成请求ID
    std::string request_id = std::to_string(time(nullptr)) + "_" + generate_token(from_user_id).substr(0, 16);

    // 插入好友请求
    std::ostringstream insert_sql;
    insert_sql << "INSERT INTO im_friend_request (request_id, from_user_id, from_nickname, to_user_id, remark, status, expire_time) "
               << "VALUES ('" << sql_escape(mysql, request_id) << "', '" << sql_escape(mysql, from_user_id)
               << "', '" << sql_escape(mysql, from_nickname) << "', '" << sql_escape(mysql, to_user_id)
               << "', '" << sql_escape(mysql, remark) << "', 0, DATE_ADD(NOW(), INTERVAL 7 DAY))";

    if (mysql_query(mysql, insert_sql.str().c_str())) {
        std::cerr << "[UserService] MySQL错误: " << mysql_error(mysql) << std::endl;
        std::cerr << "[UserService] SQL: " << insert_sql.str() << std::endl;
        result.code = 5001;
        result.message = "发送好友请求失败";
        return result;
    }

    result.code = 0;
    result.message = "好友请求已发送";
    return result;
}

LoginResult UserService::handle_friend_request(const std::string& request_id,
                                               bool accept,
                                               const std::string& user_id) {
    LoginResult result;

    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();
    if (!mysql) {
        result.code = 5001;
        result.message = "数据库连接失败";
        return result;
    }

    // 先查询请求信息
    std::ostringstream select_sql;
    select_sql << "SELECT from_user_id, to_user_id, from_nickname, remark FROM im_friend_request "
               << "WHERE request_id = '" << sql_escape(mysql, request_id)
               << "' AND to_user_id = '" << sql_escape(mysql, user_id) << "' AND status = 0";

    if (mysql_query(mysql, select_sql.str().c_str())) {
        result.code = 5001;
        result.message = "查询请求失败";
        return result;
    }

    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res || mysql_num_rows(res) == 0) {
        result.code = 1;
        result.message = "请求不存在或已处理";
        if (res) mysql_free_result(res);
        return result;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    std::string from_user_id = row[0] ? row[0] : "";
    std::string from_nickname = row[2] ? row[2] : "";
    std::string remark = row[3] ? row[3] : "";
    mysql_free_result(res);

    // 更新请求状态
    std::ostringstream update_sql;
    update_sql << "UPDATE im_friend_request SET status = " << (accept ? 1 : 2)
               << ", handle_time = NOW() WHERE request_id = '" << sql_escape(mysql, request_id) << "'";
    mysql_query(mysql, update_sql.str().c_str());

    if (accept) {
        // 双向添加好友记录
        // 获取对方用户信息
        UserInfo from_user = get_user_by_id(from_user_id);
        UserInfo to_user = get_user_by_id(user_id);

        // A添加B，A的friend_id是B，B的friend_id是A
        std::ostringstream insert_sql1;
        insert_sql1 << "INSERT INTO im_friend (user_id, friend_id, remark, friend_nickname, friend_avatar) "
                    << "VALUES ('" << sql_escape(mysql, from_user_id) << "', '" << sql_escape(mysql, user_id)
                    << "', '" << sql_escape(mysql, remark) << "', '" << sql_escape(mysql, to_user.nickname)
                    << "', '" << sql_escape(mysql, to_user.avatar_url) << "')";
        mysql_query(mysql, insert_sql1.str().c_str());

        std::ostringstream insert_sql2;
        insert_sql2 << "INSERT INTO im_friend (user_id, friend_id, remark, friend_nickname, friend_avatar) "
                    << "VALUES ('" << sql_escape(mysql, user_id) << "', '" << sql_escape(mysql, from_user_id)
                    << "', '', '" << sql_escape(mysql, from_nickname) << "', '" << sql_escape(mysql, from_user.avatar_url) << "')";
        mysql_query(mysql, insert_sql2.str().c_str());

        result.code = 0;
        result.message = "已同意好友请求";
    } else {
        result.code = 0;
        result.message = "已拒绝好友请求";
    }

    return result;
}

LoginResult UserService::delete_friend(const std::string& user_id, const std::string& friend_id) {
    LoginResult result;

    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();
    if (!mysql) {
        result.code = 5001;
        result.message = "数据库连接失败";
        return result;
    }

    // 双向删除好友记录
    std::ostringstream sql;
    sql << "DELETE FROM im_friend WHERE (user_id = '" << sql_escape(mysql, user_id)
        << "' AND friend_id = '" << sql_escape(mysql, friend_id) << "') "
        << "OR (user_id = '" << sql_escape(mysql, friend_id)
        << "' AND friend_id = '" << sql_escape(mysql, user_id) << "')";

    if (mysql_query(mysql, sql.str().c_str())) {
        result.code = 5001;
        result.message = "删除好友失败";
        return result;
    }

    result.code = 0;
    result.message = "已删除好友";
    return result;
}

LoginResult UserService::update_friend_remark(const std::string& user_id,
                                              const std::string& friend_id,
                                              const std::string& remark) {
    LoginResult result;

    if (friend_id.empty()) {
        result.code = 1;
        result.message = "好友ID不能为空";
        return result;
    }

    if (remark.size() > 128) {
        result.code = 2;
        result.message = "备注不能超过128个字符";
        return result;
    }

    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();
    if (!mysql) {
        result.code = 5001;
        result.message = "数据库连接失败";
        return result;
    }

    std::ostringstream exists_sql;
    exists_sql << "SELECT 1 FROM im_friend WHERE user_id = '" << sql_escape(mysql, user_id)
               << "' AND friend_id = '" << sql_escape(mysql, friend_id)
               << "' AND status = 1 LIMIT 1";

    if (mysql_query(mysql, exists_sql.str().c_str())) {
        result.code = 5001;
        result.message = "查询好友关系失败";
        return result;
    }

    MYSQL_RES* res = mysql_store_result(mysql);
    bool exists = res && mysql_num_rows(res) > 0;
    if (res) mysql_free_result(res);
    if (!exists) {
        result.code = 3;
        result.message = "好友不存在";
        return result;
    }

    std::ostringstream sql;
    sql << "UPDATE im_friend SET remark = '" << sql_escape(mysql, remark)
        << "', update_time = NOW() "
        << "WHERE user_id = '" << sql_escape(mysql, user_id)
        << "' AND friend_id = '" << sql_escape(mysql, friend_id)
        << "' AND status = 1";

    if (mysql_query(mysql, sql.str().c_str())) {
        std::cerr << "[UserService] 修改好友备注失败: " << mysql_error(mysql) << std::endl;
        result.code = 5001;
        result.message = "修改备注失败";
        return result;
    }

    result.code = 0;
    result.message = "备注已更新";
    return result;
}

bool UserService::save_message(const std::string& msg_id, int msg_type, int chat_type,
                               const std::string& from_user_id, const std::string& to_user_id,
                               const std::string& content_type, const std::string& content,
                               int64_t client_time) {
    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();
    if (!mysql) return false;

    std::ostringstream sql;
    sql << "INSERT INTO im_message (msg_id, msg_type, chat_type, from_user_id, to_user_id, "
        << "content_type, content, client_time, server_time, status) "
        << "VALUES ('" << sql_escape(mysql, msg_id) << "', " << msg_type << ", " << chat_type << ", "
        << "'" << sql_escape(mysql, from_user_id) << "', '" << sql_escape(mysql, to_user_id) << "', "
        << "'" << sql_escape(mysql, content_type) << "', '" << sql_escape(mysql, content) << "', "
        << "FROM_UNIXTIME(" << client_time << "), NOW(), 1)";

    if (mysql_query(mysql, sql.str().c_str())) {
        std::cerr << "[UserService] 保存消息失败: " << mysql_error(mysql) << std::endl;
        return false;
    }
    return true;
}

std::string UserService::get_offline_messages(const std::string& user_id) {
    json::array result;

    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();
    if (!mysql) return json::serialize(result);

    std::ostringstream query;
    query << "SELECT msg_id, msg_type, chat_type, from_user_id, content, client_time, server_time "
          << "FROM im_offline_message "
          << "WHERE user_id = '" << sql_escape(mysql, user_id) << "' AND is_pushed = 0 "
          << "ORDER BY create_time ASC LIMIT 100";

    if (mysql_query(mysql, query.str().c_str())) {
        return json::serialize(result);
    }

    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res) return json::serialize(result);

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        json::object item;
        item["msg_id"] = row_string(row, 0);
        item["msg_type"] = row_int(row, 1, 1);
        item["chat_type"] = row_int(row, 2, 1);
        item["from_user_id"] = row_string(row, 3);
        item["content"] = row_string(row, 4);
        item["client_time"] = row_string(row, 5);
        item["server_time"] = row_string(row, 6);
        result.push_back(std::move(item));
    }

    mysql_free_result(res);
    return json::serialize(result);
}

void UserService::mark_offline_messages_pushed(const std::string& user_id, const std::string& msg_id) {
    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();
    if (!mysql) return;

    std::ostringstream sql;
    sql << "UPDATE im_offline_message SET is_pushed = 1, push_time = NOW() "
        << "WHERE user_id = '" << sql_escape(mysql, user_id)
        << "' AND msg_id = '" << sql_escape(mysql, msg_id) << "'";

    mysql_query(mysql, sql.str().c_str());
}

std::string UserService::get_chat_history(const std::string& user_id, const std::string& friend_id,
                                        int limit, int64_t before_time) {
    json::array result;

    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();
    if (!mysql) return json::serialize(result);

    std::ostringstream query;
    query << "SELECT msg_id, msg_type, chat_type, from_user_id, to_user_id, content_type, content, "
          << "client_time, server_time, status "
          << "FROM im_message "
          << "WHERE ((from_user_id = '" << sql_escape(mysql, user_id)
          << "' AND to_user_id = '" << sql_escape(mysql, friend_id) << "') "
          << "OR (from_user_id = '" << sql_escape(mysql, friend_id)
          << "' AND to_user_id = '" << sql_escape(mysql, user_id) << "')) ";

    if (before_time > 0) {
        query << "AND server_time < FROM_UNIXTIME(" << before_time << ") ";
    }

    query << "ORDER BY server_time DESC LIMIT " << limit;

    if (mysql_query(mysql, query.str().c_str())) {
        return json::serialize(result);
    }

    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res) return json::serialize(result);

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        json::object item;
        item["msg_id"] = row_string(row, 0);
        item["msg_type"] = row_int(row, 1, 1);
        item["chat_type"] = row_int(row, 2, 1);
        item["from_user_id"] = row_string(row, 3);
        item["to_user_id"] = row_string(row, 4);
        item["content_type"] = row_string(row, 5, "text");
        item["content"] = row_string(row, 6);
        item["client_time"] = row_string(row, 7);
        item["server_time"] = row_string(row, 8);
        item["status"] = row_int(row, 9, 1);
        result.push_back(std::move(item));
    }

    mysql_free_result(res);
    return json::serialize(result);
}

} // namespace im
