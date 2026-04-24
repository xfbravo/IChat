/**
 * @file db_pool.cpp
 * @brief 数据库连接池实现
 */

#include "db_pool.h"
#include <iostream>
#include <chrono>

namespace im {

DbPool::DbPool(const DbConfig& config)
    : config_(config) {
}

DbPool::~DbPool() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto* conn : connections_) {
        delete conn;
    }
    connections_.clear();
}

bool DbPool::init() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (int i = 0; i < config_.min_connections; ++i) {
        auto* conn = create_connection();
        if (conn) {
            connections_.push_back(conn);
            ++pool_size_;
        } else {
            std::cerr << "[DbPool] 创建连接失败" << std::endl;
            return false;
        }
    }

    std::cout << "[DbPool] 连接池初始化完成，连接数: " << pool_size_ << std::endl;
    return true;
}

DbConnection* DbPool::create_connection() {
    auto* conn = new DbConnection();
    if (!conn->mysql) {
        delete conn;
        return nullptr;
    }

    // 连接 MySQL
    if (!mysql_real_connect(conn->mysql,
            config_.host.c_str(),
            config_.user.c_str(),
            config_.password.c_str(),
            config_.database.c_str(),
            config_.port,
            nullptr,  // unix_socket
            0)) {     // client_flag
        std::cerr << "[DbPool] MySQL连接失败: " << mysql_error(conn->mysql) << std::endl;
        delete conn;
        return nullptr;
    }

    // 设置字符集
    mysql_set_character_set(conn->mysql, "utf8mb4");

    conn->last_used_time = time(nullptr);
    return conn;
}

bool DbPool::ping_connection(DbConnection* conn) {
    if (!conn || !conn->mysql) return false;
    return mysql_ping(conn->mysql) == 0;
}

DbPool::Guard DbPool::get_connection() {
    std::lock_guard<std::mutex> lock(mutex_);

    // 查找空闲连接
    for (auto* conn : connections_) {
        if (!conn->in_use) {
            // 检查连接是否有效
            if (ping_connection(conn)) {
                return Guard(*this, conn);
            } else {
                // 连接已断开，重新创建
                delete conn;
                auto* new_conn = create_connection();
                if (new_conn) {
                    *conn = *new_conn;
                    delete new_conn;
                    return Guard(*this, conn);
                }
            }
        }
    }

    // 没有空闲连接且未达到最大连接数，创建新连接
    if (pool_size_ < static_cast<size_t>(config_.max_connections)) {
        auto* conn = create_connection();
        if (conn) {
            connections_.push_back(conn);
            ++pool_size_;
            return Guard(*this, conn);
        }
    }

    // 等待空闲连接（简单实现，这里返回 nullptr）
    std::cerr << "[DbPool] 连接池已满，等待空闲连接..." << std::endl;
    return Guard(*this, nullptr);
}

void DbPool::return_connection(DbConnection* conn) {
    // Guard 的析构函数会自动调用，这里不需要额外处理
    // 但保留这个接口以便未来扩展
}

} // namespace im
