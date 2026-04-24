/**
 * @file main.cpp
 * @brief 程序入口
 *
 * 使用 boost::asio::signal_set 实现优雅的信号处理
 * 支持 SIGINT (Ctrl+C) 和 SIGTERM 信号
 *
 * 使用示例：
 *   ./im_server 8080
 *   ./im_server 8080 8  # 指定端口和线程数
 */

#include "server/server.h"
#include "db/db_pool.h"
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <thread>
#include <atomic>
#include <memory>

/**
 * @brief 全局服务器指针
 */
static std::shared_ptr<im::Server> g_server;

/**
 * @brief 全局数据库连接池
 */
static std::unique_ptr<im::DbPool> g_db_pool;

/**
 * @brief 全局停止标志
 */
static std::atomic<bool> g_running{true};

/**
 * @brief 打印使用说明
 */
void print_usage(const char* prog_name) {
    std::cout << "用法: " << prog_name << " <端口> [线程数]" << std::endl;
    std::cout << std::endl;
    std::cout << "参数:" << std::endl;
    std::cout << "  端口    监听端口（必填）" << std::endl;
    std::cout << "  线程数  工作线程数（可选，默认: CPU核心数）" << std::endl;
    std::cout << std::endl;
    std::cout << "示例:" << std::endl;
    std::cout << "  " << prog_name << " 8080" << std::endl;
    std::cout << "  " << prog_name << " 8080 8" << std::endl;
}

/**
 * @brief 初始化数据库连接池
 */
bool init_database() {
    im::DbConfig config;
    config.host = "localhost";
    config.port = 3306;
    config.user = "imadmin";
    config.password = "dengni0425";
    config.database = "im_server";
    config.min_connections = 2;
    config.max_connections = 10;

    g_db_pool = std::make_unique<im::DbPool>(config);
    if (!g_db_pool->init()) {
        std::cerr << "[Main] 数据库连接池初始化失败" << std::endl;
        return false;
    }

    std::cout << "[Main] 数据库连接池初始化成功" << std::endl;
    return true;
}

/**
 * @brief 信号处理类
 *
 * 使用 boost::asio::signal_set 在异步上下文中处理信号
 * 确保服务器能够优雅关闭
 */
class SignalHandler {
public:
    /**
     * @brief 构造函数
     *
     * @param io_context ASIO io_context 引用
     */
    explicit SignalHandler(boost::asio::io_context& io_context)
        : signal_set_(io_context) {
    }

    /**
     * @brief 启动信号监听
     *
     * 异步等待 SIGINT 和 SIGTERM 信号
     */
    void start() {
        // 添加要监听的信号
        signal_set_.add(SIGINT);
        signal_set_.add(SIGTERM);

        // 异步等待信号
        signal_set_.async_wait([this](const boost::system::error_code& ec, int signal_number) {
            handle_signal(ec, signal_number);
        });
    }

    /**
     * @brief 处理收到的信号
     *
     * @param ec 错误码
     * @param signal_number 信号编号
     */
    void handle_signal(const boost::system::error_code& ec, int signal_number) {
        if (ec) {
            std::cerr << "[SignalHandler] 信号处理错误: " << ec.message() << std::endl;
            return;
        }

        std::cout << "\n[SignalHandler] 收到信号: " << signal_number;

        switch (signal_number) {
            case SIGINT:
                std::cout << " (SIGINT/Ctrl+C)";
                break;
            case SIGTERM:
                std::cout << " (SIGTERM)";
                break;
            default:
                std::cout << " (未知)";
                break;
        }
        std::cout << "，正在关闭服务器..." << std::endl;

        // 停止服务器
        if (g_server) {
            g_server->stop();
        }

        // 标记运行状态为停止
        g_running.store(false);
    }

private:
    boost::asio::signal_set signal_set_;  // 信号集
};

int main(int argc, char* argv[]) {
    // 解析命令行参数
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    uint16_t port = 8080;
    std::size_t thread_count = std::thread::hardware_concurrency();

    try {
        port = static_cast<uint16_t>(std::stoi(argv[1]));
        if (port == 0) {
            std::cerr << "[Main] 端口号不能为 0" << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "[Main] 无效的端口: " << argv[1] << std::endl;
        return 1;
    }

    if (argc >= 3) {
        try {
            thread_count = std::stoul(argv[2]);
            if (thread_count == 0) {
                thread_count = 1;
            }
            if (thread_count > 256) {
                std::cout << "[Main] 线程数过大，限制为 256" << std::endl;
                thread_count = 256;
            }
        } catch (const std::exception& e) {
            std::cerr << "[Main] 无效的线程数: " << argv[2] << std::endl;
            return 1;
        }
    }

    std::cout << "========================================" << std::endl;
    std::cout << "       IM Server v1.0.0" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "端口: " << port << std::endl;
    std::cout << "线程数: " << thread_count << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        // 初始化数据库连接池
        if (!init_database()) {
            std::cerr << "[Main] 数据库初始化失败" << std::endl;
            return 1;
        }

        // 创建服务器（传入数据库连接池）
        g_server = std::make_shared<im::Server>(port, thread_count, *g_db_pool);

        // 获取 io_context 引用（用于信号处理和运行事件循环）
        boost::asio::io_context& io_context = g_server->thread_pool().get_io_context();

        // 创建并启动信号处理器
        SignalHandler signal_handler(io_context);
        signal_handler.start();

        // 启动服务器
        g_server->start();

        std::cout << "[Main] 服务器启动成功，按 Ctrl+C 停止" << std::endl;

        // 在主线程中运行 io_context
        // 这是阻塞调用，直到 io_context 停止
        // io_context 会在收到停止信号后停止
        io_context.run();

        std::cout << "[Main] io_context 已停止" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[Main] 服务器异常: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "[Main] 服务器已退出" << std::endl;
    return 0;
}
