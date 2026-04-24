/**
 * @file db_pool.h
 * @brief 数据库连接池
 *
 * 使用 MySQL C API 实现简单的连接池
 */

#pragma once

#include <mysql/mysql.h>
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <functional>

namespace im {

/**
 * @brief 数据库配置
 */
struct DbConfig {
    std::string host = "localhost";
    int port = 3306;
    std::string user = "imadmin";
    std::string password = "dengni0425";
    std::string database = "im_server";
    int min_connections = 2;
    int max_connections = 10;
};

/**
 * @brief 数据库连接包装
 */
struct DbConnection {
    MYSQL* mysql = nullptr;
    bool in_use = false;
    time_t last_used_time = 0;

    DbConnection() : mysql(mysql_init(nullptr)) {}
    ~DbConnection() { if (mysql) mysql_close(mysql); }
};

/**
 * @brief 数据库连接池
 */
class DbPool {
public:
    explicit DbPool(const DbConfig& config);
    ~DbPool();

    // 禁止拷贝
    DbPool(const DbPool&) = delete;
    DbPool& operator=(const DbPool&) = delete;

    /**
     * @brief 初始化连接池
     */
    bool init();

    /**
     * @brief 获取连接
     *
     * 使用 RAII 风格，自动归还连接
     */
    class Guard {
    public:
        Guard(DbPool& pool, DbConnection* conn)
            : pool_(pool), conn_(conn) {
            if (conn_) conn_->in_use = true;
        }

        ~Guard() {
            if (conn_) {
                conn_->last_used_time = time(nullptr);
                conn_->in_use = false;
                pool_.return_connection(conn_);
            }
        }

        MYSQL* get() { return conn_ ? conn_->mysql : nullptr; }
        MYSQL* operator->() { return get(); }

    private:
        DbPool& pool_;
        DbConnection* conn_;
    };

    Guard get_connection();

private:
    friend class Guard;
    void return_connection(DbConnection* conn);
    DbConnection* create_connection();
    bool ping_connection(DbConnection* conn);

    DbConfig config_;
    std::vector<DbConnection*> connections_;
    std::mutex mutex_;
    size_t pool_size_ = 0;
};

/**
 * @brief 数据库结果包装
 */
class DbResult {
public:
    DbResult() = default;
    explicit DbResult(MYSQL_RES* res) : res_(res) {}
    ~DbResult() { if (res_) mysql_free_result(res_); }

    DbResult(const DbResult&) = delete;
    DbResult& operator=(const DbResult&) = delete;

    DbResult(DbResult&& other) noexcept : res_(other.res_) {
        other.res_ = nullptr;
    }

    DbResult& operator=(DbResult&& other) noexcept {
        if (this != &other) {
            if (res_) mysql_free_result(res_);
            res_ = other.res_;
            other.res_ = nullptr;
        }
        return *this;
    }

    MYSQL_ROW fetch_row() {
        if (!res_) return nullptr;
        return mysql_fetch_row(res_);
    }

    unsigned int num_rows() const {
        return res_ ? mysql_num_rows(res_) : 0;
    }

    unsigned int num_fields() const {
        return res_ ? mysql_num_fields(res_) : 0;
    }

    MYSQL_FIELD* fetch_field() {
        return res_ ? mysql_fetch_field(res_) : nullptr;
    }

    operator bool() const { return res_ != nullptr; }

private:
    MYSQL_RES* res_ = nullptr;
};

} // namespace im
