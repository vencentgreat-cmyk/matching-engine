#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <thread>
#include <atomic>

std::atomic<bool> running{true};

// 单独线程：接收服务器消息并打印
void receiveThread(int sockFd) {
    char buffer[4096];
    while (running) {
        int bytesRead = recv(sockFd, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) {
            if (running) {
                std::cout << "\n[DISCONNECTED] Server closed connection." << std::endl;
            }
            running = false;
            break;
        }
        buffer[bytesRead] = '\0';
        std::cout << buffer;
        std::cout << "> " << std::flush;
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
        send(sockFd, line.c_str(), line.size(), 0);

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
