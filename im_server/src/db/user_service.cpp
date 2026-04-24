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
    std::ostringstream oss;
    oss << password << salt;

    // SHA256 哈希
    unsigned char hash[32];
    // 简化：使用简单的字符串哈希
    size_t h = 0;
    for (char c : oss.str()) {
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
    std::string user_id = generate_user_id();
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

} // namespace im
