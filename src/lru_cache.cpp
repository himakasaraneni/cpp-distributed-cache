#include "lru_cache.hpp"
#include <stdexcept>
#include <utility>

LruCache::LruCache(std::size_t capacity) : capacity_(capacity) {
    if (capacity == 0) throw std::invalid_argument("capacity must be greater than zero");
}
void LruCache::touch(std::unordered_map<std::string, Entry>::iterator item) {
    recency_.splice(recency_.begin(), recency_, item->second.position);
    item->second.position = recency_.begin();
}
void LruCache::put(std::string key, std::string value) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto existing = entries_.find(key);
    if (existing != entries_.end()) {
        existing->second.value = std::move(value);
        touch(existing);
        return;
    }
    if (entries_.size() == capacity_) {
        entries_.erase(recency_.back());
        recency_.pop_back();
        ++evictions_;
    }
    recency_.push_front(key);
    entries_.emplace(std::move(key), Entry{std::move(value), recency_.begin()});
}
std::optional<std::string> LruCache::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto item = entries_.find(key);
    if (item == entries_.end()) {
        ++misses_;
        return std::nullopt;
    }
    ++hits_;
    touch(item);
    return item->second.value;
}
bool LruCache::erase(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto item = entries_.find(key);
    if (item == entries_.end()) return false;
    recency_.erase(item->second.position);
    entries_.erase(item);
    return true;
}
std::size_t LruCache::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
}
std::size_t LruCache::capacity() const { return capacity_; }
std::size_t LruCache::hits() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return hits_;
}
std::size_t LruCache::misses() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return misses_;
}
std::size_t LruCache::evictions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return evictions_;
}
