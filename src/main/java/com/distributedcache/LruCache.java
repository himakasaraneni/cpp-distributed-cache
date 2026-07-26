package com.distributedcache;

import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Optional;

/**
 * Thread-safe, bounded least-recently-used cache.
 *
 * LinkedHashMap's access-order mode keeps the least recently accessed entry at
 * the beginning of the map. All compound operations and metrics are protected
 * by this object's monitor.
 */
public final class LruCache {
    private final int capacity;
    private final LinkedHashMap<String, byte[]> entries;
    private long hits;
    private long misses;
    private long evictions;

    public LruCache(int capacity) {
        if (capacity <= 0) {
            throw new IllegalArgumentException("capacity must be greater than zero");
        }
        this.capacity = capacity;
        this.entries = new LinkedHashMap<>(capacity, 0.75f, true);
    }

    public synchronized void put(String key, byte[] value) {
        if (!entries.containsKey(key) && entries.size() == capacity) {
            String leastRecentlyUsed = entries.keySet().iterator().next();
            entries.remove(leastRecentlyUsed);
            evictions++;
        }
        entries.put(key, value.clone());
    }

    public synchronized Optional<byte[]> get(String key) {
        byte[] value = entries.get(key);
        if (value == null) {
            misses++;
            return Optional.empty();
        }
        hits++;
        return Optional.of(value.clone());
    }

    public synchronized boolean remove(String key) {
        return entries.remove(key) != null;
    }

    public synchronized Stats stats() {
        return new Stats(entries.size(), capacity, hits, misses, evictions);
    }

    public record Stats(int size, int capacity, long hits, long misses, long evictions) {}
}
