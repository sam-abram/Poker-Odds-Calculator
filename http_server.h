#pragma once
// Minimal localhost HTTP/1.1 server for the poker-odds dev UI (Windows/winsock).
// Thread-per-connection, "Connection: close" per request — small and robust for
// a browser talking to 127.0.0.1. Not intended for public hosting.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdint>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <thread>

namespace http {

struct Request {
    std::string method;
    std::string path;                            // decoded path, no query
    std::map<std::string, std::string> query;    // decoded query parameters
};

struct Response {
    int status = 200;
    std::string content_type = "text/plain";
    std::string body;
};

using Handler = std::function<Response(const Request&)>;

inline std::string url_decode(const std::string& s) {
    auto hex = [](char h) -> int {
        if (h >= '0' && h <= '9') return h - '0';
        h = static_cast<char>(std::tolower(static_cast<unsigned char>(h)));
        if (h >= 'a' && h <= 'f') return 10 + (h - 'a');
        return -1;
    };
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '+') {
            out += ' ';
        } else if (c == '%' && i + 2 < s.size()) {
            int hi = hex(s[i + 1]), lo = hex(s[i + 2]);
            if (hi >= 0 && lo >= 0) { out += static_cast<char>(hi * 16 + lo); i += 2; }
            else out += c;
        } else {
            out += c;
        }
    }
    return out;
}

inline void parse_query(const std::string& qs, std::map<std::string, std::string>& q) {
    size_t i = 0;
    while (i < qs.size()) {
        size_t amp = qs.find('&', i);
        std::string pair = qs.substr(i, amp == std::string::npos ? std::string::npos : amp - i);
        size_t eq = pair.find('=');
        std::string k = url_decode(eq == std::string::npos ? pair : pair.substr(0, eq));
        std::string v = eq == std::string::npos ? std::string() : url_decode(pair.substr(eq + 1));
        if (!k.empty()) q[k] = v;
        if (amp == std::string::npos) break;
        i = amp + 1;
    }
}

class Server {
public:
    explicit Server(Handler h) : handler_(std::move(h)) {}

    // Bind and serve forever. Returns false only if startup/bind/listen fails.
    bool listen(const char* host, uint16_t port) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;

        SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == INVALID_SOCKET) return false;
        int opt = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&opt), sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, host, &addr.sin_addr);

        if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
            closesocket(s);
            return false;
        }
        if (::listen(s, SOMAXCONN) == SOCKET_ERROR) {
            closesocket(s);
            return false;
        }
        for (;;) {
            SOCKET c = accept(s, nullptr, nullptr);
            if (c == INVALID_SOCKET) continue;
            std::thread(&Server::serve_conn, this, c).detach();
        }
    }

private:
    Handler handler_;

    static const char* status_text(int code) {
        switch (code) {
            case 200: return "OK";
            case 400: return "Bad Request";
            case 404: return "Not Found";
            case 500: return "Internal Server Error";
            default:  return "OK";
        }
    }

    void serve_conn(SOCKET c) {
        std::string data;
        char buf[4096];
        while (data.find("\r\n\r\n") == std::string::npos) {
            int n = recv(c, buf, static_cast<int>(sizeof(buf)), 0);
            if (n <= 0) { closesocket(c); return; }
            data.append(buf, static_cast<size_t>(n));
            if (data.size() > 65536) break;  // header size guard
        }

        Request req;
        size_t le = data.find("\r\n");
        std::string line = (le == std::string::npos) ? data : data.substr(0, le);
        std::istringstream ls(line);
        std::string target;
        ls >> req.method >> target;  // version ignored

        size_t qm = target.find('?');
        std::string path = (qm == std::string::npos) ? target : target.substr(0, qm);
        req.path = url_decode(path);
        if (qm != std::string::npos) parse_query(target.substr(qm + 1), req.query);

        Response res = handler_(req);

        std::ostringstream o;
        o << "HTTP/1.1 " << res.status << " " << status_text(res.status) << "\r\n"
          << "Content-Type: " << res.content_type << "\r\n"
          << "Content-Length: " << res.body.size() << "\r\n"
          << "Cache-Control: no-store\r\n"
          << "Connection: close\r\n\r\n"
          << res.body;
        std::string out = o.str();

        size_t sent = 0;
        while (sent < out.size()) {
            int n = send(c, out.data() + sent, static_cast<int>(out.size() - sent), 0);
            if (n <= 0) break;
            sent += static_cast<size_t>(n);
        }
        closesocket(c);
    }
};

} // namespace http
