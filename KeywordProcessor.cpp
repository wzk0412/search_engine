#include "KeywordProcessor.h"
#include "DirectoryScanner.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <utfcpp/utf8.h>

static bool hasChineseChar(const std::string& text)
{
    auto it = utf8::iterator<std::string::const_iterator>(
        text.begin(), text.begin(), text.end());
    auto end = utf8::iterator<std::string::const_iterator>(
        text.end(), text.begin(), text.end());
    for (; it != end; ++it) {
        char32_t cp = *it;
        // CJK 统一汉字范围: U+4E00 ~ U+9FFF
        if (cp >= 0x4E00 && cp <= 0x9FFF) {
            return true;
        }
    }
    return false;
}

KeywordProcessor::KeywordProcessor(){
    {
        std::ifstream ifs("stopwords/en_stopwords.txt");
        if(ifs.is_open()){
            std::string word;
            while(std::getline(ifs,word)){
                // 处理 CRLF 行尾: 去除末尾的 \r
                if(!word.empty() && word.back() == '\r'){
                    word.pop_back();
                }
                if(!word.empty()){
                    enDtopWords_.insert(word);
                }
            }
            std::cout << "[停用词] 加载英文停用词 " << enDtopWords_.size() << " 个" << std::endl;
        } else {
            std::cerr << "[警告] 无法打开 stopwords/en_stopwords.txt" << std::endl;
        }
    }
    // 加载中文停用词
    {
        std::ifstream ifs("stopwords/cn_stopwords.txt");
        if (ifs.is_open()) {
            std::string word;
            while (std::getline(ifs, word)) {
                if (!word.empty() && word.back() == '\r') {
                    word.pop_back();
                }
                if (!word.empty()) {
                    cnDtopWords_.insert(word);
                }
            }
            std::cout << "[停用词] 加载中文停用词 " << cnDtopWords_.size() << " 个" << std::endl;
        } else {
            std::cerr << "[警告] 无法打开 stopwords/cn_stopwords.txt" << std::endl;
        }
    }
}

void KeywordProcessor::process(const std::string& chDir, const std::string& enDir)
{
    std::cout << "[关键字推荐] 开始处理..." << std::endl;

    // 处理中文
    std::cout << "[中文] 生成词典库..." << std::endl;
    create_cn_dict(chDir, "cn_dict.dat");
    std::cout << "[中文] 生成索引库..." << std::endl;
    build_cn_index("cn_dict.dat", "cn_index.dat");

    // 处理英文
    std::cout << "[英文] 生成词典库..." << std::endl;
    create_en_dict(enDir, "en_dict.dat");
    std::cout << "[英文] 生成索引库..." << std::endl;
    build_en_index("en_dict.dat", "en_index.dat");

    std::cout << "[关键字推荐] 处理完成！" << std::endl;
}

void KeywordProcessor::create_cn_dict(const std::string& dir, const std::string& outfile){
    auto files=DirectoryScanner::scan(dir);
    std::map<std::string,int> wordCount;

    for(const auto& filename : files){
        std::string filepath=dir + "/" + filename;
        std::ifstream ifs(filepath);
        if(!ifs.is_open()){
            continue;
        }
        std::stringstream buffer;
        buffer << ifs.rdbuf();
        std::string content=buffer.str();

        std::vector<std::string> words;
        tokenizer_.Cut(content,words);
        
        for(const auto& w : words){
            if(cnDtopWords_.count(w)>0){
                continue;
            }
             if (!hasChineseChar(w)) {
                continue;
            }
            wordCount[w]++;
        }
    }
    std::ofstream ofs(outfile);
    int id = 1;
    for (const auto& pair : wordCount) {
        ofs << id << " " << pair.first << " " << pair.second << "\n";
        id++;
    }
}

void KeywordProcessor::build_cn_index(const std::string& dict, const std::string& index)
{
    // 汉字 → 包含该汉字的词语集合 (用 set 去重)
    std::map<std::string, std::set<std::string>> charToWords;

    std::ifstream ifs(dict);
    std::string line;
    while (std::getline(ifs, line)) {
        // 每行格式: <序号> <词语> <频率>
        std::istringstream iss(line);
        int wordId;
        std::string word;
        int freq;
        if (!(iss >> wordId >> word >> freq)) {
            continue;
        }

        // 使用 utfcpp 将词语拆成单个汉字
        std::set<std::string> uniqueChars;
        const char* curr = word.c_str();
        const char* end  = word.c_str() + word.size();
        while (curr != end) {
            const char* start = curr;
            utf8::next(curr, end);
            std::string character(start, curr);
            uniqueChars.insert(character);
        }

        // 将词语本身加入每个不重复汉字的索引
        for (const auto& ch : uniqueChars) {
            charToWords[ch].insert(word);
        }
    }

    // 写入索引文件
    // 格式: <汉字> <词语1> <词语2> ...
    std::ofstream ofs(index);
    for (const auto& pair : charToWords) {
        ofs << pair.first;                // 汉字
        for (const auto& w : pair.second) {
            ofs << " " << w;              // 词语
        }
        ofs << "\n";
    }
}

void KeywordProcessor::create_en_dict(const std::string& dir, const std::string& outfile)
{
    auto files = DirectoryScanner::scan(dir);
    std::map<std::string, int> wordCount;

    for (const auto& filename : files) {
        std::string filepath = dir + "/" + filename;
        std::ifstream ifs(filepath);
        if (!ifs.is_open()) {
            continue;
        }

        std::stringstream buffer;
        buffer << ifs.rdbuf();
        std::string content = buffer.str();

        for (char& ch : content) {
            if (std::isalpha(static_cast<unsigned char>(ch))) {
                ch = std::tolower(static_cast<unsigned char>(ch));
            } else {
                ch = ' ';
            }
        }

        std::istringstream iss(content);
        std::string token;
        while (iss >> token) {
            if (enDtopWords_.count(token) > 0) {
                continue;
            }
            wordCount[token]++;
        }
    }

    std::ofstream ofs(outfile);
    int id = 1;
    for (const auto& pair : wordCount) {
        // 格式: <序号> <单词> <频率>
        ofs << id << " " << pair.first << " " << pair.second << "\n";
        id++;
    }
}

void KeywordProcessor::build_en_index(const std::string& dict, const std::string& index)
{
    // 字母 → 包含它的词序号集合 (用 set 去重+排序)
    std::map<char, std::set<int>> charToWordIDs;

    std::ifstream ifs(dict);
    std::string line;
    while (std::getline(ifs, line)) {
        // 每行格式: <序号> <单词> <频率>
        std::istringstream iss(line);
        int wordId;
        std::string word;
        int freq;
        if (!(iss >> wordId >> word >> freq)) {
            continue;
        }

        // 先收集本词中的所有不重复字母
        // 避免 "book" 中的 'o' 被重复添加两次
        std::set<char> uniqueChars;
        for (char ch : word) {
            uniqueChars.insert(ch);
        }

        // 将词序号加入每个不重复字母的索引
        for (char ch : uniqueChars) {
            charToWordIDs[ch].insert(wordId);  // set 自动去重+排序
        }
    }

    // 写入索引文件
    // 格式: <字母> <词序号1> <词序号2> ...
    std::ofstream ofs(index);
    for (const auto& pair : charToWordIDs) {
        ofs << pair.first;            // 字母
        for (int wid : pair.second) {
            ofs << " " << wid;        // 词序号
        }
        ofs << "\n";
    }
}

