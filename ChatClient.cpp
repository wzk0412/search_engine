#include "ChatClient.h"
#include "Server.h"   // Message 协议定义

#include <muduo/base/Logging.h>

#include <cstring>

using namespace muduo;
using namespace muduo::net;

ChatClient::ChatClient(EventLoop* loop, const InetAddress& serverAddr)
    : loop_(loop)
    , client_(loop, serverAddr, "SearchClient")
{
    client_.setConnectionCallback(
        std::bind(&ChatClient::onConnection, this, _1));
    client_.setMessageCallback(
        std::bind(&ChatClient::onMessage, this, _1, _2, _3));
}

ChatClient::~ChatClient()
{
    disconnect();
}

void ChatClient::connect()
{
    client_.connect();
}

void ChatClient::disconnect()
{
    // 确保在 IO 线程中断开连接
    loop_->runInLoop([this]() {
        if (connection_) {
            connection_->shutdown();
        }
    });
    {
        std::lock_guard<std::mutex> lock(mutex_);
        connected_ = false;
    }
    loop_->quit();  // 断开连接后退出事件循环
}

void ChatClient::send(const std::string& message)
{
    // 投递到 IO 线程执行（muduo 的 TcpConnection::send 非线程安全）
    loop_->runInLoop([this, message]() {
        if (connection_ && connection_->connected()) {
            connection_->send(message);
        } else {
            LOG_WARN << "ChatClient::send - 连接未建立，消息丢弃";
        }
    });
}

std::string ChatClient::waitResponse()
{
    std::unique_lock<std::mutex> lock(mutex_);
    // 等待响应就绪或连接断开
    cv_.wait(lock, [this]() {
        return hasResponse_ || !connected_;
    });
    if (hasResponse_) {
        std::string resp = std::move(response_);
        hasResponse_ = false;
        return resp;
    }
    return "";  // 连接断开，无响应
}

// ========== 私有回调（仅在 IO 线程执行）==========

void ChatClient::onConnection(const TcpConnectionPtr& conn)
{
    if (conn->connected()) {
        LOG_INFO << "ChatClient - 已连接到服务器: "
                 << conn->peerAddress().toIpPort();
        connection_ = conn;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            connected_ = true;
        }
    } else {
        LOG_INFO << "ChatClient - 与服务器断开连接: "
                 << conn->peerAddress().toIpPort();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            connected_ = false;
            cv_.notify_all(); // 唤醒可能在 waitResponse 中阻塞的线程
        }
        connection_.reset();
    }
}

void ChatClient::onMessage(const TcpConnectionPtr& conn,
                            Buffer* buf,
                            Timestamp time)
{
    // 将收到的数据追加到缓冲区
    recvBuf_.append(buf->peek(), buf->readableBytes());
    buf->retrieveAll();

    // 循环解析 Message（处理粘包）
    while (recvBuf_.size() >= 5) {
        // 读取长度字段（大端序，字节 1-4）
        uint32_t msgLen = 0;
        msgLen = (static_cast<uint8_t>(recvBuf_[1]) << 24) |
                 (static_cast<uint8_t>(recvBuf_[2]) << 16) |
                 (static_cast<uint8_t>(recvBuf_[3]) << 8)  |
                 (static_cast<uint8_t>(recvBuf_[4]));

        // 检查是否收到完整消息
        if (recvBuf_.size() < 5 + msgLen) {
            break;  // 数据不完整，等待更多数据
        }

        // 提取一条完整消息
        std::string value(recvBuf_.data() + 5, msgLen);
        recvBuf_.erase(0, 5 + msgLen);

        // 通知业务线程
        {
            std::lock_guard<std::mutex> lock(mutex_);
            response_ = std::move(value);
            hasResponse_ = true;
        }
        cv_.notify_one();
    }
}
