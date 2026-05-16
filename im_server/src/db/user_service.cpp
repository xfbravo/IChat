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
#include <cstdint>
#include <unordered_set>
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

int64_t row_int64(MYSQL_ROW row, int index, int64_t default_value = 0) {
    return row[index] ? std::stoll(row[index]) : default_value;
}

std::string trim_copy(const std::string& value) {
    const std::size_t begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }
    const std::size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

std::string sql_escape(MYSQL* mysql, const std::string& value) {
    std::string escaped;
    escaped.resize(value.size() * 2 + 1);
    unsigned long length = mysql_real_escape_string(
        mysql, escaped.data(), value.data(), static_cast<unsigned long>(value.size()));
    escaped.resize(length);
    return escaped;
}

bool ensure_large_text_column(MYSQL* mysql,
                              const std::string& table,
                              const std::string& column,
                              const std::string& column_definition,
                              std::string& error_message) {
    std::ostringstream query;
    query << "SELECT DATA_TYPE FROM INFORMATION_SCHEMA.COLUMNS "
          << "WHERE TABLE_SCHEMA = DATABASE() "
          << "AND TABLE_NAME = '" << sql_escape(mysql, table) << "' "
          << "AND COLUMN_NAME = '" << sql_escape(mysql, column) << "' "
          << "LIMIT 1";

    if (mysql_query(mysql, query.str().c_str())) {
        error_message = mysql_error(mysql);
        return false;
    }

    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res) {
        error_message = mysql_error(mysql);
        return false;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row) {
        mysql_free_result(res);
        error_message = table + "." + column + " 字段不存在";
        return false;
    }

    const std::string data_type = row_string(row, 0);
    mysql_free_result(res);

    if (data_type == "mediumtext" || data_type == "longtext") {
        return true;
    }

    std::ostringstream alter_sql;
    alter_sql << "ALTER TABLE `" << table << "` MODIFY COLUMN `"
              << column << "` " << column_definition;

    if (mysql_query(mysql, alter_sql.str().c_str())) {
        error_message = mysql_error(mysql);
        return false;
    }

    return true;
}

bool ensure_column_exists(MYSQL* mysql,
                          const std::string& table,
                          const std::string& column,
                          const std::string& column_definition,
                          std::string& error_message) {
    std::ostringstream query;
    query << "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
          << "WHERE TABLE_SCHEMA = DATABASE() "
          << "AND TABLE_NAME = '" << sql_escape(mysql, table) << "' "
          << "AND COLUMN_NAME = '" << sql_escape(mysql, column) << "'";

    if (mysql_query(mysql, query.str().c_str())) {
        error_message = mysql_error(mysql);
        return false;
    }

    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res) {
        error_message = mysql_error(mysql);
        return false;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    const bool exists = row && row[0] && std::stoll(row[0]) > 0;
    mysql_free_result(res);

    if (exists) {
        return true;
    }

    std::ostringstream alter_sql;
    alter_sql << "ALTER TABLE `" << table << "` ADD COLUMN `"
              << column << "` " << column_definition;

    if (mysql_query(mysql, alter_sql.str().c_str())) {
        error_message = mysql_error(mysql);
        return false;
    }

    return true;
}

bool ensure_avatar_columns(MYSQL* mysql, std::string& error_message) {
    return ensure_large_text_column(mysql,
                                    "im_user",
                                    "avatar_url",
                                    "MEDIUMTEXT COMMENT '头像URL或data URL'",
                                    error_message)
        && ensure_large_text_column(mysql,
                                    "im_friend",
                                    "friend_avatar",
                                    "MEDIUMTEXT COMMENT '好友头像'",
                                    error_message);
}

bool ensure_profile_columns(MYSQL* mysql, std::string& error_message) {
    return ensure_column_exists(mysql,
                                "im_user",
                                "gender",
                                "VARCHAR(8) NOT NULL DEFAULT '' COMMENT '性别'",
                                error_message)
        && ensure_column_exists(mysql,
                                "im_user",
                                "region",
                                "VARCHAR(128) NOT NULL DEFAULT '' COMMENT '地区'",
                                error_message)
        && ensure_column_exists(mysql,
                                "im_user",
                                "signature",
                                "VARCHAR(255) NOT NULL DEFAULT '' COMMENT '个性签名'",
                                error_message);
}

bool ensure_moments_table(MYSQL* mysql, std::string& error_message) {
    const char* sql =
        "CREATE TABLE IF NOT EXISTS im_moment ("
        "id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '自增ID',"
        "moment_id VARCHAR(48) NOT NULL COMMENT '朋友圈ID',"
        "user_id VARCHAR(32) NOT NULL COMMENT '发布者ID',"
        "content TEXT COMMENT '文字内容',"
        "media_type VARCHAR(16) NOT NULL DEFAULT 'text' COMMENT 'text/image',"
        "media_json MEDIUMTEXT COMMENT '图片data URL JSON',"
        "status TINYINT NOT NULL DEFAULT 1 COMMENT '状态：1正常 5删除',"
        "create_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '发布时间',"
        "update_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',"
        "PRIMARY KEY (id),"
        "UNIQUE KEY uk_moment_id (moment_id),"
        "KEY idx_user_time (user_id, create_time),"
        "KEY idx_create_time (create_time)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='朋友圈动态表'";

    if (mysql_query(mysql, sql)) {
        error_message = mysql_error(mysql);
        return false;
    }
    return true;
}

bool ensure_group_tables(MYSQL* mysql, std::string& error_message) {
    const char* group_sql =
        "CREATE TABLE IF NOT EXISTS im_group ("
        "id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '自增ID',"
        "group_id VARCHAR(32) NOT NULL COMMENT '群ID',"
        "group_name VARCHAR(128) NOT NULL DEFAULT '' COMMENT '群名称',"
        "group_avatar MEDIUMTEXT COMMENT '群头像URL或data URL',"
        "owner_id VARCHAR(32) NOT NULL COMMENT '群主ID',"
        "group_type TINYINT NOT NULL DEFAULT 1 COMMENT '群类型',"
        "member_count INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '成员数量',"
        "max_member_count INT UNSIGNED NOT NULL DEFAULT 500 COMMENT '最大成员数',"
        "status TINYINT NOT NULL DEFAULT 1 COMMENT '状态：0解散 1正常',"
        "apply_type TINYINT NOT NULL DEFAULT 1 COMMENT '加群方式',"
        "introduction VARCHAR(512) DEFAULT '' COMMENT '群介绍',"
        "create_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',"
        "update_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',"
        "PRIMARY KEY (id),"
        "UNIQUE KEY uk_group_id (group_id),"
        "KEY idx_owner_id (owner_id),"
        "KEY idx_status (status),"
        "KEY idx_create_time (create_time)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='群组表'";
    if (mysql_query(mysql, group_sql)) {
        error_message = mysql_error(mysql);
        return false;
    }

    const char* member_sql =
        "CREATE TABLE IF NOT EXISTS im_group_member ("
        "id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '自增ID',"
        "group_id VARCHAR(32) NOT NULL COMMENT '群ID',"
        "user_id VARCHAR(32) NOT NULL COMMENT '用户ID',"
        "nickname VARCHAR(64) NOT NULL DEFAULT '' COMMENT '群内昵称',"
        "avatar_url MEDIUMTEXT COMMENT '头像URL或data URL',"
        "role TINYINT NOT NULL DEFAULT 1 COMMENT '角色：1普通成员 2管理员 3群主',"
        "join_type TINYINT NOT NULL DEFAULT 1 COMMENT '加入方式',"
        "inviter_id VARCHAR(32) DEFAULT NULL COMMENT '邀请人ID',"
        "is_notification TINYINT NOT NULL DEFAULT 1 COMMENT '是否接收通知',"
        "is_shield TINYINT NOT NULL DEFAULT 0 COMMENT '是否屏蔽群消息',"
        "join_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '加入时间',"
        "last_msg_time DATETIME DEFAULT NULL COMMENT '最后消息时间',"
        "status TINYINT NOT NULL DEFAULT 1 COMMENT '状态：0已退出 1正常',"
        "PRIMARY KEY (id),"
        "UNIQUE KEY uk_group_user (group_id, user_id),"
        "KEY idx_group_id (group_id),"
        "KEY idx_user_id (user_id),"
        "KEY idx_role (group_id, role),"
        "KEY idx_last_msg_time (group_id, last_msg_time)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='群成员表'";
    if (mysql_query(mysql, member_sql)) {
        error_message = mysql_error(mysql);
        return false;
    }

    return ensure_large_text_column(mysql,
                                    "im_group",
                                    "group_avatar",
                                    "MEDIUMTEXT COMMENT '群头像URL或data URL'",
                                    error_message)
        && ensure_large_text_column(mysql,
                                    "im_group_member",
                                    "avatar_url",
                                    "MEDIUMTEXT COMMENT '头像URL或data URL'",
                                    error_message);
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

    std::string schema_error;
    if (!ensure_profile_columns(mysql, schema_error)) {
        std::cerr << "[UserService] 资料字段迁移失败: " << schema_error << std::endl;
        result.code = 5002;
        result.message = "资料字段迁移失败: " + schema_error;
        return result;
    }

    // 查询用户
    const std::string escaped_user_id = sql_escape(mysql, user_id);
    std::ostringstream sql;
    sql << "SELECT user_id, nickname, avatar_url, password_hash, salt, status, gender, region, signature "
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
    std::string db_gender = row[6] ? row[6] : "";
    std::string db_region = row[7] ? row[7] : "";
    std::string db_signature = row[8] ? row[8] : "";

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
    result.gender = db_gender;
    result.region = db_region;
    result.signature = db_signature;
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

LoginResult UserService::change_password(const std::string& user_id,
                                         const std::string& old_password,
                                         const std::string& new_password) {
    LoginResult result;

    if (user_id.empty()) {
        result.code = 401;
        result.message = "未登录";
        return result;
    }

    if (old_password.empty() || new_password.empty()) {
        result.code = 1;
        result.message = "密码不能为空";
        return result;
    }

    if (new_password.size() < 6) {
        result.code = 2;
        result.message = "新密码长度至少6位";
        return result;
    }

    if (old_password == new_password) {
        result.code = 3;
        result.message = "新密码不能与旧密码相同";
        return result;
    }

    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();

    if (!mysql) {
        result.code = 5001;
        result.message = "数据库连接失败";
        return result;
    }

    const std::string escaped_user_id = sql_escape(mysql, user_id);
    std::ostringstream query;
    query << "SELECT password_hash, salt, status "
          << "FROM im_user WHERE user_id = '" << escaped_user_id << "'";

    if (mysql_query(mysql, query.str().c_str())) {
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
        mysql_free_result(res);
        result.code = 1001;
        result.message = "用户不存在";
        return result;
    }

    const std::string db_password_hash = row_string(row, 0);
    const std::string db_salt = row_string(row, 1);
    const int db_status = row_int(row, 2);
    mysql_free_result(res);

    if (db_status == 0) {
        result.code = 1001;
        result.message = "用户已被禁用";
        return result;
    }

    if (hash_password(old_password, db_salt) != db_password_hash) {
        result.code = 1001;
        result.message = "旧密码错误";
        return result;
    }

    const std::string new_salt = generate_salt();
    const std::string new_password_hash = hash_password(new_password, new_salt);

    std::ostringstream update_sql;
    update_sql << "UPDATE im_user SET password_hash = '" << sql_escape(mysql, new_password_hash)
               << "', salt = '" << sql_escape(mysql, new_salt)
               << "', update_time = NOW() "
               << "WHERE user_id = '" << escaped_user_id << "' AND status = 1";

    if (mysql_query(mysql, update_sql.str().c_str())) {
        std::cerr << "[UserService] 修改密码失败: " << mysql_error(mysql) << std::endl;
        result.code = 5001;
        result.message = "修改密码失败";
        return result;
    }

    result.code = 0;
    result.message = "密码已更新";
    result.user_id = user_id;
    return result;
}

UserInfo UserService::get_user_by_id(const std::string& user_id) {
    UserInfo info;

    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();

    if (!mysql) return info;

    std::string schema_error;
    if (!ensure_profile_columns(mysql, schema_error)) {
        std::cerr << "[UserService] 资料字段迁移失败: " << schema_error << std::endl;
        return info;
    }

    std::ostringstream sql;
    sql << "SELECT user_id, phone, email, nickname, avatar_url, status, user_type, create_time, "
        << "gender, region, signature "
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
        info.gender = row[8] ? row[8] : "";
        info.region = row[9] ? row[9] : "";
        info.signature = row[10] ? row[10] : "";
    }

    mysql_free_result(res);
    return info;
}

UserInfo UserService::get_user_by_phone(const std::string& phone) {
    UserInfo info;

    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();

    if (!mysql) return info;

    std::string schema_error;
    if (!ensure_profile_columns(mysql, schema_error)) {
        std::cerr << "[UserService] 资料字段迁移失败: " << schema_error << std::endl;
        return info;
    }

    std::ostringstream sql;
    sql << "SELECT user_id, phone, email, nickname, avatar_url, status, user_type, create_time, "
        << "gender, region, signature "
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
        info.gender = row[8] ? row[8] : "";
        info.region = row[9] ? row[9] : "";
        info.signature = row[10] ? row[10] : "";
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
    query << "SELECT f.friend_id, f.remark, f.friend_nickname, u.avatar_url, u.status, "
          << "lm.msg_id AS last_msg_id, lm.content AS last_msg_content, "
          << "DATE_FORMAT(lm.server_time, '%Y-%m-%d %H:%i:%s') AS last_msg_time, "
          << "CAST(UNIX_TIMESTAMP(lm.server_time) * 1000 AS UNSIGNED) AS last_msg_timestamp "
          << "FROM im_friend f "
          << "LEFT JOIN im_user u ON f.friend_id = u.user_id "
          << "LEFT JOIN im_message lm ON ("
          << " ((lm.from_user_id = '" << escaped_user_id << "' AND lm.to_user_id = f.friend_id) "
          << " OR (lm.from_user_id = f.friend_id AND lm.to_user_id = '" << escaped_user_id << "')) "
          << " AND NOT EXISTS ("
          << "   SELECT 1 FROM im_message newer "
          << "   WHERE ((newer.from_user_id = '" << escaped_user_id << "' AND newer.to_user_id = f.friend_id) "
          << "   OR (newer.from_user_id = f.friend_id AND newer.to_user_id = '" << escaped_user_id << "')) "
          << "   AND (newer.server_time > lm.server_time "
          << "   OR (newer.server_time = lm.server_time AND newer.msg_id > lm.msg_id))"
          << " )) "
          << "WHERE f.user_id = '" << escaped_user_id << "' AND f.status = 1 "
          << "ORDER BY last_msg_timestamp IS NULL ASC, last_msg_timestamp DESC, f.friend_nickname ASC";

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
        item["last_msg_id"] = row_string(row, 5);
        item["last_msg_content"] = row_string(row, 6);
        item["last_msg_time"] = row_string(row, 7);
        item["last_msg_timestamp"] = row_int64(row, 8);
        result.push_back(std::move(item));
    }

    mysql_free_result(res);
    return json::serialize(result);
}

GroupCreateResult UserService::create_group(const std::string& owner_id,
                                            const std::string& group_name,
                                            const std::vector<std::string>& member_ids) {
    GroupCreateResult result;
    const std::string clean_name = trim_copy(group_name);

    if (owner_id.empty()) {
        result.code = 401;
        result.message = "未登录";
        return result;
    }
    if (clean_name.empty() || clean_name.size() > 128) {
        result.code = 400;
        result.message = "群名称不能为空且不能超过128个字符";
        return result;
    }

    std::vector<std::string> final_members;
    std::unordered_set<std::string> seen;
    final_members.push_back(owner_id);
    seen.insert(owner_id);
    for (const std::string& raw_id : member_ids) {
        const std::string member_id = trim_copy(raw_id);
        if (member_id.empty() || seen.count(member_id)) {
            continue;
        }
        if (!user_exists(member_id)) {
            result.code = 404;
            result.message = "群成员不存在: " + member_id;
            return result;
        }
        if (!are_friends(owner_id, member_id)) {
            result.code = 403;
            result.message = "只能邀请好友入群: " + member_id;
            return result;
        }
        final_members.push_back(member_id);
        seen.insert(member_id);
    }

    if (final_members.size() < 2) {
        result.code = 400;
        result.message = "至少选择一个联系人";
        return result;
    }
    if (final_members.size() > 500) {
        result.code = 400;
        result.message = "群成员不能超过500人";
        return result;
    }

    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();
    if (!mysql) {
        result.code = 5001;
        result.message = "数据库连接失败";
        return result;
    }

    std::string schema_error;
    if (!ensure_group_tables(mysql, schema_error)) {
        result.code = 5002;
        result.message = "群聊表迁移失败: " + schema_error;
        return result;
    }

    const auto now = std::chrono::system_clock::now();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    const std::string group_id = "g" + std::to_string(millis) + "_" + generate_token(owner_id).substr(0, 8);

    if (mysql_query(mysql, "START TRANSACTION")) {
        result.code = 5001;
        result.message = "创建群聊失败";
        return result;
    }

    std::ostringstream group_sql;
    group_sql << "INSERT INTO im_group (group_id, group_name, group_avatar, owner_id, member_count, status) "
              << "VALUES ('" << sql_escape(mysql, group_id) << "', '"
              << sql_escape(mysql, clean_name) << "', '', '"
              << sql_escape(mysql, owner_id) << "', "
              << final_members.size() << ", 1)";
    if (mysql_query(mysql, group_sql.str().c_str())) {
        mysql_query(mysql, "ROLLBACK");
        result.code = 5001;
        result.message = "创建群聊失败: " + std::string(mysql_error(mysql));
        return result;
    }

    for (const std::string& member_id : final_members) {
        UserInfo info = get_user_by_id(member_id);
        const int role = member_id == owner_id ? 3 : 1;
        std::ostringstream member_sql;
        member_sql << "INSERT INTO im_group_member (group_id, user_id, nickname, avatar_url, role, join_type, inviter_id, status) "
                   << "VALUES ('" << sql_escape(mysql, group_id) << "', '"
                   << sql_escape(mysql, member_id) << "', '"
                   << sql_escape(mysql, info.nickname) << "', '"
                   << sql_escape(mysql, info.avatar_url) << "', "
                   << role << ", 1, '"
                   << sql_escape(mysql, owner_id) << "', 1)";
        if (mysql_query(mysql, member_sql.str().c_str())) {
            mysql_query(mysql, "ROLLBACK");
            result.code = 5001;
            result.message = "添加群成员失败: " + std::string(mysql_error(mysql));
            return result;
        }
    }

    if (mysql_query(mysql, "COMMIT")) {
        mysql_query(mysql, "ROLLBACK");
        result.code = 5001;
        result.message = "创建群聊失败";
        return result;
    }

    result.code = 0;
    result.message = "群聊已创建";
    result.group_id = group_id;
    result.group_name = clean_name;
    result.group_avatar = "";
    result.member_count = static_cast<int>(final_members.size());
    return result;
}

std::string UserService::get_group_list(const std::string& user_id) {
    json::array result;
    if (user_id.empty()) {
        return json::serialize(result);
    }

    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();
    if (!mysql) return json::serialize(result);

    std::string schema_error;
    if (!ensure_group_tables(mysql, schema_error)) {
        std::cerr << "[UserService] 群聊表检查失败: " << schema_error << std::endl;
        return json::serialize(result);
    }

    const std::string escaped_user_id = sql_escape(mysql, user_id);
    std::ostringstream query;
    query << "SELECT g.group_id, g.group_name, COALESCE(g.group_avatar, ''), g.owner_id, g.member_count, "
          << "lm.content, DATE_FORMAT(lm.server_time, '%Y-%m-%d %H:%i:%s'), "
          << "CAST(UNIX_TIMESTAMP(lm.server_time) * 1000 AS UNSIGNED), lm.content_type "
          << "FROM im_group_member gm "
          << "JOIN im_group g ON g.group_id = gm.group_id AND g.status = 1 "
          << "LEFT JOIN im_message lm ON lm.chat_type = 2 AND lm.to_user_id = g.group_id "
          << "LEFT JOIN im_message newer ON newer.chat_type = 2 AND newer.to_user_id = g.group_id "
          << "AND newer.server_time > lm.server_time "
          << "WHERE gm.user_id = '" << escaped_user_id << "' AND gm.status = 1 "
          << "AND newer.id IS NULL "
          << "ORDER BY lm.server_time IS NULL ASC, lm.server_time DESC, g.create_time DESC";

    if (mysql_query(mysql, query.str().c_str())) {
        std::cerr << "[UserService] 获取群列表失败: " << mysql_error(mysql) << std::endl;
        return json::serialize(result);
    }

    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res) return json::serialize(result);

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        json::object item;
        item["group_id"] = row_string(row, 0);
        item["group_name"] = row_string(row, 1);
        item["group_avatar"] = row_string(row, 2);
        item["owner_id"] = row_string(row, 3);
        item["member_count"] = row_int(row, 4, 0);
        item["last_msg_content"] = row_string(row, 5);
        item["last_msg_time"] = row_string(row, 6);
        item["last_msg_timestamp"] = row_int64(row, 7);
        item["last_msg_content_type"] = row_string(row, 8, "text");
        result.push_back(std::move(item));
    }

    mysql_free_result(res);
    return json::serialize(result);
}

bool UserService::is_group_member(const std::string& user_id, const std::string& group_id) {
    if (user_id.empty() || group_id.empty()) {
        return false;
    }

    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();
    if (!mysql) return false;

    std::string schema_error;
    if (!ensure_group_tables(mysql, schema_error)) {
        return false;
    }

    std::ostringstream sql;
    sql << "SELECT 1 FROM im_group_member gm "
        << "JOIN im_group g ON g.group_id = gm.group_id AND g.status = 1 "
        << "WHERE gm.group_id = '" << sql_escape(mysql, group_id)
        << "' AND gm.user_id = '" << sql_escape(mysql, user_id)
        << "' AND gm.status = 1 LIMIT 1";
    if (mysql_query(mysql, sql.str().c_str())) {
        return false;
    }

    MYSQL_RES* res = mysql_store_result(mysql);
    const bool exists = res && mysql_num_rows(res) > 0;
    if (res) mysql_free_result(res);
    return exists;
}

std::vector<std::string> UserService::get_group_member_ids(const std::string& group_id) {
    std::vector<std::string> members;
    if (group_id.empty()) {
        return members;
    }

    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();
    if (!mysql) return members;

    std::string schema_error;
    if (!ensure_group_tables(mysql, schema_error)) {
        return members;
    }

    std::ostringstream sql;
    sql << "SELECT user_id FROM im_group_member "
        << "WHERE group_id = '" << sql_escape(mysql, group_id)
        << "' AND status = 1 ORDER BY role DESC, join_time ASC";
    if (mysql_query(mysql, sql.str().c_str())) {
        return members;
    }

    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res) return members;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        members.push_back(row_string(row, 0));
    }
    mysql_free_result(res);
    return members;
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

        // A添加B，A的friend_id是B，B的friend_id是A；若曾删除过关系，则恢复为正常状态。
        std::ostringstream insert_sql1;
        insert_sql1 << "INSERT INTO im_friend (user_id, friend_id, remark, friend_nickname, friend_avatar) "
                    << "VALUES ('" << sql_escape(mysql, from_user_id) << "', '" << sql_escape(mysql, user_id)
                    << "', '" << sql_escape(mysql, remark) << "', '" << sql_escape(mysql, to_user.nickname)
                    << "', '" << sql_escape(mysql, to_user.avatar_url) << "') "
                    << "ON DUPLICATE KEY UPDATE remark = VALUES(remark), "
                    << "friend_nickname = VALUES(friend_nickname), "
                    << "friend_avatar = VALUES(friend_avatar), status = 1, update_time = NOW()";
        if (mysql_query(mysql, insert_sql1.str().c_str())) {
            std::cerr << "[UserService] 添加申请方好友关系失败: " << mysql_error(mysql) << std::endl;
            result.code = 5001;
            result.message = "添加好友关系失败";
            return result;
        }

        std::ostringstream insert_sql2;
        insert_sql2 << "INSERT INTO im_friend (user_id, friend_id, remark, friend_nickname, friend_avatar) "
                    << "VALUES ('" << sql_escape(mysql, user_id) << "', '" << sql_escape(mysql, from_user_id)
                    << "', '', '" << sql_escape(mysql, from_nickname) << "', '" << sql_escape(mysql, from_user.avatar_url) << "') "
                    << "ON DUPLICATE KEY UPDATE friend_nickname = VALUES(friend_nickname), "
                    << "friend_avatar = VALUES(friend_avatar), status = 1, update_time = NOW()";
        if (mysql_query(mysql, insert_sql2.str().c_str())) {
            std::cerr << "[UserService] 添加接收方好友关系失败: " << mysql_error(mysql) << std::endl;
            result.code = 5001;
            result.message = "添加好友关系失败";
            return result;
        }

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

LoginResult UserService::update_avatar(const std::string& user_id,
                                       const std::string& avatar_url) {
    LoginResult result;

    if (user_id.empty()) {
        result.code = 401;
        result.message = "未登录";
        return result;
    }

    if (avatar_url.empty()) {
        result.code = 1;
        result.message = "头像不能为空";
        return result;
    }

    constexpr std::size_t kMaxAvatarLength = 700 * 1024;
    if (avatar_url.size() > kMaxAvatarLength) {
        result.code = 2;
        result.message = "头像数据过大";
        return result;
    }

    if (avatar_url.rfind("data:image/", 0) != 0 || avatar_url.find(',') == std::string::npos) {
        result.code = 3;
        result.message = "头像格式不支持";
        return result;
    }

    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();
    if (!mysql) {
        result.code = 5001;
        result.message = "数据库连接失败";
        return result;
    }

    std::string schema_error;
    if (!ensure_avatar_columns(mysql, schema_error)) {
        std::cerr << "[UserService] 头像字段迁移失败: " << schema_error << std::endl;
        result.code = 5002;
        result.message = "头像字段迁移失败: " + schema_error;
        return result;
    }

    std::ostringstream sql;
    sql << "UPDATE im_user SET avatar_url = '" << sql_escape(mysql, avatar_url)
        << "', update_time = NOW() "
        << "WHERE user_id = '" << sql_escape(mysql, user_id)
        << "' AND status = 1";

    if (mysql_query(mysql, sql.str().c_str())) {
        std::cerr << "[UserService] 更新头像失败: " << mysql_error(mysql) << std::endl;
        result.code = 5001;
        result.message = "头像同步失败";
        return result;
    }

    result.code = 0;
    result.message = "头像已同步";
    result.user_id = user_id;
    result.avatar_url = avatar_url;
    return result;
}

LoginResult UserService::update_profile(const std::string& user_id,
                                        const std::string& nickname,
                                        const std::string& gender,
                                        const std::string& region,
                                        const std::string& signature) {
    LoginResult result;
    const std::string clean_nickname = trim_copy(nickname);
    const std::string clean_gender = trim_copy(gender);
    const std::string clean_region = trim_copy(region);
    const std::string clean_signature = trim_copy(signature);

    if (user_id.empty()) {
        result.code = 401;
        result.message = "未登录";
        return result;
    }

    if (clean_nickname.empty()) {
        result.code = 1;
        result.message = "昵称不能为空";
        return result;
    }

    if (clean_nickname.size() > 64) {
        result.code = 2;
        result.message = "昵称长度不能超过64个字符";
        return result;
    }

    if (clean_gender != "男" && clean_gender != "女") {
        result.code = 3;
        result.message = "性别只能是男或女";
        return result;
    }

    if (clean_region.size() > 128) {
        result.code = 4;
        result.message = "地区长度不能超过128个字符";
        return result;
    }

    if (clean_signature.size() > 255) {
        result.code = 5;
        result.message = "签名长度不能超过255个字符";
        return result;
    }

    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();
    if (!mysql) {
        result.code = 5001;
        result.message = "数据库连接失败";
        return result;
    }

    std::string schema_error;
    if (!ensure_profile_columns(mysql, schema_error)) {
        std::cerr << "[UserService] 资料字段迁移失败: " << schema_error << std::endl;
        result.code = 5002;
        result.message = "资料字段迁移失败: " + schema_error;
        return result;
    }

    const std::string escaped_user_id = sql_escape(mysql, user_id);
    const std::string escaped_nickname = sql_escape(mysql, clean_nickname);
    const std::string escaped_gender = sql_escape(mysql, clean_gender);
    const std::string escaped_region = sql_escape(mysql, clean_region);
    const std::string escaped_signature = sql_escape(mysql, clean_signature);

    std::ostringstream sql;
    sql << "UPDATE im_user SET nickname = '" << escaped_nickname
        << "', gender = '" << escaped_gender
        << "', region = '" << escaped_region
        << "', signature = '" << escaped_signature
        << "', update_time = NOW() "
        << "WHERE user_id = '" << escaped_user_id
        << "' AND status = 1";

    if (mysql_query(mysql, sql.str().c_str())) {
        std::cerr << "[UserService] 更新个人信息失败: " << mysql_error(mysql) << std::endl;
        result.code = 5001;
        result.message = "资料保存失败";
        return result;
    }

    std::ostringstream verify_sql;
    verify_sql << "SELECT nickname, gender, region, signature, status FROM im_user "
               << "WHERE user_id = '" << escaped_user_id << "' LIMIT 1";
    if (mysql_query(mysql, verify_sql.str().c_str())) {
        std::cerr << "[UserService] 回读个人信息失败: " << mysql_error(mysql) << std::endl;
        result.code = 5001;
        result.message = "资料保存后校验失败";
        return result;
    }

    MYSQL_RES* verify_res = mysql_store_result(mysql);
    if (!verify_res) {
        result.code = 5001;
        result.message = "资料保存后校验失败";
        return result;
    }

    MYSQL_ROW verify_row = mysql_fetch_row(verify_res);
    if (!verify_row) {
        mysql_free_result(verify_res);
        result.code = 404;
        result.message = "用户不存在，资料未保存";
        return result;
    }

    const int db_status = verify_row[4] ? std::stoi(verify_row[4]) : 0;
    if (db_status != 1) {
        mysql_free_result(verify_res);
        result.code = 403;
        result.message = "账号状态异常，资料未保存";
        return result;
    }

    const std::string saved_nickname = row_string(verify_row, 0);
    const std::string saved_gender = row_string(verify_row, 1);
    const std::string saved_region = row_string(verify_row, 2);
    const std::string saved_signature = row_string(verify_row, 3);
    mysql_free_result(verify_res);

    std::cout << "[UserService] 个人信息已写入数据库: user_id=" << user_id
              << ", gender=" << saved_gender
              << ", region=" << saved_region
              << ", signature=" << saved_signature << std::endl;

    std::ostringstream friend_sql;
    friend_sql << "UPDATE im_friend SET friend_nickname = '" << escaped_nickname
               << "' WHERE friend_id = '" << escaped_user_id << "'";
    if (mysql_query(mysql, friend_sql.str().c_str())) {
        std::cerr << "[UserService] 同步好友昵称失败: " << mysql_error(mysql) << std::endl;
    }

    result.code = 0;
    result.message = "资料已保存";
    result.user_id = user_id;
    result.nickname = saved_nickname;
    result.gender = saved_gender;
    result.region = saved_region;
    result.signature = saved_signature;
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
    query << "SELECT o.msg_id, o.msg_type, o.chat_type, o.from_user_id, o.to_user_id, o.content, "
          << "o.client_time, o.server_time, CAST(UNIX_TIMESTAMP(o.server_time) * 1000 AS UNSIGNED), "
          << "COALESCE(m.content_type, 'text') "
          << "FROM im_offline_message o "
          << "LEFT JOIN im_message m ON m.msg_id = o.msg_id "
          << "WHERE o.user_id = '" << sql_escape(mysql, user_id) << "' AND o.is_pushed = 0 "
          << "ORDER BY o.create_time ASC LIMIT 100";

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
        item["content"] = row_string(row, 5);
        item["client_time"] = row_string(row, 6);
        item["server_time"] = row_string(row, 7);
        item["server_timestamp"] = row_int64(row, 8);
        item["content_type"] = row_string(row, 9, "text");
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

std::string UserService::get_chat_history(const std::string& user_id, const std::string& peer_id,
                                 int limit, int64_t before_time, int chat_type) {
    json::array result;

    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();
    if (!mysql) return json::serialize(result);

    if (chat_type == 2 && !is_group_member(user_id, peer_id)) {
        return json::serialize(result);
    }

    std::ostringstream query;
    query << "SELECT msg_id, msg_type, chat_type, from_user_id, to_user_id, content_type, content, "
          << "client_time, server_time, status, CAST(UNIX_TIMESTAMP(server_time) * 1000 AS UNSIGNED) "
          << "FROM im_message ";

    if (chat_type == 2) {
        query << "WHERE chat_type = 2 AND to_user_id = '" << sql_escape(mysql, peer_id) << "' ";
    } else {
        query << "WHERE chat_type = 1 AND ((from_user_id = '" << sql_escape(mysql, user_id)
              << "' AND to_user_id = '" << sql_escape(mysql, peer_id) << "') "
              << "OR (from_user_id = '" << sql_escape(mysql, peer_id)
              << "' AND to_user_id = '" << sql_escape(mysql, user_id) << "')) ";
    }

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
        item["server_timestamp"] = row_int64(row, 10);
        result.push_back(std::move(item));
    }

    mysql_free_result(res);
    return json::serialize(result);
}

LoginResult UserService::create_moment(const std::string& user_id,
                                       const std::string& content,
                                       const std::string& media_type,
                                       const std::string& media_json) {
    LoginResult result;
    const std::string clean_content = trim_copy(content);

    if (user_id.empty()) {
        result.code = 401;
        result.message = "未登录";
        return result;
    }

    if (clean_content.empty() && media_json.empty()) {
        result.code = 1;
        result.message = "朋友圈内容不能为空";
        return result;
    }

    if (clean_content.size() > 2000) {
        result.code = 2;
        result.message = "文字内容不能超过2000字符";
        return result;
    }

    if (media_type != "text" && media_type != "image") {
        result.code = 3;
        result.message = "媒体类型不支持";
        return result;
    }

    constexpr std::size_t kMaxMediaJsonLength = 14 * 1024 * 1024;
    if (media_json.size() > kMaxMediaJsonLength) {
        result.code = 4;
        result.message = "媒体数据过大";
        return result;
    }

    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();
    if (!mysql) {
        result.code = 5001;
        result.message = "数据库连接失败";
        return result;
    }

    std::string schema_error;
    if (!ensure_moments_table(mysql, schema_error)) {
        result.code = 5002;
        result.message = "朋友圈表迁移失败: " + schema_error;
        return result;
    }

    const auto now = std::chrono::system_clock::now();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    const std::string moment_id = "m" + std::to_string(millis) + "_" + generate_token(user_id).substr(0, 12);

    std::ostringstream sql;
    sql << "INSERT INTO im_moment (moment_id, user_id, content, media_type, media_json) "
        << "VALUES ('" << sql_escape(mysql, moment_id) << "', "
        << "'" << sql_escape(mysql, user_id) << "', "
        << "'" << sql_escape(mysql, clean_content) << "', "
        << "'" << sql_escape(mysql, media_type) << "', "
        << "'" << sql_escape(mysql, media_json) << "')";

    if (mysql_query(mysql, sql.str().c_str())) {
        result.code = 5001;
        result.message = "发布失败: " + std::string(mysql_error(mysql));
        return result;
    }

    result.code = 0;
    result.message = "发布成功";
    result.user_id = user_id;
    return result;
}

std::string UserService::get_moments_feed(const std::string& user_id, int limit) {
    json::array result;

    if (user_id.empty()) {
        return json::serialize(result);
    }

    if (limit <= 0 || limit > 100) {
        limit = 50;
    }

    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();
    if (!mysql) return json::serialize(result);

    std::string schema_error;
    if (!ensure_moments_table(mysql, schema_error)) {
        std::cerr << "[UserService] 朋友圈表迁移失败: " << schema_error << std::endl;
        return json::serialize(result);
    }

    const std::string escaped_user_id = sql_escape(mysql, user_id);
    std::ostringstream query;
    query << "SELECT m.moment_id, m.user_id, u.nickname, u.avatar_url, "
          << "m.content, m.media_type, m.media_json, "
          << "DATE_FORMAT(m.create_time, '%Y-%m-%d %H:%i:%s') AS create_time, "
          << "CAST(UNIX_TIMESTAMP(m.create_time) * 1000 AS UNSIGNED) AS create_timestamp "
          << "FROM im_moment m "
          << "LEFT JOIN im_user u ON m.user_id = u.user_id "
          << "WHERE m.status = 1 AND (m.user_id = '" << escaped_user_id << "' "
          << "OR EXISTS (SELECT 1 FROM im_friend f "
          << "WHERE f.user_id = '" << escaped_user_id << "' "
          << "AND f.friend_id = m.user_id AND f.status = 1)) "
          << "ORDER BY m.create_time DESC, m.id DESC "
          << "LIMIT " << limit;

    if (mysql_query(mysql, query.str().c_str())) {
        std::cerr << "[UserService] 获取朋友圈失败: " << mysql_error(mysql) << std::endl;
        return json::serialize(result);
    }

    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res) return json::serialize(result);

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        json::object item;
        item["moment_id"] = row_string(row, 0);
        item["user_id"] = row_string(row, 1);
        item["nickname"] = row_string(row, 2);
        item["avatar_url"] = row_string(row, 3);
        item["content"] = row_string(row, 4);
        item["media_type"] = row_string(row, 5);
        const std::string media_json = row_string(row, 6);
        try {
            item["media"] = media_json.empty() ? json::array() : json::parse(media_json);
        } catch (const std::exception&) {
            item["media"] = json::array();
        }
        item["create_time"] = row_string(row, 7);
        item["create_timestamp"] = row_int64(row, 8);
        result.push_back(std::move(item));
    }

    mysql_free_result(res);
    return json::serialize(result);
}

std::string UserService::get_user_moments(const std::string& viewer_user_id,
                                          const std::string& target_user_id,
                                          int limit) {
    json::array result;

    if (viewer_user_id.empty() || target_user_id.empty()) {
        return json::serialize(result);
    }

    if (viewer_user_id != target_user_id && !are_friends(viewer_user_id, target_user_id)) {
        return json::serialize(result);
    }

    if (limit <= 0 || limit > 100) {
        limit = 50;
    }

    auto conn_guard = db_pool_.get_connection();
    MYSQL* mysql = conn_guard.get();
    if (!mysql) return json::serialize(result);

    std::string schema_error;
    if (!ensure_moments_table(mysql, schema_error)) {
        std::cerr << "[UserService] 朋友圈表迁移失败: " << schema_error << std::endl;
        return json::serialize(result);
    }

    const std::string escaped_target_id = sql_escape(mysql, target_user_id);
    std::ostringstream query;
    query << "SELECT m.moment_id, m.user_id, u.nickname, u.avatar_url, "
          << "m.content, m.media_type, m.media_json, "
          << "DATE_FORMAT(m.create_time, '%Y-%m-%d %H:%i:%s') AS create_time, "
          << "CAST(UNIX_TIMESTAMP(m.create_time) * 1000 AS UNSIGNED) AS create_timestamp "
          << "FROM im_moment m "
          << "LEFT JOIN im_user u ON m.user_id = u.user_id "
          << "WHERE m.status = 1 AND m.user_id = '" << escaped_target_id << "' "
          << "ORDER BY m.create_time DESC, m.id DESC "
          << "LIMIT " << limit;

    if (mysql_query(mysql, query.str().c_str())) {
        std::cerr << "[UserService] 获取用户朋友圈失败: " << mysql_error(mysql) << std::endl;
        return json::serialize(result);
    }

    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res) return json::serialize(result);

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        json::object item;
        item["moment_id"] = row_string(row, 0);
        item["user_id"] = row_string(row, 1);
        item["nickname"] = row_string(row, 2);
        item["avatar_url"] = row_string(row, 3);
        item["content"] = row_string(row, 4);
        item["media_type"] = row_string(row, 5);
        const std::string media_json = row_string(row, 6);
        try {
            item["media"] = media_json.empty() ? json::array() : json::parse(media_json);
        } catch (const std::exception&) {
            item["media"] = json::array();
        }
        item["create_time"] = row_string(row, 7);
        item["create_timestamp"] = row_int64(row, 8);
        result.push_back(std::move(item));
    }

    mysql_free_result(res);
    return json::serialize(result);
}

} // namespace im
