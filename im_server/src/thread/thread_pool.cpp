/**
 * @file thread_pool.cpp
 * @brief 线程池实现
 */

#include "thread_pool.h"
#include <iostream>

namespace im {

ThreadPool::ThreadPool(std::size_t thread_count)
    : io_context_()
    , work_guard_()
    , threads_() {

    // 如果未指定线程数，使用硬件支持的核心数，但至少为 1
    if (thread_count == 0) {
        thread_count = 1;
    }

    // 预分配线程向量空间
    threads_.reserve(thread_count);
}

ThreadPool::~ThreadPool() {
    // 确保线程池已停止
    stop();
}

void ThreadPool::start() {
    if (running_.load()) {
        return;  // 已经在运行
    }

    running_.store(true);

    // 创建工作 guard 来保持 io_context 运行
    // 如果不添加 work，io_context 没有任务时会立即返回
    work_guard_ = std::make_unique<boost::asio::executor_work_guard<
        boost::asio::io_context::executor_type>>(
            boost::asio::make_work_guard(io_context_));

    for (std::size_t i = 0; i < threads_.capacity(); ++i) {
        threads_.emplace_back([this]() {
            try {
                std::cout << "[ThreadPool] 工作线程启动: " << std::this_thread::get_id() << std::endl;
                io_context_.run();
                std::cout << "[ThreadPool] 工作线程结束: " << std::this_thread::get_id() << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[ThreadPool] 线程异常: " << e.what() << std::endl;
            }
        });
    }

    std::cout << "[ThreadPool] 线程池已启动，线程数: " << threads_.size() << std::endl;
}

void ThreadPool::stop() {
    if (!running_.load()) {
        return;  // 已经停止
    }

    running_.store(false);

    // 清除 work，让 io_context 可以正常停止
    work_guard_.reset();

    // 停止 io_context
    // 这会让所有 run() 调用返回
    io_context_.stop();

    // 等待所有线程结束
    for (auto& t : threads_) {
        if (t.joinable()) {
            t.join();
        }
    }

    threads_.clear();
    std::cout << "[ThreadPool] 线程池已停止" << std::endl;
}

} // namespace im
