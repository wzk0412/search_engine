#pragma once

#include <string>
#include <cstdint>

struct redisContext;

class RedisCache {
public:
    RedisCache(const std::string& host = "127.0.0.1",
               int port = 6379,
               const std::string& password = "",
               int timeoutMs = 500);

    ~RedisCache();

    RedisCache(const RedisCache&) = delete;
    RedisCache& operator=(const RedisCache&) = delete;
    RedisCache(RedisCache&&) = delete;
    RedisCache& operator=(RedisCache&&) = delete;
    
    bool isConnected() const;

    bool get(const std::string& key, std::string& value);

    bool put(const std::string& key, const std::string& value);

    bool remove(const std::string& key);

    bool exists(const std::string& key);

    private:

    bool reconnect();

    bool auth();

    redisContext* ctx_=nullptr;
    std::string host_;
    int port_;
    std::string password_;
    int timeoutMs_;
    bool connected_=false;
};
