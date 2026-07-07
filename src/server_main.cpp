#include <iostream>
#include <cstdlib>
#include "Server.h"

int main(int argc, char* argv[]) {
    int port = 9000;

    if (argc > 1) {
        port = std::atoi(argv[1]);
    }

    std::cout << "=== Matching Engine Server ===" << std::endl;
    Server server(port);
    server.run();

    return 0;
}
