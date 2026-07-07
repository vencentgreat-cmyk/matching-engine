#include "Server.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <algorithm>

Server::Server(int port) : port_(port) {}

Server::~Server() {
    stop();
    for (auto& t : clientThreads_) {
        if (t.joinable()) t.join();
    }
    if (serverFd_ >= 0) close(serverFd_);
}

void Server::run() {
    // 创建 socket
    serverFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd_ < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return;
    }

    // 允许端口重用（避免 "Address already in use"）
    int opt = 1;
    setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 绑定端口
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(serverFd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Failed to bind port " << port_ << std::endl;
        return;
    }

    // 开始监听
    if (listen(serverFd_, 10) < 0) {
        std::cerr << "Failed to listen" << std::endl;
        return;
    }

    running_ = true;
    std::cout << "[SERVER] Listening on port " << port_ << std::endl;

    int clientCount = 0;
    while (running_) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int clientFd = accept(serverFd_, (sockaddr*)&clientAddr, &clientLen);

        if (clientFd < 0) {
            if (running_) std::cerr << "Accept failed" << std::endl;
            continue;
        }

        clientCount++;
        std::string clientName = "Trader_" + std::to_string(clientCount);
        std::cout << "[SERVER] " << clientName << " connected" << std::endl;

        // 发送欢迎消息
        sendMsg(clientFd, "Welcome, " + clientName + "! Type HELP for commands.\n");

        // 每个客户端一个线程
        clientThreads_.emplace_back(&Server::handleClient, this, clientFd, clientName);
    }
}

void Server::stop() {
    running_ = false;
    if (serverFd_ >= 0) {
        shutdown(serverFd_, SHUT_RDWR);
        close(serverFd_);
        serverFd_ = -1;
    }
}

void Server::handleClient(int clientFd, const std::string& clientName) {
    char buffer[4096];
    std::string partial;  // 处理不完整的消息

    while (running_) {
        int bytesRead = recv(clientFd, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) {
            std::cout << "[SERVER] " << clientName << " disconnected" << std::endl;
            break;
        }

        buffer[bytesRead] = '\0';
        partial += buffer;

        // 按换行符拆分，处理每一行
        size_t pos;
        while ((pos = partial.find('\n')) != std::string::npos) {
            std::string line = partial.substr(0, pos);
            partial = partial.substr(pos + 1);

            // 去掉 \r（Windows 客户端可能发 \r\n）
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (line.empty()) continue;

            std::string response = processCommand(line, clientName);
            sendMsg(clientFd, response);
        }
    }

    close(clientFd);
}

void Server::sendMsg(int fd, const std::string& msg) {
    send(fd, msg.c_str(), msg.size(), 0);
}

std::string Server::processCommand(const std::string& line,
                                    const std::string& clientName) {
    std::istringstream iss(line);
    std::string command;
    iss >> command;

    for (auto& c : command) c = toupper(c);

    std::ostringstream out;

    if (command == "BUY" || command == "SELL") {
        std::string symbol;
        int qty;
        double price;

        if (!(iss >> symbol >> qty >> price)) {
            out << "Usage: " << command << " <symbol> <qty> <price>\n";
            return out.str();
        }

        for (auto& c : symbol) c = toupper(c);

        Side side = (command == "BUY") ? Side::BUY : Side::SELL;
        Order order{nextId_++, side, symbol, qty, price,
                    std::chrono::steady_clock::now()};

        out << "[ORDER] id=" << order.id << " " << command
            << " " << qty << " " << symbol
            << " @ " << std::fixed << std::setprecision(2) << price << "\n";

        // 服务端也打印
        std::cout << "[" << clientName << "] " << command
                  << " " << qty << " " << symbol
                  << " @ " << std::fixed << std::setprecision(2) << price << std::endl;

        std::vector<Trade> trades;
        {
            std::lock_guard<std::mutex> lock(booksMutex_);
            trades = books_[symbol].addOrder(order);
        }

        for (const auto& t : trades) {
            std::string tradeMsg = "[TRADE] " + std::to_string(t.quantity) + " " + t.symbol
                + " @ " + std::to_string(t.price).substr(0, std::to_string(t.price).find('.') + 3)
                + " | Buyer: Order#" + std::to_string(t.buyOrderId)
                + " Seller: Order#" + std::to_string(t.sellOrderId) + "\n";
            out << tradeMsg;
            std::cout << "  " << tradeMsg;
        }

        if (order.quantity > 0 && !trades.empty()) {
            out << "[PARTIAL] " << order.quantity << " remaining on book\n";
        }

    } else if (command == "CANCEL") {
        int orderId;
        if (!(iss >> orderId)) {
            out << "Usage: CANCEL <order_id>\n";
            return out.str();
        }

        std::lock_guard<std::mutex> lock(booksMutex_);
        bool found = false;
        for (auto& [sym, book] : books_) {
            if (book.cancelOrder(orderId)) {
                out << "[CANCELLED] Order#" << orderId << " (" << sym << ")\n";
                found = true;
                break;
            }
        }
        if (!found) {
            out << "[NOT FOUND] Order#" << orderId << "\n";
        }

    } else if (command == "BOOK") {
        std::string symbol;
        std::lock_guard<std::mutex> lock(booksMutex_);

        // 把 order book 输出到字符串
        std::ostringstream bookOut;
        auto oldBuf = std::cout.rdbuf(bookOut.rdbuf());

        if (iss >> symbol) {
            for (auto& c : symbol) c = toupper(c);
            if (books_.count(symbol)) {
                std::cout << "\n--- " << symbol << " ---";
                books_[symbol].print();
            } else {
                std::cout << "No orders for " << symbol << "\n";
            }
        } else {
            if (books_.empty()) {
                std::cout << "Order book is empty.\n";
            } else {
                for (auto& [sym, book] : books_) {
                    std::cout << "\n--- " << sym << " ---";
                    book.print();
                }
            }
        }

        std::cout.rdbuf(oldBuf);
        out << bookOut.str();

    } else if (command == "HELP") {
        out << "\nCommands:\n"
            << "  BUY <symbol> <qty> <price>\n"
            << "  SELL <symbol> <qty> <price>\n"
            << "  CANCEL <order_id>\n"
            << "  BOOK [symbol]\n"
            << "  HELP\n"
            << "  QUIT\n\n";

    } else if (command == "QUIT" || command == "EXIT") {
        out << "Goodbye.\n";

    } else {
        out << "Unknown command. Type HELP for options.\n";
    }

    return out.str();
}
