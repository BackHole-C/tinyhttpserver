#ifndef REDIS_CACHE_H
#define REDIS_CACHE_H

#include <string>
#include <memory>
#include <sw/redis++/redis++.h>

class RedisCache {
public:
    RedisCache(const std::string& host = "127.0.0.1", int port = 6379);
    ~RedisCache();

    RedisCache(const RedisCache&) = delete;
    RedisCache& operator=(const RedisCache&) = delete;

    bool is_connected() const;

    bool get(const std::string& key, std::string& value);
    void put(const std::string& key, const std::string& value, int ttl_seconds = 300);
    void remove(const std::string& key);
    bool exists(const std::string& key);
    void flush_all();

private:
    std::unique_ptr<sw::redis::Redis> redis_;
    bool connected_;
};

#endif