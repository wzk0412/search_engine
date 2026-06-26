#pragma once

#include <string>
#include <list>
#include <unordered_map>
#include <mutex>
#include <stdexcept>

template <typename Key, typename Value>

class LruCache
{
public:
    LruCache(int capacity) : capacity_(capacity) {
        if(capacity <=0){
            throw std::invalid_argument("capacity must be greater than 0");
        }
    }

    bool get(const Key& key, Value& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = map_.find(key);
        if (it != map_.end()) {

            list_.splice(list_.begin(), list_, it->second);
            value = it->second->second;

            return true;
        }
        return false;
    }

    void put(const Key& key, const Value& value) {

        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = map_.find(key);
        if (it != map_.end()) {
             // key 已存在 — 更新值并移到头部
            list_.splice(list_.begin(), list_, it->second);
            it->second->second = value;
            return;
        }
         // 容量满 — 淘汰尾部 (最久未访问)
        if (list_.size() >= capacity_) {
            map_.erase(list_.back().first);
            list_.pop_back();
        }
         // 添加新元素到头部
        list_.emplace_front(key, value);
        map_[key] = list_.begin();
    }

    void remove(const Key& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = map_.find(key);
        if (it != map_.end()) {
            list_.erase(it->second);
            map_.erase(it);
        }
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        list_.clear();
        map_.clear();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return list_.size();
    }


private:
    int capacity_;
    std::list<std::pair<Key, Value>> list_;
    std::unordered_map<Key, typename std::list<std::pair<Key, Value>>::iterator> map_;
    mutable std::mutex mutex_;
};

using KeywordCache  = LruCache<std::string, std::string>;
using WebSearchCache = LruCache<std::string, std::string>;
