#include "Server.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <algorithm>
#include "NetIO.h"
#include <cerrno>

namespace {
constexpr size_t MAX_FRAME_SIZE = 4096;
}

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

        partial.append(buffer, bytesRead);

        if (partial.size() > MAX_FRAME_SIZE && partial.find('\n') == std::string::npos) {
            sendMsg(clientFd, "[ERROR] Frame too large (max 4096). Closing connection.\n");
            break;
        }

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
            if (!sendMsg(clientFd, response)) {
                break;
            }
        }
    }

    close(clientFd);
}

bool Server::sendMsg(int fd, const std::string& msg) {
    if (msg.empty()) return true;
    return sendAll(fd, msg);
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

        if (symbol.empty()) {
            out << "[ERROR] Symbol required\n";
            return out.str();
        }

        if (qty <= 0) {
            out << "[ERROR] Quantity must be positive\n";
            return out.str();
        }

        if (price <= 0) {
            out << "[ERROR] Price must be positive for limit orders\n";
            return out.str();
        }

        Side side = (command == "BUY") ? Side::BUY : Side::SELL;
        Order order{nextId_++, side, symbol, qty, price,
                    std::chrono::steady_clock::now()};
        order.type = OrderType::LIMIT;

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

    } else if (command == "MARKET") {
        std::string sideStr;
        std::string symbol;
        int qty;

        if (!(iss >> sideStr >> symbol >> qty)) {
            out << "Usage: MARKET BUY/SELL <symbol> <qty>\n";
            return out.str();
        }

        for (auto& c : sideStr) c = toupper(c);
        for (auto& c : symbol) c = toupper(c);

        if (sideStr != "BUY" && sideStr != "SELL") {
            out << "Usage: MARKET BUY/SELL <symbol> <qty>\n";
            return out.str();
        }

        if (symbol.empty()) {
            out << "[ERROR] Symbol required\n";
            return out.str();
        }

        if (qty <= 0) {
            out << "[ERROR] Quantity must be positive\n";
            return out.str();
        }

        Side side = (sideStr == "BUY") ? Side::BUY : Side::SELL;
        Order order{nextId_++, side, symbol, qty, 0.0,
                    std::chrono::steady_clock::now()};
        order.type = OrderType::MARKET;

        out << "[MARKET ORDER] id=" << order.id << " " << sideStr
            << " " << qty << " " << symbol << "\n";

        std::cout << "[" << clientName << "] MARKET " << sideStr
                  << " " << qty << " " << symbol << std::endl;

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

        if (trades.empty()) {
            out << "[WARNING] No liquidity — order book is empty for " << symbol << "\n";
        } else if (order.quantity > 0) {
            out << "[UNFILLED] " << order.quantity << " shares dropped (IOC)\n";
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

        std::ostringstream bookOut;

        if (iss >> symbol) {
            for (auto& c : symbol) c = toupper(c);
            if (books_.count(symbol)) {
                bookOut << "\n--- " << symbol << " ---";
                books_[symbol].print(bookOut);
            } else {
                bookOut << "No orders for " << symbol << "\n";
            }
        } else {
            if (books_.empty()) {
                bookOut << "Order book is empty.\n";
            } else {
                for (auto& [sym, book] : books_) {
                    bookOut << "\n--- " << sym << " ---";
                    book.print(bookOut);
                }
            }
        }

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
