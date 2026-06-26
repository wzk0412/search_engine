#include "RedisCache.h"

#include <hiredis/hiredis.h>

#include <iostream>
#include <cstring>
#include <sstream>

RedisCache::RedisCache(const std::string& host, int port,
                       const std::string& password, int timeoutMs)
    : host_(host)
    , port_(port)
    , password_(password)
    , timeoutMs_(timeoutMs)
{
    std::cout << "[Redis] 正在连接 " << host_ << ":" << port_ << " ..." << std::endl;
    reconnect();
}

RedisCache::~RedisCache()
{
    if (ctx_) {
        redisFree(ctx_);
        ctx_ = nullptr;
    }
    std::cout << "[Redis] 连接已关闭" << std::endl;
}

//连接管理

bool RedisCache::isConnected() const
{
    return connected_ && ctx_ != nullptr && ctx_->err == 0;
}

bool RedisCache::reconnect()
{
    if (ctx_) {
        redisFree(ctx_);
        ctx_ = nullptr;
    }
    connected_ = false;
    //创建新连接
    struct timeval timeout;
    timeout.tv_sec = timeoutMs_ / 1000;
    timeout.tv_usec = (timeoutMs_ % 1000) * 1000;

    ctx_=redisConnectWithTimeout(host_.c_str(), port_, timeout);
    if(ctx_==nullptr||ctx_->err){
        if(ctx_){
            std::cerr << "[Redis] 连接失败: " << ctx_->errstr << std::endl;
            redisFree(ctx_);
            ctx_ = nullptr;
        }else{
            std::cerr << "[Redis] 连接失败: 无法分配 redisContext" << std::endl;
        }
        std::cerr << "[Redis] 自动降级: Redis 缓存不可用, 仅使用本地缓存" << std::endl;
        return false;
    }

    redisSetTimeout(ctx_, timeout);

    //认证
    if(!password_.empty()){
        if(!auth()){
            redisFree(ctx_);
            ctx_ = nullptr;
            std::cerr << "[Redis] 自动降级: Redis 缓存不可用, 仅使用本地缓存" << std::endl;
            return false;
        }
    }

    connected_ = true;
    std::cout << "[Redis] 连接成功" << std::endl;
    return true;

}


bool RedisCache::auth(){
    auto* reply =static_cast<redisReply*>(
        redisCommand(ctx_, "AUTH %s", password_.c_str())
    );
    if(reply == nullptr){
        return false;
    }
    bool ok = (reply->type ==REDIS_REPLY_STATUS &&
                std::strcmp(reply->str, "OK") == 0);
        return ok;
}

//缓存操作

bool RedisCache::get(const std::string& key, std::string& value)
{
    if(!isConnected()){
        if(reconnect()){
            return false;
        }
    }

    auto* reply = static_cast<redisReply*>(
        redisCommand(ctx_, "GET %s", key.c_str())
    );

    if (reply == nullptr) {
        connected_ = false;
        return false;
    }

    bool ok=false;
    if (reply->type == REDIS_REPLY_STRING) {
        value.assign(reply->str, reply->len);
        ok = true;
    }
    freeReplyObject(reply);
    return ok;
}

bool RedisCache::put(const std::string& key, const std::string& value)
{
    if(!isConnected()){
        if(reconnect()){
            return false;
        }
    }

    auto* reply = static_cast<redisReply*>(
        redisCommand(ctx_, "SET %s %b", value.c_str(), value.size())
    );
    if (reply == nullptr) {
        connected_ = false;
        return false;
    }
    freeReplyObject(reply);
    return true;
}

bool RedisCache::remove(const std::string& key){
    if(!isConnected()){
        if(reconnect()){
            return false;
        }
    }
    auto* reply = static_cast<redisReply*>(
        redisCommand(ctx_, "DEL %s", key.c_str()));

    if (reply == nullptr) {
        connected_ = false;
        return false;
    }
    freeReplyObject(reply);
    return true;
}

bool RedisCache::exists(const std::string& key)
{
    if (!isConnected()) {
        if (!reconnect()) return false;
    }
    auto* reply = static_cast<redisReply*>(
        redisCommand(ctx_, "EXISTS %s", key.c_str()));

    if (reply == nullptr) {
        connected_ = false;
        return false;
    }

    bool exists = (reply->type == REDIS_REPLY_INTEGER && reply->integer == 1);
    freeReplyObject(reply);
    return exists;

}
