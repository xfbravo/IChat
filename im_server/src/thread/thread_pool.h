/**
 * @file thread_pool.h
 * @brief 线程池实现
 *
 * 线程池设计：
 * - 固定数量的工作线程
 * - 使用 boost::asio::io_context 处理异步任务
 * - 使用 boost::asio::executor_work_guard 保持 io_context 运行
 */

#pragma once

#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <thread>
#include <vector>
#include <functional>
#include <atomic>
#include <memory>

namespace im {

/**
 * @brief 线程池类
 *
 * 使用 boost::asio::io_context 实现异步任务处理
 */
class ThreadPool {
public:
    /**
     * @brief 构造函数
     * @param thread_count 工作线程数量，默认为 std::thread::hardware_concurrency()
     */
    explicit ThreadPool(std::size_t thread_count = std::thread::hardware_concurrency());

    ~ThreadPool();

    // 禁止拷贝
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /**
     * @brief 投递任务到线程池
     *
     * 任务会在某个工作线程中执行
     *
     * @param handler 任务函数（可以是 lambda、function、bind 表达式）
     */
    template<typename Handler>
    void post(Handler&& handler) {
        boost::asio::post(io_context_, std::forward<Handler>(handler));
    }

    /**
     * @brief 启动线程池
     *
     * 创建工作线程并开始处理任务
     */
    void start();

    /**
     * @brief 停止线程池
     *
     * 等待所有工作线程结束
     */
    void stop();

    /**
     * @brief 获取 io_context 引用（供 Server 使用）
     */
    boost::asio::io_context& get_io_context() { return io_context_; }

    /**
     * @brief 获取线程数量
     */
    std::size_t thread_count() const { return threads_.size(); }

private:
    boost::asio::io_context io_context_;                                // ASIO io_context
    std::unique_ptr<boost::asio::executor_work_guard<
        boost::asio::io_context::executor_type>> work_guard_;          // 工作 guard
    std::vector<std::thread> threads_;                                  // 工作线程列表
    std::atomic<bool> running_{false};                                // 运行状态标志
};

} // namespace im
