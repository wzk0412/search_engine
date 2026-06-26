#include "Server.h"
#include <iostream>
#include <cstdlib>
#include <signal.h>

muduo::net::EventLoop* g_loop = nullptr;

void signalHandler(int sig)
{
    std::cout << std::endl;
    std::cout << "[服务器] 收到信号 " << sig << ", 正在关闭..." << std::endl;
    if (g_loop) {
        g_loop->quit();
    }
}

int main(int argc, char* argv[])
{
    int port = 8888;
    if (argc >= 2) {
        port = std::atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            std::cerr << "错误: 端口号必须在 1-65535 之间!" << std::endl;
            return 1;
        }
    }

    std::cout << "========================================" << std::endl;
    std::cout << "  搜索引擎项目 — 第二期: 在线服务" << std::endl;
    std::cout << "  网络库: muduo (Reactor 模式)" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    std::cout << "监听端口: " << port << std::endl;
    std::cout << "服务说明:" << std::endl;
    std::cout << "  - 关键字推荐 (type=1): 输入关键字返回候选词" << std::endl;
    std::cout << "  - 网页搜索   (type=2): 输入查询返回相关网页" << std::endl;
    std::cout << std::endl;

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    muduo::net::EventLoop loop;
    g_loop = &loop;

    SearchServer server(&loop, port);
    server.start();

    loop.loop();

    std::cout << "[服务器] 已关闭。" << std::endl;
    return 0;
}
