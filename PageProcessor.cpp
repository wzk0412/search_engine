#include "PageProcessor.h"
#include "DirectoryScanner.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>
#include <map>
#include <set>

#include <tinyxml2.h>

PageProcessor::PageProcessor(){
    std::ifstream ifs("stopwords/cn_stopwords.txt");
    if(ifs.is_open()){
        std::string word;
        while(std::getline(ifs,word)){
            if(!word.empty()&&word.back() == '/r'){
                word.pop_back();
            }
            if(!word.empty()){
                stopWords_.insert(word);
            }
        }
    }
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









