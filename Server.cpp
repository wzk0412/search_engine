#include "Server.h"

#include <sstream>       
#include <fstream>      
#include <cmath>        
#include <algorithm>     
#include <iostream>      
#include <cstring>      
#include <utfcpp/utf8.h> 

using namespace muduo;
using namespace muduo::net;

//序列化
std::string Message::serialize() const{
    std::string result;
    result.push_back(static_cast<char>(type));

    //大端序写入4个字节的length
    uint32_t len=static_cast<uint32_t>(value.size());
    result.push_back(static_cast<char>(len>>24)& 0xFF);
    result.push_back(static_cast<char>(len>>16)& 0xFF);
    result.push_back(static_cast<char>(len>>8) & 0xFF);
    result.push_back(static_cast<char>(len));

    result+=value;
    return result;
}

// ============================================================
// Message 反序列化: 返回消耗的字节数，0 表示数据不完整
// ============================================================
size_t Message::deserialize(const char* data, size_t len, Message& msg)
{
    if (len < 5) return 0;

    msg.type=static_cast<uint8_t>(data[0]);

    uint32_t msgLen=0;
    msgLen=(static_cast<uint32_t>(data[1])<<24)|
           (static_cast<uint32_t>(data[2])<<16)|
           (static_cast<uint32_t>(data[3])<<8) |
           (static_cast<uint32_t>(data[4]));

           if(len<5+msgLen){
               return 0;
           }

        msg.length=msgLen;
        msg.value.assign(data+5,data+5+msgLen);

    return 5+msgLen;
}

SearchServer::SearchServer(EventLoop* loop, int port,
                         const std::string& redisHost,
                         int redisPort,
                         int cacheCapacity,
                         bool redisEnabled)
:loop_(loop)
,server_(loop,InetAddress(static_cast<uint16_t>(port)),"SearchServer")
,totalDocs_(0)
,cacheManager_(redisHost, redisPort, cacheCapacity, redisEnabled)
{
    //加载停用词表
     std::ifstream stopFile("stopwords/cn_stopwords.txt");
    if (stopFile.is_open()) {
        std::string word;
        while (std::getline(stopFile, word)) {
            if (!word.empty() && word.back() == '\r') word.pop_back();
            if (!word.empty()) stopWords_.insert(word);
        }
    }
    std::ifstream enStopFile("stopwords/en_stopwords.txt");
    if (enStopFile.is_open()) {
        std::string word;
        while (std::getline(enStopFile, word)) {
            if (!word.empty() && word.back() == '\r') word.pop_back();
            if (!word.empty()) stopWords_.insert(word);
        }
    }

    //加载第一期生成的数据
    loadKeywordData();
    loadWebSearchData();

    //设置moduo的回调函数
      // 连接回调: 客户端连上/断开时触发
    server_.setConnectionCallback(
        std::bind(&SearchServer::onConnection, this, std::placeholders::_1));

    // 消息回调: 收到数据时触发
    server_.setMessageCallback(
        std::bind(&SearchServer::onMessage, this,
                  std::placeholders::_1,
                  std::placeholders::_2,
                  std::placeholders::_3));
}


SearchServer::~SearchServer() {
    //dtor
}

void SearchServer::start() {
    server_.start();
}

void SearchServer::onConnection(const TcpConnectionPtr& conn) {
    if (conn->connected()) {
       std::cout << "SearchServer - new connection"
                 << conn->peerAddress().toIpPort() << std::endl;
    } else {
        std::cout << "SearchServer - connection closed"
                  << conn->peerAddress().toIpPort() << std::endl;
    }
}

// 消息回调: 收到客户端数据时触发
void SearchServer::onMessage(const TcpConnectionPtr& conn,
                             Buffer* buf,
                             Timestamp receiveTime) {
    while (buf->readableBytes() >= 5) {
        const char* header = buf->peek();

        uint8_t type = static_cast<uint8_t>(header[0]);

        uint32_t msgLen = 0;
        msgLen = (static_cast<uint32_t>(header[1]) << 24) |
                 (static_cast<uint32_t>(header[2]) << 16) |
                 (static_cast<uint32_t>(header[3]) << 8)  |
                 (static_cast<uint32_t>(header[4]));

        if (buf->readableBytes() < 5 + msgLen) {
            break;
        }

        buf->retrieve(5);

        std::string value = buf->retrieveAsString(msgLen);

        Message request;
        request.type   = type;
        request.length = msgLen;
        request.value  = std::move(value);

        Message response = handleRequest(request);

        std::string data = response.serialize();
        conn->send(data.data(), data.size());
    }
}

// 处理客户端请求 (根据 TLV type 分发)
Message SearchServer::handleRequest(const Message& msg) {
    Message response;
    response.type = msg.type; //相应类型和请求相同

   try {
        if (msg.type == 1) {
            // ========== 关键字推荐 ==========
            std::cout << "[请求] 关键字推荐: " << msg.value << std::endl;
            response.value = queryKeyword(msg.value);
        }
        else if (msg.type == 2) {
            // ========== 网页搜索 ==========
            std::cout << "[请求] 网页搜索: " << msg.value << std::endl;
            response.value = queryWebPages(msg.value);
        }
        else {
            response.value = "{\"error\":\"未知的消息类型\"}";
        }
    }
    catch (const std::exception& e) {
        response.value = "{\"error\":\"" + std::string(e.what()) + "\"}";
    }

    return response;
}


// 加载关键字推荐数据
void SearchServer::loadKeywordData(){
    std::ifstream dictFile("cn_dict.dat");
    if (!dictFile.is_open()) {
        std::cerr << "[警告] 无法打开 cn_dict.dat, 请先运行第一期程序!" << std::endl;
        return;
    }

     std::string line;
    while (std::getline(dictFile, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        // cn_dict.dat 格式: <序号> <词语> <频率>
        std::istringstream iss(line);
        int id;
        std::string word;
        int freq;
        if (iss >> id >> word >> freq) {
            cnDict_[word] = freq;
        }
    }
    std::cout << "[服务器] 词典库加载完成 (共 " << cnDict_.size() << " 个词)" << std::endl;

    // ----- 加载中文索引库 (汉字 → 词语列表) -----
    std::ifstream indexFile("cn_index.dat");
    if (!indexFile.is_open()) {
        std::cerr << "[警告] 无法打开 cn_index.dat, 请先运行第一期程序!" << std::endl;
        return;
    } 

    while (std::getline(indexFile, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream iss(line);
        std::string character;
        iss >> character;

        std::string word;
        while (iss >> word) {
            cnIndex_[character].push_back(word);
        }
    }
    std::cout << "[服务器] 索引库加载完成 (共 " << cnIndex_.size() << " 个字符条目)" << std::endl;
}


// 加载网页搜索数据
void SearchServer::loadWebSearchData(){
     std::ifstream indexFile("inverted_index.lib");
    if (!indexFile.is_open()) {
        std::cerr << "[警告] 无法打开 inverted_index.lib, 请先运行第一期程序!" << std::endl;
        return;
    }

    std::string line;
    while (std::getline(indexFile, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream iss(line);
        std::string keyword;
        iss >> keyword;

        int docId;
        double weight;
        while (iss >> docId >> weight) {
            invertedIndex_[keyword][docId] = weight;
        }
    }
    std::cout << "[服务器] 倒排索引加载完成 (共 " << invertedIndex_.size() << " 个索引词)" << std::endl;

    // ----- 加载偏移库 -----
    std::ifstream offsetFile("offsets.lib");
    if (!offsetFile.is_open()) {
        std::cerr << "[警告] 无法打开 offsets.lib, 请先运行第一期程序!" << std::endl;
        return;
    }

    while (std::getline(offsetFile, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream iss(line);
        int docId;
        long offset, size;
        iss >> docId >> offset >> size;
        offsets_[docId] = {offset, size};
        if (docId + 1 > totalDocs_) {
            totalDocs_ = docId + 1;
        }
    }
    std::cout << "[服务器] 偏移库加载完成 (共 " << offsets_.size() << " 篇文档)" << std::endl;
}

// 关键字推荐服务
// 算法:
//   用户输入 → 拆成单字(utfcpp) → 查索引库 → 合并候选词
//   → 算编辑距离 → 排序(编辑距离 > 词频 > 字典序) → 取 topK
std::string SearchServer::queryKeyword(const std::string& keyword, int topK){


     // ----- 0. 查缓存 (key = "keyword:topK") -----
    std::string cacheKey = keyword + ":" + std::to_string(topK);
    std::string cachedResult;
    if (cacheManager_.getKeywordCache(cacheKey, cachedResult)) {
        std::cout << "[缓存命中] 关键字推荐: " << keyword << std::endl;
        return cachedResult;
    }

    // ----- 1. 把关键字拆成单个汉字 (用 utfcpp) -----
    std::vector<std::string> chars;
    const char* curr=keyword.c_str();
    const char* end=keyword.c_str()+keyword.size();

    while(curr != end){
        const char* start=curr;
        utf8::next(curr,end);
        chars.push_back(std::string(start,curr));
    }

    // ----- 2. 从索引库获取候选词 (set 自动去重) -----
    std::set<std::string> candidates;
    for (const auto& ch : chars) {
        auto it = cnIndex_.find(ch);
        if (it != cnIndex_.end()) {
            for (const auto& word : it->second) {
                candidates.insert(word);
            }
        }
    }

    // ----- 3. 为每个候选词计算编辑距离和词频 -----
    struct Candidate {
        int distance;
        int frequency;
        std::string word;
    };
    std::vector<Candidate> scored;

    for (const auto& word : candidates) {
        Candidate c;
        c.word = word;
        c.distance = editDistance(keyword, word);
        auto freqIt = cnDict_.find(word);
        c.frequency = (freqIt != cnDict_.end()) ? freqIt->second : 0;
        scored.push_back(c);
    }

    // ----- 4. 排序: 编辑距离小 > 词频高 > 字典序 -----
    std::sort(scored.begin(), scored.end(),
        [](const Candidate& a, const Candidate& b) {
            if (a.distance != b.distance) return a.distance < b.distance;
            if (a.frequency != b.frequency) return a.frequency > b.frequency;
            return a.word < b.word;
        });

    // ----- 5. 取 topK, 构建 JSON -----
        std::ostringstream json;
    json << "[";
    int count = std::min(topK, static_cast<int>(scored.size()));
    for (int i = 0; i < count; i++) {
        if (i > 0) json << ",";
        std::string escaped = scored[i].word;
        size_t pos = 0;
        while ((pos = escaped.find('"', pos)) != std::string::npos) {
            escaped.insert(pos, "\\");
            pos += 2;
        }
        json << "\"" << escaped << "\"";
    }
    json << "]";
    std::string result = json.str();
    cacheManager_.putKeywordCache(cacheKey, result);
    return result;

}

// 网页搜索服务
// 算法:
//   查询分词 → TF-IDF 向量(X) → 从倒排索引查相关文档
//   → 余弦相似度排序 → 读取网页库 → 生成摘要 → JSON 输出

std::string SearchServer::queryWebPages(const std::string& query, int topK){

// ----- 0. 查缓存 (key = "query:topK") -----
    std::string cacheKey = query + ":" + std::to_string(topK);
    std::string cachedResult;
    if (cacheManager_.getWebSearchCache(cacheKey, cachedResult)) {
        std::cout << "[缓存命中] 网页搜索: " << query << std::endl;
        return cachedResult;
    }
    // ----- 1. 查询分词 + 去停用词 -----
    std::vector<std::string> queryWords;
    tokenizer_.Cut(query, queryWords);

    std::map<std::string, int> queryTermFreq;
     int queryTotalWords = 0;
    for (const auto& w : queryWords) {
        if (stopWords_.count(w) > 0) continue;
        queryTermFreq[w]++;
        queryTotalWords++;
    }

    // ----- 2. 计算查询的 TF-IDF 向量 (L2归一化) -----
     std::map<std::string, double> queryWeights;
    double queryNorm = 0.0;

    for (const auto& pair : queryTermFreq) {
        const std::string& term = pair.first;
        double tf = static_cast<double>(pair.second) / queryTotalWords;

        auto invIt = invertedIndex_.find(term);
        double df = (invIt != invertedIndex_.end())
                        ? static_cast<double>(invIt->second.size()) : 0.0;
        double idf = std::log2((totalDocs_ + 1.0) / (df + 1.0));

        double weight = tf * idf;
        queryWeights[term] = weight;
        queryNorm += weight * weight;
    }

    queryNorm = std::sqrt(queryNorm);
    if (queryNorm > 0.0) {
        for (auto& pair : queryWeights) {
            pair.second /= queryNorm;  // L2 归一化
        }
    }

    // ----- 3. 查找包含查询词的文档, 计算余弦相似度 -----
    std::map<int, double> docScores;
    for (const auto& qpair : queryWeights) {
        const std::string& term = qpair.first;
        double queryWeight = qpair.second;

        auto invIt = invertedIndex_.find(term);
        if (invIt == invertedIndex_.end()) continue;

        for (const auto& docPair : invIt->second) {
            int docId = docPair.first;
            double docWeight = docPair.second;
            docScores[docId] += queryWeight * docWeight;
        }
    }

    // ----- 4. 按得分排序 -----
    std::vector<std::pair<int, double>> ranked(
        docScores.begin(), docScores.end());
    std::sort(ranked.begin(), ranked.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    // ----- 5. 取 topK, 从网页库读取文档, 生成摘要 -----
    std::ifstream pageFile("pages.lib");
    if (!pageFile.is_open()) {
        return "{\"error\":\"无法打开网页库\"}";
    }
    // 收集查询关键词 (用于动态摘要)
    std::vector<std::string> keywords;
    for (const auto& pair : queryTermFreq) {
        keywords.push_back(pair.first);
    } 

     // JSON 字符串转义辅助函数
     // 为什么要转义？
    //   JSON 规范要求某些字符必须转义才能合法:
    //   - 双引号 "   →  \"    (否则会提前结束字符串)
    //   - 反斜杠 \   →  \\    (否则会被误解为转义符)
    //   - 换行符 \n  →  \\n   (JSON 字符串内不允许裸换行)
    //   - 回车符 \r  →  忽略   (Windows 行尾的 \r\n，去掉 \r)
    //   - 制表符 \t  →  \\t
auto escapeJson = [](std::string s) -> std::string {
        std::string r;
        for (char c : s) {
            switch (c) {
                case '"':  r += "\\\""; break;  
                case '\\': r += "\\\\"; break;  
                case '\n': r += "\\n"; break;  
                case '\r': break;                
                case '\t': r += "\\t"; break;  
                default:   r += c;               
            }
        }
        return r;
    };

    // ---------- 辅助: 从 <tag>...</tag> 中提取文本 ----------
     auto extract = [](const std::string& xml, const std::string& tag) -> std::string {
        auto s = xml.find("<" + tag + ">");
        auto e = xml.find("</" + tag + ">");
        if (s != std::string::npos && e != std::string::npos)
            return xml.substr(s + tag.size() + 2,    
                              e - s - tag.size() - 2); 
        return "";
    };

    // 最终返回 JSON 数组: [{"id":0,"title":"...","link":"...","score":0.95,"abstract":"..."}, ...]
    std::ostringstream json;
    json << "[";
    int count = std::min(topK, static_cast<int>(ranked.size()));

    for (int i = 0; i < count; i++) {
        int docId     = ranked[i].first;   // 文档编号
        double score  = ranked[i].second;  // 余弦相似度得分 (0~1)

        // 查偏移库: docId → (offset, size)，实现 O(1) 随机访问
        auto offIt = offsets_.find(docId);
        if (offIt == offsets_.end()) continue;

        long offset = offIt->second.first;   // 文档在网页库中的起始字节
        long size   = offIt->second.second;  // 文档占用的字节数

        // seekg 跳转 + read 读取: 没有偏移库的话只能顺序扫描 (O(N))
        pageFile.seekg(offset);
        std::string docContent(size, '\0');
        pageFile.read(&docContent[0], size);

        // 从 XML 块中提取各字段
        std::string title   = extract(docContent, "title");
        std::string link    = extract(docContent, "link");     // 原文链接，搜索结果必备
        std::string content = extract(docContent, "content");
        if (title.empty()) title = "无标题";

        // 动态摘要: 取关键词在正文中出现位置周围的文字
        std::string abstract = AbstractGenerator::dynamicAbstract(content, keywords, 30);

        // 拼 JSON 对象，escapeJson 防特殊字符破坏 JSON 格式
        if (i > 0) json << ",";
        json << "{\"id\":" << docId
             << ",\"title\":\""    << escapeJson(title)    << "\""
             << ",\"link\":\""     << escapeJson(link)     << "\""
             << ",\"score\":"      << score
             << ",\"abstract\":\"" << escapeJson(abstract) << "\"}";
    }
    json << "]";
    std::string result = json.str();
    cacheManager_.putWebSearchCache(cacheKey, result);
    return result;

}

// 编辑距离 (Levenshtein Distance) — 动态规划
int SearchServer::editDistance(const std::string& a, const std::string& b)
{
    size_t m = a.size();
    size_t n = b.size();

    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1, 0));

    for (size_t i = 0; i <= m; i++) dp[i][0] = static_cast<int>(i);
    for (size_t j = 0; j <= n; j++) dp[0][j] = static_cast<int>(j);

    for (size_t i = 1; i <= m; i++) {
        for (size_t j = 1; j <= n; j++) {
            if (a[i - 1] == b[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1];
            } else {
                dp[i][j] = 1 + std::min({
                    dp[i - 1][j],      // 删除
                    dp[i][j - 1],      // 插入
                    dp[i - 1][j - 1]   // 替换
                });
            }
        }
    }
    return dp[m][n];
}


// 余弦相似度 cosθ = (X·Y) / (|X| × |Y|)

double SearchServer::cosineSimilarity(
    const std::vector<double>& vecX,
    const std::vector<double>& vecY)
{
    if (vecX.size() != vecY.size() || vecX.empty()) return 0.0;

    double dotProduct = 0.0, normX = 0.0, normY = 0.0;
    for (size_t i = 0; i < vecX.size(); i++) {
        dotProduct += vecX[i] * vecY[i];
        normX += vecX[i] * vecX[i];
        normY += vecY[i] * vecY[i];
    }

    double denominator = std::sqrt(normX) * std::sqrt(normY);
    return (denominator == 0.0) ? 0.0 : dotProduct / denominator;
}

// 静态摘要: 取文档内容的前 maxLen 个字符

std::string AbstractGenerator::staticAbstract(const std::string& content, size_t maxLen)
{
    if (content.size() <= maxLen) return content;
    return content.substr(0, maxLen) + "...";
}

// ============================================================
// 动态摘要: 找到关键字在文档位置, 提取周围文字
//
// 算法:
//   1. 在 content 中搜索第一个出现的关键字
//   2. 以该位置为中心, 前后各取 contextLen 个字符
//   3. 首尾添加 "..." 标记
//   4. 找不到任何关键字 → 回退到静态摘要
// ============================================================
std::string AbstractGenerator::dynamicAbstract(
    const std::string& content,
    const std::vector<std::string>& keywords,
    size_t contextLen)
{
    if (content.empty() || keywords.empty()) {
        return staticAbstract(content, 100);
    }

    // 搜索第一个匹配的关键字位置
    size_t bestPos = std::string::npos;
    std::string bestKeyword;

    for (const auto& kw : keywords) {
        size_t pos = content.find(kw);
        if (pos != std::string::npos &&
            (bestPos == std::string::npos || pos < bestPos)) {
            bestPos = pos;
            bestKeyword = kw;
        }
    }

    if (bestPos == std::string::npos) {
        return staticAbstract(content, 100);
    }

    // 提取关键字周围的文字
    size_t start = (bestPos >= contextLen) ? (bestPos - contextLen) : 0;
    size_t end = std::min(bestPos + bestKeyword.size() + contextLen,
                          content.size());

    std::string snippet = content.substr(start, end - start);

    std::string result;
    if (start > 0) result += "...";
    result += snippet;
    if (end < content.size()) result += "...";

    return result;
}