#pragma once
#include <string>
#include <vector>
#include <set>

#include "cppjieba/Jieba.hpp"
#include "simhash/Simhasher.hpp"

class PageProcessor
{
public:
PageProcessor();
void process(const std::string& dir);

private:
/// 解析 dir 目录下的 xml 文件，提取文档，放入 documents_中
void extract_documents(const std::string& dir);
/// 依据 SimHash 算法对 documents_去重
void deduplicate_documents();
/// 构建网页库和网页偏移库
void build_pages_and_offsets(const std::string& pages, const std::string& offsets);
/// 构建倒排索引库
void build_inverted_index(const std::string& filename);
private:
struct Document {
int id;
std::string link;
std::string title;
std::string content;
};
private:
cppjieba::Jieba tokenizer_;
simhash::Simhasher hasher_;
std::set<std::string> stopWords_; // 使用 set, 而非 vector, 是为了方便查找
std::vector<Document> documents_;
std::map<std::string, std::map<int, double>> invertedIndex_;
};