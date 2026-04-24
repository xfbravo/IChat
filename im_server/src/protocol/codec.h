/**
 * @file codec.h
 * @brief 消息编解码器 - 处理粘包问题
 *
 * 粘包问题解决方案：
 * 1. 发送端：先发送 6 字节头部（类型+长度），再发送数据
 * 2. 接收端：先读取 6 字节头部，解析出长度后再读取完整数据
 *
 * 设计要点：
 * - 使用 boost::asio::streambuf 作为读写缓冲区
 * - 头部解析：读取 6 字节，解析出 type(2B) + length(4B)
 * - 数据读取：根据 length 读取对应字节数
 * - 剩余数据保留在缓冲区中，下次读取时合并
 */

#pragma once

#include "message.h"
#include <boost/asio/streambuf.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/system/error_code.hpp>

namespace im {

/**
 * @brief 消息编解码器类
 *
 * 负责：
 * 1. 从接收缓冲区中解析完整的消息（处理粘包）
 * 2. 将消息编码为字节流（添加头部）
 */
class Codec {
public:
    Codec() = default;

    /**
     * @brief 从缓冲区解析消息
     *
     * 读取流程：
     * 1. 检查缓冲区数据是否 >= 6 字节（最小头部）
     * 2. 读取头部，解析 type 和 length
     * 3. 检查缓冲区是否包含完整的消息体（>= 6 + length）
     * 4. 提取消息体，移除已处理数据
     *
     * @param buf 接收缓冲区
     * @param ec 错误码
     * @return MessagePtr 解析出的消息，解析失败返回 nullptr
     */
    MessagePtr decode(boost::asio::streambuf& buf, boost::system::error_code& ec);

    /**
     * @brief 编码消息为字节流
     *
     * 写入流程：
     * 1. 创建 6 字节头部（type + length）
     * 2. 写入头部到缓冲区
     * 3. 写入消息体
     *
     * @param type 消息类型
     * @param body 消息体内容
     * @param buf 输出缓冲区
     */
    void encode(MsgType type, const std::string& body, boost::asio::streambuf& buf);

    /**
     * @brief 编码消息（便捷接口）
     *
     * @param msg 消息结构
     * @param buf 输出缓冲区
     */
    void encode(const Message& msg, boost::asio::streambuf& buf);

    /**
     * @brief 检查缓冲区是否包含完整消息
     *
     * @param buf 接收缓冲区
     * @return bool true 表示包含完整消息
     */
    bool hasCompleteMessage(const boost::asio::streambuf& buf) const;

private:
    /**
     * @brief 将 uint16_t 转为网络字节序（大端序）
     */
    static uint16_t hton(uint16_t v);

    /**
     * @brief 将 uint32_t 转为网络字节序（大端序）
     */
    static uint32_t hton(uint32_t v);

    /**
     * @brief 将网络字节序转为主机字节序
     */
    static uint16_t ntoh(uint16_t v);
    static uint32_t ntoh(uint32_t v);
};

} // namespace im
