package com.distributedcache;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

public final class LruCacheTest {
    private static String text(byte[] value) {
        return new String(value, StandardCharsets.UTF_8);
    }

    public static void main(String[] args) throws InterruptedException {
        LruCache cache = new LruCache(2);
        cache.put("a", "1".getBytes(StandardCharsets.UTF_8));
        cache.put("b", "2".getBytes(StandardCharsets.UTF_8));
        assert text(cache.get("a").orElseThrow()).equals("1");

        cache.put("c", "3".getBytes(StandardCharsets.UTF_8));
        assert cache.get("b").isEmpty() : "b should be the least-recently-used entry";
        assert text(cache.get("a").orElseThrow()).equals("1");
        assert text(cache.get("c").orElseThrow()).equals("3");
        assert cache.stats().evictions() == 1;

        cache.put("a", "updated".getBytes(StandardCharsets.UTF_8));
        assert text(cache.get("a").orElseThrow()).equals("updated");
        assert cache.remove("a");
        assert !cache.remove("a");

        LruCache concurrent = new LruCache(100);
        List<Thread> workers = new ArrayList<>();
        for (int worker = 0; worker < 8; worker++) {
            int workerId = worker;
            workers.add(Thread.startVirtualThread(() -> {
                for (int i = 0; i < 1_000; i++) {
                    String key = Integer.toString((workerId * 1_000 + i) % 150);
                    concurrent.put(key, key.getBytes(StandardCharsets.UTF_8));
                    concurrent.get(key);
                }
            }));
        }
        for (Thread worker : workers) {
            worker.join();
        }
        assert concurrent.stats().size() <= concurrent.stats().capacity();
        System.out.println("Java LRU cache tests passed");
    }
}
