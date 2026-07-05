#include "cache/file_cache.h"

FileCache::FileCache(size_t max_size, int ttl_seconds)
    : max_size_(max_size),
      current_size_(0),
      ttl_seconds_(ttl_seconds),
      hit_count_(0),
      miss_count_(0) {
}

bool FileCache::get(const std::string& key, std::string& content, std::string& content_type) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_map_.find(key);
    if (it == cache_map_.end()) {
        miss_count_++;
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    auto& entry = it->second.first;

    if (ttl_seconds_ > 0) {
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - entry.created_time);
        if (age.count() > ttl_seconds_) {
            lru_list_.erase(it->second.second);
            cache_map_.erase(it);
            miss_count_++;
            return false;
        }
    }

    entry.last_access_time = now;
    entry.access_count++;

    lru_list_.erase(it->second.second);
    lru_list_.push_front(key);
    it->second.second = lru_list_.begin();

    content = entry.content;
    content_type = entry.content_type;
    hit_count_++;

    return true;
}

void FileCache::put(const std::string& key, const std::string& content, const std::string& content_type) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_map_.find(key);
    if (it != cache_map_.end()) {
        current_size_ -= it->second.first.content.size();
        lru_list_.erase(it->second.second);
        cache_map_.erase(it);
    }

    size_t entry_size = content.size() + key.size();
    while (current_size_ + entry_size > max_size_ && !cache_map_.empty()) {
        evict();
    }

    CacheEntry entry;
    entry.content = content;
    entry.content_type = content_type;
    entry.last_access_time = std::chrono::steady_clock::now();
    entry.created_time = entry.last_access_time;
    entry.access_count = 1;

    lru_list_.push_front(key);
    cache_map_[key] = std::make_pair(entry, lru_list_.begin());
    current_size_ += entry_size;
}

void FileCache::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_map_.find(key);
    if (it != cache_map_.end()) {
        current_size_ -= it->second.first.content.size();
        lru_list_.erase(it->second.second);
        cache_map_.erase(it);
    }
}

void FileCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);

    lru_list_.clear();
    cache_map_.clear();
    current_size_ = 0;
    hit_count_ = 0;
    miss_count_ = 0;
}

size_t FileCache::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_map_.size();
}

double FileCache::hit_rate() const {
    size_t total = hit_count_ + miss_count_;
    if (total == 0) return 0.0;
    return static_cast<double>(hit_count_) / total;
}

void FileCache::evict() {
    if (lru_list_.empty()) return;

    std::string key = lru_list_.back();
    lru_list_.pop_back();

    auto it = cache_map_.find(key);
    if (it != cache_map_.end()) {
        current_size_ -= it->second.first.content.size();
        cache_map_.erase(it);
    }
}