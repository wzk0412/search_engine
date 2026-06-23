#include "PageProcessor.h"
#include "DirectoryScanner.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>
#include <map>
#include <set>

#include <tinyxml2.h>
#include <utfcpp/utf8.h>

// 判断字符串是否包含中文字符 (CJK 统一汉字: U+4E00 ~ U+9FFF)
static bool hasChineseChar(const std::string& text)
{
    auto it = utf8::iterator<std::string::const_iterator>(
        text.begin(), text.begin(), text.end());
    auto end = utf8::iterator<std::string::const_iterator>(
        text.end(), text.begin(), text.end());
    for (; it != end; ++it) {
        char32_t cp = *it;
        if (cp >= 0x4E00 && cp <= 0x9FFF) {
            return true;
        }
    }
    return false;
}

PageProcessor::PageProcessor(){
    // 加载中文停用词
    std::ifstream ifs("stopwords/cn_stopwords.txt");
    if(ifs.is_open()){
        std::string word;
        while(std::getline(ifs,word)){
            if(!word.empty() && word.back() == '\r'){
                word.pop_back();
            }
            if(!word.empty()){
                stopWords_.insert(word);
            }
        }
        std::cout << "[停用词] 加载中文停用词 " << stopWords_.size() << " 个" << std::endl;
    } else {
        std::cerr << "[警告] 无法打开 stopwords/cn_stopwords.txt" << std::endl;
    }
    // 加载英文停用词
    size_t beforeEn = stopWords_.size();
    std::ifstream ifs2("stopwords/en_stopwords.txt");
    if (ifs2.is_open()) {
        std::string word;
        while (std::getline(ifs2, word)) {
            if (!word.empty() && word.back() == '\r') {
                word.pop_back();
            }
            if (!word.empty()) {
                stopWords_.insert(word);
            }
        }
        std::cout << "[停用词] 加载英文停用词 " << (stopWords_.size() - beforeEn) << " 个，"
                  << "共计 " << stopWords_.size() << " 个" << std::endl;
    } else {
        std::cerr << "[警告] 无法打开 stopwords/en_stopwords.txt" << std::endl;
    }
}

void PageProcessor::process(const std::string& dir){
    std::cout << "[网页搜索] 第1步: 提取文档..." << std::endl;
    extract_documents(dir);
    std::cout << "  提取到 " << documents_.size() << " 篇文档" << std::endl;

    std::cout << "[网页搜索] 第2步: SimHash 去重..." << std::endl;
    deduplicate_documents();
    std::cout << "  去重后剩余 " << documents_.size() << " 篇文档" << std::endl;

    std::cout << "[网页搜索] 第3步: 生成网页库和偏移库..." << std::endl;
    build_pages_and_offsets("pages.lib", "offsets.lib");

    std::cout << "[网页搜索] 第4步: 生成倒排索引库..." << std::endl;
    build_inverted_index("inverted_index.lib");

    std::cout << "[网页搜索] 处理完成！" << std::endl;
}

static int hamming_distance(uint64_t x, uint64_t y)
{
    int count = 0;
    int z = x ^ y;
    while (z) {
        z &= (z - 1);  
        count++;
    }
    return count;
}
// 解析 XML 文件, 提取文档
void PageProcessor::extract_documents(const std::string& dir){

    auto files=DirectoryScanner::scan(dir);
    int docId=0;

    for(const auto& filename : files){
        if(filename.size() < 4 || filename.substr(filename.size() - 4) != ".xml"){
            continue;
        }

        std::string filepath=dir + "/" +  filename;

        tinyxml2::XMLDocument xmlDoc;
        if(xmlDoc.LoadFile(filepath.c_str()) != tinyxml2::XML_SUCCESS){
            std::cerr << "[错误] 无法解析 XML 文件: " << filepath << std::endl;
            continue;
        }

        auto* rss =xmlDoc.FirstChildElement("rss");
        if(rss == nullptr){
            std::cerr << "[错误] 无法找到根元素 rss: " << filepath << std::endl;
            continue;
        }

        auto* channel = rss->FirstChildElement("channel");
        if(channel == nullptr){
            std::cerr << "[错误] 无法找到子元素 channel: " << filepath << std::endl;
            continue;
        }

        for(auto* item = channel->FirstChildElement("item");
            item != nullptr;
            item = item->NextSiblingElement("item")
        ){
            Document doc;
            doc.id = docId++;

            auto* linkElem =item->FirstChildElement("link");
            if(linkElem && linkElem->GetText()){
                doc.link=linkElem->GetText();
            }
            
            auto* titleElem = item->FirstChildElement("title");
            if (titleElem && titleElem->GetText()) {
                doc.title = titleElem->GetText();
            }

            std::string contentText;
            bool hasContent = false;

            auto* contentElem = item->FirstChildElement("content");
            if (contentElem && contentElem->GetText()) {
                contentText = contentElem->GetText();
                hasContent = true;
            }

            if(!hasContent){
                auto* descElem=item->FirstChildElement("description");
                if (descElem && descElem->GetText()) {
                    contentText = descElem->GetText();
                    hasContent = true;
                }
            }

            if(!hasContent){
                docId--;
                continue;
            }

            doc.content = contentText;
            documents_.push_back(doc);
        }
    }
}


void PageProcessor::deduplicate_documents(){

    if(documents_.size() <= 1){
        return;
    }

    std::vector<uint64_t> hashes(documents_.size());

    for(size_t i=0;i<documents_.size();i++){
        int topN=std::max(5,std::min(200,static_cast<int>
            (documents_[i].content.size()/120)));
            hasher_.make(documents_[i].content,topN,hashes[i]);
    }

    std::vector<bool> keep(documents_.size(), true);

    for(size_t i=0;i<documents_.size();i++){
        if(!keep[i]){
            continue;
        }
        for (size_t j = i + 1; j < documents_.size(); j++) {
            if (!keep[j]){
                continue;
            } 
            if (simhash::Simhasher::isEqual(hashes[i], hashes[j])) {
                keep[j] = false; 
            }
        }
    }
    
    std::vector<Document> deduped;
    for (size_t i = 0; i < documents_.size(); i++) {
        if (keep[i]) {
            deduped.push_back(documents_[i]);
        }
    }
    std::cout<<" 去除 "<<(documents_.size()-deduped.size())
             <<" 个重复文档"<<std::endl;
             documents_ = std::move(deduped);

}

void PageProcessor::build_pages_and_offsets(
    const std::string& pages, const std::string& offsets)
{
    std::ofstream pageFile(pages);
    std::ofstream offsetFile(offsets);

    for (const auto& doc : documents_) {

        std::ostringstream oss;
        oss << "<doc>\n";
        oss << "<docid>" << doc.id << "</docid>\n";
        oss << "<link>" << doc.link << "</link>\n";
        oss << "<title>" << doc.title << "</title>\n";
        oss << "<content>" << doc.content << "</content>\n";
        oss << "</doc>\n";

        std::string entry = oss.str();
        std::streampos offset = pageFile.tellp();

        offsetFile << doc.id << " " << offset << " " << entry.size() << "\n";

        pageFile << entry;
    }
}

void PageProcessor::build_inverted_index(const std::string& filename){
    int N = static_cast<int>(documents_.size());
    if(N == 0){
        return;
    }

    // 第 1 步: 统计每个文档的词频 (TF)
    std::vector<std::map<std::string, int>> docTermFreq(N);
    std::vector<int> docTotalWords(N, 0);

    for(int i = 0; i < N; i++){
        std::vector<std::string> words;
        tokenizer_.Cut(documents_[i].content, words);

        for(const auto& w : words){
            if(stopWords_.count(w) > 0)
                continue;
            if(!hasChineseChar(w))   // 只保留中文词
                continue;
            docTermFreq[i][w]++;
            docTotalWords[i]++;
        }
    }

    // 第 2 步: 统计文档频率 (DF) —— 包含某词的文档数
    std::map<std::string, int> docFreq;
    for(int i = 0; i < N; i++){
        for(const auto& pair : docTermFreq[i]){
            docFreq[pair.first]++;
        }
    }

    // 第 3 步: 计算 TF-IDF 权重并归一化
    std::vector<std::map<std::string, double>> docWeights(N);
    
    for(int i = 0; i < N; i++){
        double sumSquares = 0.0;

        for(const auto& pair : docTermFreq[i]){
            const std::string& word = pair.first;
            int tf = pair.second;
            int df = docFreq[word];

            double tfVal = static_cast<double>(tf) / docTotalWords[i];
            double idfVal = std::log2(static_cast<double>(N) / (df + 1));
            double weight = tfVal * idfVal;

            docWeights[i][word] = weight;
            sumSquares += weight * weight;
        }

        // 归一化
        double norm = std::sqrt(sumSquares);
        if(norm > 0.0){
            for(auto& pair : docWeights[i]){
                pair.second /= norm;
            }
        }
    }

    // 第 4 步: 生成倒排索引 (词 → {文档ID → 权重})
    std::map<std::string, std::map<int, double>> invertedMap;

    for(int i = 0; i < N; i++){
        for(const auto& pair : docWeights[i]){
            invertedMap[pair.first][documents_[i].id] = pair.second;
        }
    }

    // 第 5 步: 写入倒排索引文件
    std::ofstream ofs(filename);
    for(const auto& entry : invertedMap){
        ofs << entry.first;
        for(const auto& docPair : entry.second){
            ofs << " " << docPair.first << " " << docPair.second;
        }
        ofs << "\n";
    }

    invertedIndex_ = std::move(invertedMap);
}


