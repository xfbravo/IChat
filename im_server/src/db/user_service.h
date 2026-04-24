/**
 * @file user_service.h
 * @brief 用户服务
 *
 * 处理用户登录、注册等业务逻辑
 */

#pragma once

#include "db_pool.h"
#include <string>
#include <vector>

namespace im {

/**
 * @brief 用户信息
 */
struct UserInfo {
    std::string user_id;
    std::string phone;
    std::string email;
    std::string nickname;
    std::string avatar_url;
    int status = 0;  // 0禁用 1正常 2被举报
    int user_type = 1;
    std::string create_time;
};

/**
 * @brief 登录结果
 */
struct LoginResult {
    int code = 0;           // 0成功，非0失败
    std::string message;    // 错误信息
    std::string user_id;
    std::string nickname;
    std::string avatar_url;
    std::string token;       // 暂不实现JWT
};

/**
 * @brief 用户服务类
 */
class UserService {
public:
    explicit UserService(DbPool& db_pool) : db_pool_(db_pool) {}

    /**
     * @brief 用户登录
     *
     * @param user_id 用户ID（手机号或user_id）
     * @param password 密码
     * @return LoginResult 登录结果
     */
    LoginResult login(const std::string& user_id, const std::string& password);

    /**
     * @brief 用户注册
     *
     * @param phone 手机号
     * @param nickname 昵称
     * @param password 密码
     * @return LoginResult 注册结果
     */
    LoginResult register_user(const std::string& phone,
                              const std::string& nickname,
                              const std::string& password);

    /**
     * @brief 根据用户ID获取用户信息
     *
     * @param user_id 用户ID
     * @return UserInfo 用户信息
     */
    UserInfo get_user_by_id(const std::string& user_id);

    /**
     * @brief 检查用户是否存在
     *
     * @param user_id 用户ID
     * @return bool true 存在，false 不存在
     */
    bool user_exists(const std::string& user_id);

    /**
     * @brief 更新最后登录信息
     *
     * @param user_id 用户ID
     * @param ip 登录IP
     */
    void update_login_info(const std::string& user_id, const std::string& ip);

    /**
     * @brief 生成简单 token
     *
     * 实际应用中应使用 JWT
     */
    std::string generate_token(const std::string& user_id);

    /**
     * @brief 生成用户ID
     */
    std::string generate_user_id();

private:
    /**
     * @brief 计算密码哈希
     *
     * 实际应用中应使用更安全的算法
     */
    std::string hash_password(const std::string& password, const std::string& salt);

    /**
     * @brief 生成随机盐值
     */
    std::string generate_salt();

    DbPool& db_pool_;
};

} // namespace im
