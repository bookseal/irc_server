#ifndef SERVERSOCKET_HPP
#define SERVERSOCKET_HPP
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <iomanip>
#include <string>
#include <sstream>

class ServerSocket {
public:
    ServerSocket();
    ServerSocket(int port = 6667) {
        this->port = port;

    }; // Default port is 6667
    ~ServerSocket();
    int initialize();
    int acceptClient();
    int getFD() const;

private:
    int server_fd;
    struct sockaddr_in address;
    int port;
};

ServerSocket::ServerSocket() : server_fd(-1), port(6667) {
    memset(&address, 0, sizeof(address));
}

ServerSocket::~ServerSocket() {
    if (server_fd != -1) {
        close(server_fd);
    }
}

int ServerSocket::initialize() {
    int opt = 1;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("socket failed");
        return -1; // Return an error code instead of exiting
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        return -1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        return -1;
    }
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        return -1;
    }
    return 0; // Success
}

int ServerSocket::acceptClient() {
    socklen_t addrlen = sizeof(address);
    int new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
    if (new_socket < 0) {
        perror("accept");
        return -1; // Return an error code instead of exiting
    }
    return new_socket;
}

int ServerSocket::getFD() const {
    return server_fd;
}

#endif