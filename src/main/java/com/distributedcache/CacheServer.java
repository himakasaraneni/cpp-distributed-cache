package com.distributedcache;

import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpServer;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.URLDecoder;
import java.nio.charset.StandardCharsets;
import java.util.Optional;
import java.util.concurrent.Executors;

public final class CacheServer {
    private static final int MAX_VALUE_BYTES = 1024 * 1024;

    private final String nodeId;
    private final LruCache cache;
    private final HttpServer server;

    public CacheServer(int port, int capacity, String nodeId) throws IOException {
        this.nodeId = nodeId;
        this.cache = new LruCache(capacity);
        this.server = HttpServer.create(new InetSocketAddress(port), 256);
        this.server.createContext("/health", this::handleHealth);
        this.server.createContext("/stats", this::handleStats);
        this.server.createContext("/cache", this::handleCache);
        this.server.setExecutor(Executors.newVirtualThreadPerTaskExecutor());
    }

    public void start() {
        server.start();
    }

    private void handleHealth(HttpExchange exchange) throws IOException {
        if (!"GET".equals(exchange.getRequestMethod())) {
            send(exchange, 405, new byte[0], "text/plain");
            return;
        }
        String json = "{\"status\":\"ok\",\"node\":\"" + escapeJson(nodeId) + "\"}";
        send(exchange, 200, json.getBytes(StandardCharsets.UTF_8), "application/json");
    }

    private void handleStats(HttpExchange exchange) throws IOException {
        if (!"GET".equals(exchange.getRequestMethod())) {
            send(exchange, 405, new byte[0], "text/plain");
            return;
        }
        LruCache.Stats stats = cache.stats();
        String json = "{\"node\":\"" + escapeJson(nodeId) + "\",\"size\":" + stats.size()
                + ",\"capacity\":" + stats.capacity() + ",\"hits\":" + stats.hits()
                + ",\"misses\":" + stats.misses() + ",\"evictions\":"
                + stats.evictions() + "}";
        send(exchange, 200, json.getBytes(StandardCharsets.UTF_8), "application/json");
    }

    private void handleCache(HttpExchange exchange) throws IOException {
        String rawPath = exchange.getRequestURI().getRawPath();
        if (!rawPath.startsWith("/cache/") || rawPath.length() <= "/cache/".length()) {
            send(exchange, 404, new byte[0], "text/plain");
            return;
        }

        String key = URLDecoder.decode(
                rawPath.substring("/cache/".length()), StandardCharsets.UTF_8);
        switch (exchange.getRequestMethod()) {
            case "PUT" -> {
                byte[] value = exchange.getRequestBody().readNBytes(MAX_VALUE_BYTES + 1);
                if (value.length > MAX_VALUE_BYTES) {
                    send(exchange, 413, "value exceeds 1 MiB".getBytes(StandardCharsets.UTF_8),
                            "text/plain");
                    return;
                }
                cache.put(key, value);
                send(exchange, 201, new byte[0], "text/plain");
            }
            case "GET" -> {
                Optional<byte[]> value = cache.get(key);
                if (value.isPresent()) {
                    send(exchange, 200, value.get(), "application/octet-stream");
                } else {
                    send(exchange, 404, new byte[0], "text/plain");
                }
            }
            case "DELETE" -> send(exchange, cache.remove(key) ? 204 : 404,
                    new byte[0], "text/plain");
            default -> send(exchange, 405, new byte[0], "text/plain");
        }
    }

    private static void send(HttpExchange exchange, int status, byte[] body, String contentType)
            throws IOException {
        exchange.getResponseHeaders().set("Content-Type", contentType);
        if (status == 204) {
            exchange.sendResponseHeaders(status, -1);
        } else {
            exchange.sendResponseHeaders(status, body.length);
            exchange.getResponseBody().write(body);
        }
        exchange.close();
    }

    private static String escapeJson(String value) {
        return value.replace("\\", "\\\\").replace("\"", "\\\"");
    }

    public static void main(String[] args) throws IOException {
        int port = Integer.parseInt(System.getenv().getOrDefault("PORT", "8080"));
        int capacity = Integer.parseInt(System.getenv().getOrDefault("CAPACITY", "10000"));
        String nodeId = System.getenv().getOrDefault("NODE_ID", "local");

        CacheServer server = new CacheServer(port, capacity, nodeId);
        server.start();
        System.out.printf("Java cache node %s listening on %d with capacity %d%n",
                nodeId, port, capacity);
    }
}
