#include "KeywordProcessor.h"
#include "PageProcessor.h"
#include <iostream>

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << "  搜索引擎项目 — 第一期: 离线部分" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // =========================================================
    // 任务 1.1: 关键字推荐
    // 生成中文和英文的词典库与索引库
    // =========================================================
    KeywordProcessor kwProc;
    kwProc.process("corpus/CN", "corpus/EN");

    // =========================================================
    // 任务 1.2: 网页搜索
    // 从 XML 语料中提取文档, 去重, 生成网页库和倒排索引
    // =========================================================
    PageProcessor pageProc;
    pageProc.process("corpus/webpages");

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  第一期处理完成!" << std::endl;
    std::cout << "  生成文件:" << std::endl;
    std::cout << "    - cn_dict.dat (中文词典库)" << std::endl;
    std::cout << "    - cn_index.dat (中文索引库)" << std::endl;
    std::cout << "    - en_dict.dat (英文词典库)" << std::endl;
    std::cout << "    - en_index.dat (英文索引库)" << std::endl;
    std::cout << "    - pages.lib (网页库)" << std::endl;
    std::cout << "    - offsets.lib (网页偏移库)" << std::endl;
    std::cout << "    - inverted_index.lib (倒排索引库)" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}

