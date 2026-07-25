#include "lru_cache.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {
volatile std::sig_atomic_t running = 1;
void stop_server(int) { running = 0; }
std::string response(int status, const std::string& body,
                     const std::string& type = "text/plain") {
    const char* reason = status == 200 ? "OK" : status == 201 ? "Created"
                         : status == 204 ? "No Content" : status == 404 ? "Not Found"
                         : "Bad Request";
    std::ostringstream out;
    out << "HTTP/1.1 " << status << ' ' << reason << "\r\nContent-Type: " << type
        << "\r\nContent-Length: " << body.size()
        << "\r\nConnection: close\r\n\r\n" << body;
    return out.str();
}
void send_all(int socket, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        const auto count = ::send(socket, data.data() + sent, data.size() - sent, 0);
        if (count <= 0) return;
        sent += static_cast<std::size_t>(count);
    }
}
void handle_client(int client, LruCache& cache, const std::string& node_id) {
    std::string request;
    char buffer[4096];
    std::size_t body_length = 0;
    std::size_t header_end = std::string::npos;
    while (request.size() < 1024 * 1024) {
        const auto count = recv(client, buffer, sizeof(buffer), 0);
        if (count <= 0) break;
        request.append(buffer, static_cast<std::size_t>(count));
        header_end = request.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            const auto marker = request.find("Content-Length:");
            if (marker != std::string::npos && marker < header_end)
                body_length = std::stoull(request.substr(marker + 15));
            if (request.size() >= header_end + 4 + body_length) break;
        }
    }
    std::istringstream first_line(request);
    std::string method, path;
    first_line >> method >> path;
    std::string result;
    if (path == "/health" && method == "GET") {
        result = response(200, "{\"status\":\"ok\",\"node\":\"" + node_id + "\"}",
                          "application/json");
    } else if (path == "/stats" && method == "GET") {
        std::ostringstream json;
        json << "{\"node\":\"" << node_id << "\",\"size\":" << cache.size()
             << ",\"capacity\":" << cache.capacity() << ",\"hits\":" << cache.hits()
             << ",\"misses\":" << cache.misses()
             << ",\"evictions\":" << cache.evictions() << '}';
        result = response(200, json.str(), "application/json");
    } else if (path.rfind("/cache/", 0) == 0 && path.size() > 7) {
        const std::string key = path.substr(7);
        if (method == "PUT" && header_end != std::string::npos) {
            cache.put(key, request.substr(header_end + 4, body_length));
            result = response(201, "");
        } else if (method == "GET") {
            auto value = cache.get(key);
            result = value ? response(200, *value) : response(404, "");
        } else if (method == "DELETE") {
            result = cache.erase(key) ? response(204, "") : response(404, "");
        } else result = response(400, "unsupported method");
    } else result = response(404, "");
    send_all(client, result);
    close(client);
}
}

int main() {
    const int port = std::getenv("PORT") ? std::stoi(std::getenv("PORT")) : 8080;
    const std::size_t capacity =
        std::getenv("CAPACITY") ? std::stoull(std::getenv("CAPACITY")) : 10000;
    const std::string node_id = std::getenv("NODE_ID") ? std::getenv("NODE_ID") : "local";
    LruCache cache(capacity);
    const int server = socket(AF_INET, SOCK_STREAM, 0);
    int reuse = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(static_cast<uint16_t>(port));
    if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 ||
        listen(server, 256) < 0) {
        std::cerr << "failed to listen: " << std::strerror(errno) << '\n';
        return 1;
    }
    std::signal(SIGINT, stop_server);
    std::signal(SIGTERM, stop_server);
    std::cout << "node " << node_id << " listening on " << port << '\n';
    while (running) {
        const int client = accept(server, nullptr, nullptr);
        if (client >= 0)
            std::thread(handle_client, client, std::ref(cache), std::cref(node_id)).detach();
        else if (errno != EINTR) break;
    }
    close(server);
}
