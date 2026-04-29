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

namespace im {

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
    std::ostringstream sql;
    sql << "SELECT user_id, nickname, avatar_url, password_hash, salt, status "
        << "FROM im_user WHERE user_id = '" << user_id << "' OR phone = '" << user_id << "'";

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
    std::ostringstream check_sql;
    check_sql << "SELECT user_id FROM im_user WHERE phone = '" << phone << "'";

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
    std::ostringstream insert_sql;
    insert_sql << "INSERT INTO im_user (user_id, phone, nickname, password_hash, salt, status) "
               << "VALUES ('" << user_id << "', '" << phone << "', '" << nickname << "', '"
               << password_hash << "', '" << salt << "', 1)";

    if (mysql_query(mysql, insert_sql.str().c_str())) {
        result.code = 5001;
        result.message = "注册失败: " + std::string(mysql_error(mysql));
        return result;
    }

    // 初始化用户统计
    std::ostringstream stats_sql;
    stats_sql << "INSERT INTO im_user_stats (user_id) VALUES ('" << user_id << "')";
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
        << "FROM im_user WHERE user_id = '" << user_id << "'";

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
        << "FROM im_user WHERE phone = '" << phone << "'";

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
    sql << "SELECT 1 FROM im_user WHERE user_id = '" << user_id << "' LIMIT 1";

    if (mysql_query(mysql, sql.str().c_str())) return false;

    MYSQL_RES* res = mysql_store_result(mysql);
    bool exists = res && mysql_num_rows(res) > 0;

    if (res) mysql_free_result(res);
    return exists;
}

void UserService::update_login_info(const std::string& user_id,
                                    const std::string& ip) {
    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();

    if (!mysql) return;

    std::ostringstream sql;
    sql << "UPDATE im_user SET last_login_time = NOW(), last_login_ip = '" << ip
        << "' WHERE user_id = '" << user_id << "'";

    mysql_query(mysql, sql.str().c_str());
}

std::string UserService::get_friend_list(const std::string& user_id) {
    std::ostringstream result;
    result << "[";

    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();
    if (!mysql) return result.str();

    std::ostringstream query;
    query << "SELECT f.friend_id, f.remark, f.friend_nickname, f.friend_avatar, u.status "
          << "FROM im_friend f "
          << "LEFT JOIN im_user u ON f.friend_id = u.user_id "
          << "WHERE f.user_id = '" << user_id << "' AND f.status = 1 "
          << "ORDER BY f.friend_nickname ASC";

    if (mysql_query(mysql, query.str().c_str())) {
        return result.str();
    }

    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res) return result.str();

    bool first = true;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        if (!first) result << ",";
        first = false;
        std::string friend_id = row[0] ? row[0] : "";
        std::string remark = row[1] ? row[1] : "";
        std::string nickname = row[2] ? row[2] : "";
        std::string avatar = row[3] ? row[3] : "";
        int status = row[4] ? std::stoi(row[4]) : 0;

        result << "{\"friend_id\":\"" << friend_id << "\","
               << "\"remark\":\"" << remark << "\","
               << "\"nickname\":\"" << nickname << "\","
               << "\"avatar_url\":\"" << avatar << "\","
               << "\"status\":" << status << "}";
    }

    mysql_free_result(res);
    result << "]";
    return result.str();
}

std::string UserService::get_friend_requests(const std::string& user_id, int status) {
    std::ostringstream result;
    result << "[";

    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();
    if (!mysql) return result.str();

    std::ostringstream query;
    query << "SELECT request_id, from_user_id, from_nickname, from_avatar, remark, create_time "
          << "FROM im_friend_request "
          << "WHERE to_user_id = '" << user_id << "' AND status = " << status
          << " ORDER BY create_time DESC";

    if (mysql_query(mysql, query.str().c_str())) {
        return result.str();
    }

    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res) return result.str();

    bool first = true;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        if (!first) result << ",";
        first = false;
        std::string request_id = row[0] ? row[0] : "";
        std::string from_user_id = row[1] ? row[1] : "";
        std::string from_nickname = row[2] ? row[2] : "";
        std::string from_avatar = row[3] ? row[3] : "";
        std::string remark = row[4] ? row[4] : "";
        std::string create_time = row[5] ? row[5] : "";

        result << "{\"request_id\":\"" << request_id << "\","
               << "\"from_user_id\":\"" << from_user_id << "\","
               << "\"from_nickname\":\"" << from_nickname << "\","
               << "\"from_avatar\":\"" << from_avatar << "\","
               << "\"remark\":\"" << remark << "\","
               << "\"create_time\":\"" << create_time << "\"}";
    }

    mysql_free_result(res);
    result << "]";
    return result.str();
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
        result.code = 5001;
        result.message = "数据库连接失败";
        return result;
    }

    // 生成请求ID
    std::string request_id = generate_token(from_user_id) + "_" + std::to_string(time(nullptr));

    // 插入好友请求
    std::ostringstream insert_sql;
    insert_sql << "INSERT INTO im_friend_request (request_id, from_user_id, from_nickname, to_user_id, remark, status, expire_time) "
               << "VALUES ('" << request_id << "', '" << from_user_id << "', '" << from_nickname << "', '" << to_user_id
               << "', '" << remark << "', 0, DATE_ADD(NOW(), INTERVAL 7 DAY))";

    if (mysql_query(mysql, insert_sql.str().c_str())) {
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
    select_sql << "SELECT from_user_id, to_user_id, from_nickname, to_user_id FROM im_friend_request "
               << "WHERE request_id = '" << request_id << "' AND to_user_id = '" << user_id << "' AND status = 0";

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
    mysql_free_result(res);

    // 更新请求状态
    std::ostringstream update_sql;
    update_sql << "UPDATE im_friend_request SET status = " << (accept ? 1 : 2)
               << ", handle_time = NOW() WHERE request_id = '" << request_id << "'";
    mysql_query(mysql, update_sql.str().c_str());

    if (accept) {
        // 双向添加好友记录
        // 获取对方用户信息
        UserInfo from_user = get_user_by_id(from_user_id);
        UserInfo to_user = get_user_by_id(user_id);

        // A添加B，A的friend_id是B，B的friend_id是A
        std::ostringstream insert_sql1;
        insert_sql1 << "INSERT INTO im_friend (user_id, friend_id, friend_nickname, friend_avatar) "
                    << "VALUES ('" << from_user_id << "', '" << user_id << "', '" << to_user.nickname << "', '" << to_user.avatar_url << "')";
        mysql_query(mysql, insert_sql1.str().c_str());

        std::ostringstream insert_sql2;
        insert_sql2 << "INSERT INTO im_friend (user_id, friend_id, friend_nickname, friend_avatar) "
                    << "VALUES ('" << user_id << "', '" << from_user_id << "', '" << from_nickname << "', '" << from_user.avatar_url << "')";
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
    sql << "DELETE FROM im_friend WHERE (user_id = '" << user_id << "' AND friend_id = '" << friend_id << "') "
        << "OR (user_id = '" << friend_id << "' AND friend_id = '" << user_id << "')";

    if (mysql_query(mysql, sql.str().c_str())) {
        result.code = 5001;
        result.message = "删除好友失败";
        return result;
    }

    result.code = 0;
    result.message = "已删除好友";
    return result;
}

} // namespace im
