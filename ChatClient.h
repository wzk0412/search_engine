#pragma once

#include <muduo/net/TcpClient.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/Buffer.h>
#include <muduo/base/Timestamp.h>

#include <string>
#include <mutex>
#include <condition_variable>

// 基于 muduo 的 TCP 客户端包装类
// - IO 线程 (EventLoop) 处理网络收发
// - 业务线程通过 send() / waitResponse() 交互
class ChatClient : muduo::noncopyable
{
public:
    ChatClient(muduo::net::EventLoop* loop,
               const muduo::net::InetAddress& serverAddr);
    ~ChatClient();

    void connect();
    void disconnect();

    // 发送消息（线程安全，将任务投递到 IO 线程执行）
    void send(const std::string& message);

    // 阻塞等待服务器响应（在业务线程调用）
    std::string waitResponse();

private:
    void onConnection(const muduo::net::TcpConnectionPtr& conn);
    void onMessage(const muduo::net::TcpConnectionPtr& conn,
                   muduo::net::Buffer* buf,
                   muduo::Timestamp time);

    muduo::net::EventLoop* loop_;
    muduo::net::TcpClient client_;
    muduo::net::TcpConnectionPtr connection_;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::string response_;
    bool hasResponse_ = false;
    bool connected_  = false;

    std::string recvBuf_; // 接收缓冲区，处理粘包
};
