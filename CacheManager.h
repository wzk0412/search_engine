#pragma once

#include "Cache.h"

#include <string>
#include <memory>
#include <cstdint>

class RedisCache;

class CacheManager
{
public:
    CacheManager(const std::string& redisHost="127.0.0.1",
                int redisPort=6379,
                int localCapacity=10000,
                bool redisEnabled=true);
    ~CacheManager();

    CacheManager(const CacheManager&) = delete;
    CacheManager& operator=(const CacheManager&) = delete;

    //关键词推荐缓存
    bool getKeywordCache(const std::string& key,std::string& jsonResult);
    void putKeywordCache(const std::string& key,const std::string& jsonResult);

    //网页搜索缓存
    bool getWebSearchCache(const std::string& key,std::string& jsonResult);
    void putWebSearchCache(const std::string& key,const std::string& jsonResult);

    //状态
    bool isRedisAvailable() const{
        return RedisAvailable_;
    }



private:
    static std::string makeRedisKey(const std::string& prefix,const std::string& key);

    int LocalCapacity_;
    bool RedisAvailable_=false;

    std::unique_ptr<KeywordCache> keywordCache_;
    std::unique_ptr<WebSearchCache> webSearchCache_;

    std::unique_ptr<RedisCache> redis_;

     mutable uint64_t localHits_   = 0;
    mutable uint64_t localMisses_ = 0;
    mutable uint64_t redisHits_   = 0;
    mutable uint64_t redisMisses_ = 0;
};

