// net.cpp — Sapphire Net Module (v1.0.9)
// Low-level TCP/UDP networking via POSIX sockets (Winsock on Windows).
// No new external dependencies — uses the OS socket API directly.
//
// Exposed as Sapphire global functions:
//   Net.tcpConnect(host, port)           -> handle (number) or nil on error
//   Net.tcpSend(handle, data)            -> bytes sent (number)
//   Net.tcpReceive(handle, maxBytes)     -> string of received data
//   Net.tcpClose(handle)                 -> bool
//   Net.resolve(hostname)                -> ip string or nil
//   Net.localIP()                        -> string
//   Net.isPortOpen(host, port, timeoutMs)-> bool
#include "builtins.h"
#include "../object.h"
#include "../value.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <cstring>
#include <cstdint>
#include <sstream>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "Ws2_32.lib")
    using sock_t = SOCKET;
    static constexpr sock_t INVALID_SOCK = INVALID_SOCKET;
    #define CLOSE_SOCK(s) closesocket(s)
    #define GET_LAST_ERR() WSAGetLastError()
#else
    #include <sys/socket.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/select.h>
    #include <errno.h>
    using sock_t = int;
    static constexpr sock_t INVALID_SOCK = -1;
    #define CLOSE_SOCK(s) ::close(s)
    #define GET_LAST_ERR() errno
#endif

// ─────────────────────────────────────────────
// Socket handle registry (thread-safe)
// ─────────────────────────────────────────────

static std::mutex         g_sock_mutex;
static std::unordered_map<int, sock_t> g_sockets;
static int                g_next_handle = 1;

static int register_socket(sock_t s) {
    std::lock_guard<std::mutex> lock(g_sock_mutex);
    int handle = g_next_handle++;
    g_sockets[handle] = s;
    return handle;
}

static sock_t get_socket(int handle) {
    std::lock_guard<std::mutex> lock(g_sock_mutex);
    auto it = g_sockets.find(handle);
    if (it == g_sockets.end()) return INVALID_SOCK;
    return it->second;
}

static void remove_socket(int handle) {
    std::lock_guard<std::mutex> lock(g_sock_mutex);
    g_sockets.erase(handle);
}

// ─────────────────────────────────────────────
// Platform init (Winsock on Windows)
// ─────────────────────────────────────────────

static bool g_net_initialized = false;

static bool net_init() {
    if (g_net_initialized) return true;
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
#endif
    g_net_initialized = true;
    return true;
}

// ─────────────────────────────────────────────
// Net.tcpConnect(host, port) -> handle or nil
// ─────────────────────────────────────────────

SapphireValue native_net_tcp_connect(int arg_count, SapphireValue* args) {
    if (arg_count != 2 ||
        !is_obj_type(args[0], OBJ_STRING) ||
        args[1].type != ValType::VAL_NUMBER)
        return {};

    if (!net_init()) return {};

    std::string host = static_cast<ObjString*>(args[0].as.obj)->chars;
    int port = static_cast<int>(args[1].as.number);

    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    std::string port_str = std::to_string(port);
    if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0 || !res)
        return {};

    sock_t s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == INVALID_SOCK) { freeaddrinfo(res); return {}; }

    if (connect(s, res->ai_addr, static_cast<int>(res->ai_addrlen)) != 0) {
        CLOSE_SOCK(s);
        freeaddrinfo(res);
        return {};
    }
    freeaddrinfo(res);

    int handle = register_socket(s);
    return static_cast<double>(handle);
}

// ─────────────────────────────────────────────
// Net.tcpSend(handle, data) -> bytes_sent
// ─────────────────────────────────────────────

SapphireValue native_net_tcp_send(int arg_count, SapphireValue* args) {
    if (arg_count != 2 ||
        args[0].type != ValType::VAL_NUMBER ||
        !is_obj_type(args[1], OBJ_STRING))
        return 0.0;

    int handle = static_cast<int>(args[0].as.number);
    sock_t s = get_socket(handle);
    if (s == INVALID_SOCK) return 0.0;

    std::string data = static_cast<ObjString*>(args[1].as.obj)->chars;

#ifdef _WIN32
    int sent = send(s, data.data(), static_cast<int>(data.size()), 0);
#else
    ssize_t sent = ::send(s, data.data(), data.size(), 0);
#endif
    if (sent < 0) return 0.0;
    return static_cast<double>(sent);
}

// ─────────────────────────────────────────────
// Net.tcpReceive(handle, maxBytes) -> string
// ─────────────────────────────────────────────

SapphireValue native_net_tcp_receive(int arg_count, SapphireValue* args) {
    if (arg_count < 1 || args[0].type != ValType::VAL_NUMBER) return {};

    int handle   = static_cast<int>(args[0].as.number);
    int max_bytes = (arg_count >= 2 && args[1].type == ValType::VAL_NUMBER)
                    ? static_cast<int>(args[1].as.number) : 4096;
    if (max_bytes <= 0 || max_bytes > 65536) max_bytes = 4096;

    sock_t s = get_socket(handle);
    if (s == INVALID_SOCK) return {};

    std::vector<char> buf(static_cast<size_t>(max_bytes));
#ifdef _WIN32
    int received = recv(s, buf.data(), max_bytes, 0);
#else
    ssize_t received = recv(s, buf.data(), static_cast<size_t>(max_bytes), 0);
#endif
    if (received <= 0) return {};

    return new_string(g_current_vm, std::string(buf.data(), static_cast<size_t>(received)));
}

// ─────────────────────────────────────────────
// Net.tcpClose(handle) -> bool
// ─────────────────────────────────────────────

SapphireValue native_net_tcp_close(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || args[0].type != ValType::VAL_NUMBER) return false;
    int handle = static_cast<int>(args[0].as.number);
    sock_t s = get_socket(handle);
    if (s == INVALID_SOCK) return false;
    CLOSE_SOCK(s);
    remove_socket(handle);
    return true;
}

// ─────────────────────────────────────────────
// Net.resolve(hostname) -> ip string or nil
// ─────────────────────────────────────────────

SapphireValue native_net_resolve(int arg_count, SapphireValue* args) {
    if (arg_count != 1 || !is_obj_type(args[0], OBJ_STRING)) return {};
    if (!net_init()) return {};

    std::string host = static_cast<ObjString*>(args[0].as.obj)->chars;

    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_UNSPEC;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0 || !res) return {};

    char ip[INET6_ADDRSTRLEN] = {};
    if (res->ai_family == AF_INET) {
        inet_ntop(AF_INET,
            &reinterpret_cast<struct sockaddr_in*>(res->ai_addr)->sin_addr,
            ip, sizeof(ip));
    } else if (res->ai_family == AF_INET6) {
        inet_ntop(AF_INET6,
            &reinterpret_cast<struct sockaddr_in6*>(res->ai_addr)->sin6_addr,
            ip, sizeof(ip));
    }
    freeaddrinfo(res);

    if (ip[0] == '\0') return {};
    return new_string(g_current_vm, std::string(ip));
}

// ─────────────────────────────────────────────
// Net.localIP() -> string
// ─────────────────────────────────────────────

SapphireValue native_net_local_ip(int arg_count, SapphireValue* args) {
    if (!net_init()) return new_string(g_current_vm, "127.0.0.1");

    char hostname[256] = {};
    if (gethostname(hostname, sizeof(hostname)) != 0)
        return new_string(g_current_vm, "127.0.0.1");

    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET; // prefer IPv4
    if (getaddrinfo(hostname, nullptr, &hints, &res) != 0 || !res)
        return new_string(g_current_vm, "127.0.0.1");

    char ip[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET,
        &reinterpret_cast<struct sockaddr_in*>(res->ai_addr)->sin_addr,
        ip, sizeof(ip));
    freeaddrinfo(res);

    return new_string(g_current_vm, std::string(ip));
}

// ─────────────────────────────────────────────
// Net.isPortOpen(host, port, timeoutMs) -> bool
// ─────────────────────────────────────────────

SapphireValue native_net_is_port_open(int arg_count, SapphireValue* args) {
    if (arg_count < 2 ||
        !is_obj_type(args[0], OBJ_STRING) ||
        args[1].type != ValType::VAL_NUMBER)
        return false;

    if (!net_init()) return false;

    std::string host    = static_cast<ObjString*>(args[0].as.obj)->chars;
    int         port    = static_cast<int>(args[1].as.number);
    int         timeout = (arg_count >= 3 && args[2].type == ValType::VAL_NUMBER)
                          ? static_cast<int>(args[2].as.number) : 2000;

    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    std::string port_str = std::to_string(port);
    if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0 || !res)
        return false;

    sock_t s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == INVALID_SOCK) { freeaddrinfo(res); return false; }

    // Set non-blocking for timeout support
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);
#else
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif

    connect(s, res->ai_addr, static_cast<int>(res->ai_addrlen));
    freeaddrinfo(res);

    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(s, &wfds);

    struct timeval tv;
    tv.tv_sec  = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    int result = select(static_cast<int>(s) + 1, nullptr, &wfds, nullptr, &tv);
    CLOSE_SOCK(s);

    return result > 0;
}
