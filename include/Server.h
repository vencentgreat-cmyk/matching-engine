#pragma once
#include <string>
#include <map>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <netinet/in.h>
#include "OrderBook.h"

class Server {
public:
    Server(int port);
    ~Server();

    // 启动服务器，阻塞直到停止
    void run();
    void stop();

private:
    int port_;
    int serverFd_ = -1;
    std::atomic<bool> running_{false};

    // 所有 symbol 的 order book，加锁保护
    std::map<std::string, OrderBook> books_;
    std::mutex booksMutex_;

    // 订单 ID 生成器
    std::atomic<int> nextId_{1};

    // 已连接的客户端
    std::vector<std::thread> clientThreads_;

    // 处理单个客户端连接
    void handleClient(int clientFd, const std::string& clientName);

    // 发送消息给客户端
    void sendMsg(int fd, const std::string& msg);

    // 处理一行命令，返回响应
    std::string processCommand(const std::string& line, const std::string& clientName);
};
