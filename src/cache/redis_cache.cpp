#include "cache/redis_cache.h"
#include "utils/logger.h"

RedisCache::RedisCache(const std::string& host, int port) : connected_(false) {
    try {
        sw::redis::ConnectionOptions opts;
        opts.host = host;
        opts.port = port;
        opts.socket_timeout = std::chrono::milliseconds(500);
        opts.connect_timeout = std::chrono::seconds(2);

        redis_ = std::make_unique<sw::redis::Redis>(opts);
        redis_->ping();
        connected_ = true;
        Logger::instance().info("RedisCache connected to " + host + ":" + std::to_string(port));
    } catch (const sw::redis::Error& e) {
        Logger::instance().error("Failed to connect to Redis: " + std::string(e.what()));
    }
}

RedisCache::~RedisCache() {
    if (connected_) {
        Logger::instance().info("RedisCache disconnected");
    }
}

bool RedisCache::is_connected() const {
    return connected_;
}

bool RedisCache::get(const std::string& key, std::string& value) {
    if (!connected_) return false;
    try {
        auto result = redis_->get(key);
        if (result) {
            value = *result;
            return true;
        }
        return false;
    } catch (const sw::redis::Error& e) {
        Logger::instance().error("Redis get error: " + std::string(e.what()));
        return false;
    }
}

void RedisCache::put(const std::string& key, const std::string& value, int ttl_seconds) {
    if (!connected_) return;
    try {
        if (ttl_seconds > 0) {
            redis_->set(key, value, std::chrono::seconds(ttl_seconds));
        } else {
            redis_->set(key, value);
        }
    } catch (const sw::redis::Error& e) {
        Logger::instance().error("Redis set error: " + std::string(e.what()));
    }
}

void RedisCache::remove(const std::string& key) {
    if (!connected_) return;
    try {
        redis_->del(key);
    } catch (const sw::redis::Error& e) {
        Logger::instance().error("Redis del error: " + std::string(e.what()));
    }
}

bool RedisCache::exists(const std::string& key) {
    if (!connected_) return false;
    try {
        return redis_->exists(key);
    } catch (const sw::redis::Error& e) {
        Logger::instance().error("Redis exists error: " + std::string(e.what()));
        return false;
    }
}

void RedisCache::flush_all() {
    if (!connected_) return;
    try {
        redis_->flushall();
    } catch (const sw::redis::Error& e) {
        Logger::instance().error("Redis flushall error: " + std::string(e.what()));
    }
}