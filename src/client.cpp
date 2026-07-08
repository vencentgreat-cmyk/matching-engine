#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include <atomic>
#include "NetIO.h"

namespace {
constexpr size_t MAX_FRAME_SIZE = 4096;
}

std::atomic<bool> running{true};

// 单独线程：接收服务器消息并打印
void receiveThread(int sockFd) {
    char buffer[4096];
    std::string partial;

    while (running) {
        int bytesRead = recv(sockFd, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) {
            if (running) {
                std::cout << "\n[DISCONNECTED] Server closed connection." << std::endl;
            }
            running = false;
            break;
        }

        partial.append(buffer, bytesRead);

        if (partial.size() > MAX_FRAME_SIZE && partial.find('\n') == std::string::npos) {
            std::cout << "\n[ERROR] Server message too large (max 4096). Disconnecting." << std::endl;
            running = false;
            break;
        }

        size_t pos;
        while ((pos = partial.find('\n')) != std::string::npos) {
            std::string line = partial.substr(0, pos);
            partial = partial.substr(pos + 1);

            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            std::cout << line << std::endl;
            std::cout << "> " << std::flush;
        }
    }
}

int main(int argc, char* argv[]) {
    std::string host = "localhost";
    int port = 9000;

    if (argc > 1) host = argv[1];
    if (argc > 2) port = std::atoi(argv[2]);

    // 创建 socket 并连接服务器
    int sockFd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockFd < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (host == "localhost") {
        serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    } else {
        serverAddr.sin_addr.s_addr = inet_addr(host.c_str());
    }

    if (connect(sockFd, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Failed to connect to " << host << ":" << port << std::endl;
        close(sockFd);
        return 1;
    }

    std::cout << "Connected to " << host << ":" << port << std::endl;

    // 启动接收线程
    std::thread receiver(receiveThread, sockFd);

    // 主线程：读取用户输入，发送给服务器
    std::string line;
    while (running) {
        if (!std::getline(std::cin, line)) break;
        if (!running) break;

        line += "\n";
        if (!sendAll(sockFd, line)) {
            std::cout << "[ERROR] Failed to send command to server." << std::endl;
            running = false;
            break;
        }

        // 去掉换行符检查是否是 QUIT
        std::string cmd = line.substr(0, line.size() - 1);
        for (auto& c : cmd) c = toupper(c);
        if (cmd == "QUIT" || cmd == "EXIT") {
            running = false;
            break;
        }
    }

    running = false;
    shutdown(sockFd, SHUT_RDWR);
    close(sockFd);
    if (receiver.joinable()) receiver.join();

    return 0;
}
