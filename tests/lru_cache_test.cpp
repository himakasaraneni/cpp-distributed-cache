#include "lru_cache.hpp"
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>
int main() {
    LruCache cache(2);
    cache.put("a", "1"); cache.put("b", "2");
    assert(cache.get("a") == "1");
    cache.put("c", "3");
    assert(!cache.get("b"));
    assert(cache.get("a") == "1" && cache.get("c") == "3");
    assert(cache.evictions() == 1);
    cache.put("a", "updated");
    assert(cache.get("a") == "updated");
    assert(cache.erase("a") && !cache.erase("a"));
    LruCache concurrent(100);
    std::vector<std::thread> workers;
    for (int worker = 0; worker < 8; ++worker)
        workers.emplace_back([worker, &concurrent] {
            for (int i = 0; i < 1000; ++i) {
                auto key = std::to_string((worker * 1000 + i) % 150);
                concurrent.put(key, key); concurrent.get(key);
            }
        });
    for (auto& worker : workers) worker.join();
    assert(concurrent.size() <= concurrent.capacity());
    std::cout << "LRU cache tests passed\n";
}
