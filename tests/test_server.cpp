#include <gtest/gtest.h>

#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "NetIO.h"
#include "Server.h"

namespace {

int pickFreePort() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    socklen_t len = sizeof(addr);
    if (getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len) < 0) {
        close(fd);
        return -1;
    }

    int port = ntohs(addr.sin_port);
    close(fd);
    return port;
}

int connectClient(int port, int retries = 80, int retryDelayMs = 25) {
    for (int i = 0; i < retries; ++i) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) return -1;

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(port);

        if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
            return fd;
        }

        close(fd);
        std::this_thread::sleep_for(std::chrono::milliseconds(retryDelayMs));
    }

    return -1;
}

void setRecvTimeout(int fd, int ms) {
    timeval tv{};
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

std::string recvUntilQuiet(int fd, int quietMs = 120, int maxTotalMs = 2500) {
    std::string out;
    setRecvTimeout(fd, 200);

    auto start = std::chrono::steady_clock::now();
    auto lastData = start;

    char buf[4096];
    while (true) {
        int n = recv(fd, buf, sizeof(buf), 0);
        if (n > 0) {
            out.append(buf, n);
            lastData = std::chrono::steady_clock::now();
            continue;
        }

        if (n == 0) {
            break;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            auto now = std::chrono::steady_clock::now();
            int quiet = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(now - lastData).count());
            int total = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());

            if (!out.empty() && quiet >= quietMs) break;
            if (total >= maxTotalMs) break;
            continue;
        }

        break;
    }

    return out;
}

bool sendLine(int fd, const std::string& cmd) {
    return sendAll(fd, cmd + "\n");
}

class RunningServer {
public:
    explicit RunningServer(int port) : port_(port), server_(port_) {
        thread_ = std::thread([this] { server_.run(); });
    }

    ~RunningServer() {
        server_.stop();
        if (thread_.joinable()) thread_.join();
    }

private:
    int port_;
    Server server_;
    std::thread thread_;
};

} // namespace

TEST(NetIO, SendAllImplHandlesPartialAndEintr) {
    struct Step {
        ssize_t ret;
        int err;
    };

    std::vector<Step> steps{{3, 0}, {-1, EINTR}, {2, 0}, {5, 0}};
    size_t idx = 0;
    std::string written;

    auto sender = [&](int, const char* buf, size_t len, int) -> ssize_t {
        if (idx >= steps.size()) {
            errno = EPIPE;
            return -1;
        }

        Step s = steps[idx++];
        if (s.ret < 0) {
            errno = s.err;
            return -1;
        }

        size_t toWrite = static_cast<size_t>(s.ret);
        if (toWrite > len) toWrite = len;
        written.append(buf, toWrite);
        return static_cast<ssize_t>(toWrite);
    };

    const std::string payload = "HELLOWORLD";
    bool ok = sendAllImpl(sender, 0, payload.c_str(), payload.size());

    EXPECT_TRUE(ok);
    EXPECT_EQ(written, payload);
}

TEST(NetIO, SendAllImplFailsOnUnrecoverableError) {
    struct Step {
        ssize_t ret;
        int err;
    };

    std::vector<Step> steps{{4, 0}, {-1, EPIPE}};
    size_t idx = 0;
    std::string written;

    auto sender = [&](int, const char* buf, size_t len, int) -> ssize_t {
        if (idx >= steps.size()) {
            errno = EPIPE;
            return -1;
        }

        Step s = steps[idx++];
        if (s.ret < 0) {
            errno = s.err;
            return -1;
        }

        size_t toWrite = static_cast<size_t>(s.ret);
        if (toWrite > len) toWrite = len;
        written.append(buf, toWrite);
        return static_cast<ssize_t>(toWrite);
    };

    const std::string payload = "HELLOWORLD";
    bool ok = sendAllImpl(sender, 0, payload.c_str(), payload.size());

    EXPECT_FALSE(ok);
    EXPECT_EQ(written, "HELL");
}

TEST(ServerIntegration, OversizedFrameClosesConnection) {
    int port = pickFreePort();
    ASSERT_GT(port, 0);

    RunningServer server(port);

    int fd = connectClient(port);
    ASSERT_GE(fd, 0);

    std::string welcome = recvUntilQuiet(fd);
    EXPECT_NE(welcome.find("Welcome"), std::string::npos);

    std::string oversized(5000, 'A');
    ASSERT_TRUE(sendAll(fd, oversized));

    std::string response;
    bool closed = false;
    setRecvTimeout(fd, 800);

    char buf[4096];
    for (int i = 0; i < 8; ++i) {
        int n = recv(fd, buf, sizeof(buf), 0);
        if (n > 0) {
            response.append(buf, n);
            continue;
        }
        if (n == 0) {
            closed = true;
            break;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            continue;
        }
        break;
    }

    close(fd);

    EXPECT_NE(response.find("Frame too large"), std::string::npos);
    EXPECT_TRUE(closed);
}

TEST(ServerIntegration, ConcurrentBookResponsesAreCompleteAndIsolated) {
    int port = pickFreePort();
    ASSERT_GT(port, 0);

    RunningServer server(port);

    int fd1 = connectClient(port);
    int fd2 = connectClient(port);
    ASSERT_GE(fd1, 0);
    ASSERT_GE(fd2, 0);

    recvUntilQuiet(fd1);
    recvUntilQuiet(fd2);

    ASSERT_TRUE(sendLine(fd1, "BUY AMD 100 145.20"));
    recvUntilQuiet(fd1);

    ASSERT_TRUE(sendLine(fd1, "SELL AMD 70 146.00"));
    recvUntilQuiet(fd1);

    ASSERT_TRUE(sendLine(fd1, "BUY NVDA 80 500.00"));
    recvUntilQuiet(fd1);

    ASSERT_TRUE(sendLine(fd1, "SELL NVDA 60 501.00"));
    recvUntilQuiet(fd1);

    std::string amdResp;
    std::string nvdaResp;

    std::thread t1([&] {
        ASSERT_TRUE(sendLine(fd1, "BOOK AMD"));
        amdResp = recvUntilQuiet(fd1);
    });

    std::thread t2([&] {
        ASSERT_TRUE(sendLine(fd2, "BOOK NVDA"));
        nvdaResp = recvUntilQuiet(fd2);
    });

    t1.join();
    t2.join();

    close(fd1);
    close(fd2);

    EXPECT_NE(amdResp.find("--- AMD ---"), std::string::npos);
    EXPECT_NE(amdResp.find("ASK 146.00 x 70"), std::string::npos);
    EXPECT_NE(amdResp.find("BID 145.20 x 100"), std::string::npos);
    EXPECT_EQ(amdResp.find("NVDA"), std::string::npos);

    EXPECT_NE(nvdaResp.find("--- NVDA ---"), std::string::npos);
    EXPECT_NE(nvdaResp.find("ASK 501.00 x 60"), std::string::npos);
    EXPECT_NE(nvdaResp.find("BID 500.00 x 80"), std::string::npos);
    EXPECT_EQ(nvdaResp.find("AMD"), std::string::npos);
}
