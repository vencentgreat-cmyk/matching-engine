#pragma once

#include <cerrno>
#include <cstddef>
#include <string>
#include <sys/socket.h>

// 可测试实现：调用 sender(fd, buf, len, flags)
template <typename Sender>
bool sendAllImpl(Sender sender, int fd, const char* data, size_t len, int flags = 0) {
    size_t sentTotal = 0;

    while (sentTotal < len) {
        ssize_t sent = sender(fd, data + sentTotal, len - sentTotal, flags);
        if (sent > 0) {
            sentTotal += static_cast<size_t>(sent);
            continue;
        }

        if (sent < 0 && errno == EINTR) {
            continue;
        }

        return false;
    }

    return true;
}

inline bool sendAll(int fd, const char* data, size_t len, int flags = 0) {
    auto sender = [](int sockfd, const char* buf, size_t n, int sendFlags) -> ssize_t {
        return ::send(sockfd, buf, n, sendFlags);
    };

    return sendAllImpl(sender, fd, data, len, flags);
}

inline bool sendAll(int fd, const std::string& msg, int flags = 0) {
    return sendAll(fd, msg.c_str(), msg.size(), flags);
}
