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
    std::string gender;
    std::string region;
    std::string signature;
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
    std::string gender;
    std::string region;
    std::string signature;
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
     */
    LoginResult login(const std::string& user_id, const std::string& password);

    /**
     * @brief 用户注册
     */
    LoginResult register_user(const std::string& phone,
                              const std::string& nickname,
                              const std::string& password);

    /**
     * @brief 修改用户密码
     */
    LoginResult change_password(const std::string& user_id,
                                const std::string& old_password,
                                const std::string& new_password);

    /**
     * @brief 根据用户ID获取用户信息
     */
    UserInfo get_user_by_id(const std::string& user_id);

    /**
     * @brief 根据手机号获取用户信息
     */
    UserInfo get_user_by_phone(const std::string& phone);

    /**
     * @brief 检查用户是否存在
     */
    bool user_exists(const std::string& user_id);

    /**
     * @brief 检查两个用户是否是有效好友
     */
    bool are_friends(const std::string& user_id, const std::string& friend_id);

    /**
     * @brief 检查 user_id 是否拉黑了 block_user_id
     */
    bool is_blocked(const std::string& user_id, const std::string& block_user_id);

    /**
     * @brief 更新最后登录信息
     */
    void update_login_info(const std::string& user_id, const std::string& ip);

    /**
     * @brief 生成简单 token
     */
    std::string generate_token(const std::string& user_id);

    /**
     * @brief 生成用户ID
     */
    std::string generate_user_id();

    /**
     * @brief 获取好友列表
     */
    std::string get_friend_list(const std::string& user_id);

    /**
     * @brief 获取好友请求列表
     */
    std::string get_friend_requests(const std::string& user_id, int status = 0);

    /**
     * @brief 添加好友请求
     */
    LoginResult add_friend_request(const std::string& from_user_id,
                                    const std::string& to_user_id,
                                    const std::string& remark,
                                    const std::string& from_nickname);

    /**
     * @brief 处理好友请求
     */
    LoginResult handle_friend_request(const std::string& request_id,
                                       bool accept,
                                       const std::string& user_id);

    /**
     * @brief 删除好友
     */
    LoginResult delete_friend(const std::string& user_id, const std::string& friend_id);

    /**
     * @brief 修改好友备注
     */
    LoginResult update_friend_remark(const std::string& user_id,
                                     const std::string& friend_id,
                                     const std::string& remark);

    /**
     * @brief 更新用户头像
     */
    LoginResult update_avatar(const std::string& user_id,
                              const std::string& avatar_url);

    /**
     * @brief 更新用户个人信息
     */
    LoginResult update_profile(const std::string& user_id,
                               const std::string& nickname,
                               const std::string& gender,
                               const std::string& region,
                               const std::string& signature);

    /**
     * @brief 保存消息到数据库
     */
    bool save_message(const std::string& msg_id, int msg_type, int chat_type,
                      const std::string& from_user_id, const std::string& to_user_id,
                      const std::string& content_type, const std::string& content,
                      int64_t client_time);

    /**
     * @brief 获取离线消息
     */
    std::string get_offline_messages(const std::string& user_id);

    /**
     * @brief 标记离线消息已推送
     */
    void mark_offline_messages_pushed(const std::string& user_id, const std::string& msg_id);

    /**
     * @brief 获取聊天记录
     */
    std::string get_chat_history(const std::string& user_id, const std::string& friend_id,
                                 int limit = 20, int64_t before_time = 0);

    /**
     * @brief 发布朋友圈
     */
    LoginResult create_moment(const std::string& user_id,
                              const std::string& content,
                              const std::string& media_type,
                              const std::string& media_json);

    /**
     * @brief 获取自己和好友的朋友圈
     */
    std::string get_moments_feed(const std::string& user_id, int limit = 50);

private:
    std::string hash_password(const std::string& password, const std::string& salt);
    std::string generate_salt();

    DbPool& db_pool_;
};

} // namespace im
