#include "CacheManager.h"
#include "RedisCache.h"

#include <iostream>
#include <sstream>


CacheManager::CacheManager(const std::string& redisHost, int redisPort,
                          int localCapacity, bool redisEnabled)
    : LocalCapacity_(localCapacity)
{
    int half = LocalCapacity_ / 2;
    if (half < 100) half = 100;

    keywordCache_   = std::make_unique<KeywordCache>(half);
    webSearchCache_ = std::make_unique<WebSearchCache>(half);

    std::cout << "[CacheManager] 本地 LRU 缓存已就绪"
              << " (总容量: " << LocalCapacity_ << ")" << std::endl;

    if (!redisEnabled) {
        std::cout << "[CacheManager] 已禁用 Redis, 仅使用本地缓存" << std::endl;
        return;
    }

    std::cout << "[CacheManager] 正在探测 Redis ("
              << redisHost << ":" << redisPort << ") ..." << std::endl;

    redis_ = std::make_unique<RedisCache>(redisHost, redisPort);

    if (redis_->isConnected()) {
        RedisAvailable_ = true;
        std::cout << "[CacheManager] Redis 探测成功 → L1(本地) + L2(Redis) 双层缓存" << std::endl;
    } else {
        std::cout << "[CacheManager] Redis 探测失败 → 自动降级为纯本地缓存" << std::endl;
    }
}

CacheManager::~CacheManager()
{
    uint64_t totalLocal = localHits_ + localMisses_;
    double localRate = (totalLocal == 0) ? 0.0
        : static_cast<double>(localHits_) / totalLocal * 100.0;

    std::cout << "[CacheManager] 缓存统计 — "
              << "本地: " << localHits_ << " 命中 / " << localMisses_ << " 未命中"
              << " (" << localRate << "%)";

    if (RedisAvailable_) {
        uint64_t totalRedis = redisHits_ + redisMisses_;
        double redisRate = (totalRedis == 0) ? 0.0
            : static_cast<double>(redisHits_) / totalRedis * 100.0;
        std::cout << ", Redis: " << redisHits_ << " 命中 / " << redisMisses_ << " 未命中"
                  << " (" << redisRate << "%)";
    }
    std::cout << std::endl;
}

// 关键字推荐缓存
bool CacheManager::getKeywordCache(const std::string& key, std::string& jsonResult)
{
    if (keywordCache_->get(key, jsonResult)) {
        localHits_++;
        return true;
    }
    localMisses_++;

    if (RedisAvailable_ && redis_->isConnected()) {
        std::string rkey = makeRedisKey("keyword", key);
        if (redis_->get(rkey, jsonResult)) {
            redisHits_++;
            keywordCache_->put(key, jsonResult);
            return true;
        }
        redisMisses_++;
    }
    return false;
}

void CacheManager::putKeywordCache(const std::string& key, const std::string& jsonResult)
{
    keywordCache_->put(key, jsonResult);
    if (RedisAvailable_ && redis_->isConnected()) {
        redis_->put(makeRedisKey("keyword", key), jsonResult);
    }
}

// 网页搜索缓存
bool CacheManager::getWebSearchCache(const std::string& key, std::string& jsonResult)
{
    if (webSearchCache_->get(key, jsonResult)) {
        localHits_++;
        return true;
    }
    localMisses_++;

    if (RedisAvailable_ && redis_->isConnected()) {
        std::string rkey = makeRedisKey("webpage", key);
        if (redis_->get(rkey, jsonResult)) {
            redisHits_++;
            webSearchCache_->put(key, jsonResult);
            return true;
        }
        redisMisses_++;
    }
    return false;
}

void CacheManager::putWebSearchCache(const std::string& key, const std::string& jsonResult)
{
    webSearchCache_->put(key, jsonResult);
    if (RedisAvailable_ && redis_->isConnected()) {
        redis_->put(makeRedisKey("webpage", key), jsonResult);
    }
}

// Redis key 生成
std::string CacheManager::makeRedisKey(const std::string& type, const std::string& key)
{
    std::ostringstream oss;
    oss << "cache:" << type << ":" << key;
    return oss.str();
}
