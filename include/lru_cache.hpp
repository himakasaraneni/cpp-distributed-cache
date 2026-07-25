#pragma once
#include <cstddef>
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

class LruCache {
public:
    explicit LruCache(std::size_t capacity);
    void put(std::string key, std::string value);
    std::optional<std::string> get(const std::string& key);
    bool erase(const std::string& key);
    std::size_t size() const;
    std::size_t capacity() const;
    std::size_t hits() const;
    std::size_t misses() const;
    std::size_t evictions() const;
private:
    struct Entry {
        std::string value;
        std::list<std::string>::iterator position;
    };
    void touch(std::unordered_map<std::string, Entry>::iterator item);
    const std::size_t capacity_;
    mutable std::mutex mutex_;
    std::list<std::string> recency_;
    std::unordered_map<std::string, Entry> entries_;
    std::size_t hits_{0};
    std::size_t misses_{0};
    std::size_t evictions_{0};
};
