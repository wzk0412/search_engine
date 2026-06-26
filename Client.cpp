#include "ChatClient.h"
#include "Server.h"   // Message 结构体 + 序列化

#include <iostream>
#include <string>
#include <cstring>

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>

#include <pthread.h>

#include <sstream>

using namespace std;
using namespace muduo;
using namespace muduo::net;

// ============================================================
// 简单格式化 JSON 数组，每个对象占一行
// ============================================================
static string formatResult(const string& json)
{
    // 简单策略：在 },{ 之间换行，并加缩进
    string result = json;
    size_t pos = 0;
    while ((pos = result.find("},{", pos)) != string::npos) {
        result.replace(pos, 3, "},\n  {");
        pos += 4; // 跳过刚插入的换行和空格
    }
    return result;
}

// ============================================================
// 输入线程：独立线程处理用户交互，不阻塞 muduo 事件循环
// ============================================================
struct InputArgs {
    ChatClient* client;
};

void* inputThreadFunc(void* args)
{
    ChatClient* client = static_cast<InputArgs*>(args)->client;

    // 等待连接建立
    sleep(1);

    while (true) {
        cout << "----------------------------------------" << endl;
        cout << "请选择服务类型:" << endl;
        cout << "  1 - 关键字推荐 (输入关键字, 获取候选词)" << endl;
        cout << "  2 - 网页搜索   (输入查询, 获取相关网页)" << endl;
        cout << "  0 - 退出" << endl;
        cout << "> ";

        int choice;
        cin >> choice;
        cin.ignore();  // 忽略换行符

        if (choice == 0) {
            cout << "再见!" << endl;
            client->disconnect();
            break;
        }

        if (choice != 1 && choice != 2) {
            cout << "无效选择, 请重新输入!" << endl;
            continue;
        }

        cout << "请输入查询内容: ";
        string query;
        getline(cin, query);

        if (query.empty()) {
            cout << "查询内容不能为空!" << endl;
            continue;
        }

        // 构建 Message 并序列化
        Message request;
        request.type  = static_cast<uint8_t>(choice);
        request.value = query;
        string data   = request.serialize();

        // 发送请求
        client->send(data);

        // 等待并接收响应
        cout << endl;
        string result = client->waitResponse();

        if (result.empty()) {
            cout << "[错误] 未收到有效响应，请检查服务器状态。" << endl;
            break;
        }

        // 显示结果
        cout << endl;
        cout << "========== 服务器响应 ==========" << endl;
        cout << formatResult(result) << endl;
        cout << "=================================" << endl;
        cout << endl;
    }

    return nullptr;
}

// ============================================================
// 主线程：运行 muduo 事件循环
// ============================================================
int main(int argc, char* argv[])
{
    if (argc < 3) {
        cout << "Usage: " << argv[0] << " <server_ip> <port>" << endl;
        return 1;
    }

    // 设置日志级别
    Logger::setLogLevel(Logger::INFO);

    cout << "========================================" << endl;
    cout << "  搜索服务器测试客户端 (muduo 版)" << endl;
    cout << "========================================" << endl;
    cout << "连接到: " << argv[1] << ":" << argv[2] << endl;
    cout << endl;

    // 创建事件循环（主线程运行）
    EventLoop loop;

    // 解析服务器地址
    InetAddress serverAddr(argv[1], static_cast<uint16_t>(atoi(argv[2])));

    // 创建客户端
    ChatClient client(&loop, serverAddr);
    client.connect();

    // 启动输入线程
    InputArgs inputArgs;
    inputArgs.client = &client;

    pthread_t tid;
    pthread_create(&tid, nullptr, &inputThreadFunc, &inputArgs);

    // 进入事件循环（阻塞，直到 loop.quit() 被调用）
    loop.loop();

    pthread_join(tid, nullptr);
    return 0;
}
