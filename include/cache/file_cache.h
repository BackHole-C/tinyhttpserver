#ifndef FILE_CACHE_H
#define FILE_CACHE_H

#include <string>
#include <unordered_map>
#include <list>
#include <mutex>
#include <chrono>
#include <atomic>

struct CacheEntry {
    std::string content;
    std::string content_type;
    std::chrono::steady_clock::time_point last_access_time;
    std::chrono::steady_clock::time_point created_time;
    size_t access_count;
};

class FileCache {
public:
    FileCache(size_t max_size = 1024 * 1024 * 100, int ttl_seconds = 300);
    ~FileCache() = default;

    FileCache(const FileCache&) = delete;
    FileCache& operator=(const FileCache&) = delete;

    bool get(const std::string& key, std::string& content, std::string& content_type);
    void put(const std::string& key, const std::string& content, const std::string& content_type);
    void remove(const std::string& key);
    void clear();
    size_t size() const;
    size_t hit_count() const { return hit_count_; }
    size_t miss_count() const { return miss_count_; }
    double hit_rate() const;

private:
    void evict();

    size_t max_size_;
    size_t current_size_;
    int ttl_seconds_;

    std::list<std::string> lru_list_;
    std::unordered_map<std::string, std::pair<CacheEntry, typename std::list<std::string>::iterator>> cache_map_;
    mutable std::mutex mutex_;

    std::atomic<size_t> hit_count_;
    std::atomic<size_t> miss_count_;
};

#endif // FILE_CACHE_H