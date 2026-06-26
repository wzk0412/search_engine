#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <cstdint>
#include <memory>

#include <cppjieba/Jieba.hpp> 

#include "CacheManager.h"

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/Buffer.h>
#include <muduo/base/Timestamp.h>

struct Message {
    uint8_t     type   = 0;  // 1: 关键字推荐, 2: 网页搜索
    uint32_t    length = 0;  // value 的长度
    std::string value;       // JSON 内容

    // 序列化为网络字节流: Type(1B) + Length(4B 大端) + Value
    std::string serialize() const;

    // 反序列化：从字节流解析一条消息，返回消耗的字节数，0 表示数据不完整
    static size_t deserialize(const char* data, size_t len, Message& msg);
};

class SearchServer {
public:
 
   SearchServer(muduo::net::EventLoop* loop, int port,
                 const std::string& redisHost = "127.0.0.1",
                 int redisPort = 6379,
                 int cacheCapacity = 10000,
                 bool redisEnabled = true);
    ~SearchServer();

    void start();

    private:
    //连接回调
    void onConnection(const muduo::net::TcpConnectionPtr& conn);
    //消息回调
    void onMessage(const muduo::net::TcpConnectionPtr& conn,
                   muduo::net::Buffer* buf,
                   muduo::Timestamp receiveTime);

    //关键字推荐服务
    //输入关键字，输出json 候选词列表
    std::string queryKeyword(const std::string& keyword,int topK=5);

    //网页搜索服务
    //输入：查询文本，输出json 搜索结果
    std::string queryWebPages(const std::string& query,int topK=10);

    //处理请求 根据TLV type 分发到对应服务
    Message handleRequest(const Message& msg);

    void loadKeywordData();//加载索引库和词典库，加载关键字推荐数据
    void loadWebSearchData();//记载倒排索引库和偏移库，加载网页搜索数据

    //编辑距离
    static int editDistance(const std::string &s1, const std::string &s2);

    //余弦相似度
    static double cosineSimilarity(const std::vector<double> &s1, 
                                   const std::vector<double> &s2);

private:
        //muduo网络库组件
        muduo::net::EventLoop* loop_;
        muduo::net::TcpServer server_;
        
        //关键字推荐数据
        // 索引库: 字符 → 包含它的所有词语
        std::map<std::string,std::vector<std::string>> cnIndex_;
        // 词典库: 词语 → 词频
        std::map<std::string,int> cnDict_;

        //网页搜索数据
        //倒排索引：关键字-->（文档ID--->权重）
        std::map<std::string, std::map<int, double>> invertedIndex_;
        int totalDocs_;//文档总数
        //偏移库，文档id → (在网页库中的偏移量, 文档大小)
        std::map<int,std::pair<long,long>> offsets_;
        //中文分词器
        cppjieba::Jieba tokenizer_;
        //停用词集合
        std::set<std::string> stopWords_;

         //第三期: 缓存管理器
        CacheManager cacheManager_;
};


class AbstractGenerator {
public:
    //静态摘要:取文档内容的前maxLen个字符
    static std::string staticAbstract(const std::string& content, size_t maxLen = 50);

    //动态摘要:根据关键词在文档中的位置，提取周围文字
    static std::string dynamicAbstract(
        const std::string& content,
        const std::vector<std::string>& keywords,
        size_t contentLen = 30
    );
};
