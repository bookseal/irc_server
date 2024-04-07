#ifndef IRCSERVER_HPP
#define IRCSERVER_HPP
#include "ServerSocket.hpp"
#include <iostream>
#include <iomanip>
#include <unistd.h> // For close function
#include <cstring>  // For memset
#include <sstream>  // For istringstream
#include <pthread.h>
#include <vector>
#include <poll.h>

class IRCServer {
public:
    IRCServer(int port) : serverSocket(port) {};
    void run();
    void initializeServerSocketAndPollFD();
    void waitForConnectionsAndHandleData();
    void acceptNewClients();
    void handleClientEvents();

    void handleClientData(int clientSocket);
    void handleClientWrite(int clientSocket);

//    void printHexRepresentation(const char* buffer, int length);
//    void echoMessages(int socket);
private:
    ServerSocket serverSocket;
    std::vector<struct pollfd> fds;
};

void IRCServer::run() {
    initializeServerSocketAndPollFD();
    waitForConnectionsAndHandleData();
}

void IRCServer::initializeServerSocketAndPollFD() {
    if (serverSocket.initialize() != 0) {
        std::cerr << "Failed to initialize server socket." << std::endl;
        return;
    }

    std::cout << "Server initialized. Waiting for client connections..." << std::endl;

    struct pollfd server_fd;
    server_fd.fd = serverSocket.getFD();
    server_fd.events = POLLIN;
    server_fd.revents = 0;
    fds.push_back(server_fd);
}

void IRCServer::waitForConnectionsAndHandleData() {
    while (true) {
        if (poll(fds.data(), fds.size(), -1) < 0) {
            perror("poll");
            return;
        }
        acceptNewClients();
        handleClientEvents();
    }
}

void IRCServer::acceptNewClients() {
    if (fds[0].revents & POLLIN) {
        int clientSocket = serverSocket.acceptClient();
        if (clientSocket >= 0) {
            struct pollfd client_fd = {clientSocket, POLLIN | POLLOUT, 0};
            fds.push_back(client_fd);
        } else {
            std::cerr << "Failed to accept client connection." << std::endl;
        }
    }
}

void IRCServer::handleClientEvents() {
    for (size_t i = 1; i < fds.size(); ++i) {
        if (fds[i].revents & POLLIN) {
            handleClientData(fds[i].fd);
        }
        if (fds[i].revents & POLLOUT) {
            handleClientWrite(fds[i].fd);
        }
        if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            close(fds[i].fd); // 연결 종료 및 에러 처리
            fds.erase(fds.begin() + i);
            --i;
        }
    }
}

void IRCServer::handleClientData(int clientSocket) {
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));

    // Read data from the client socket
    ssize_t bytesRead = read(clientSocket, buffer, sizeof(buffer) - 1);
    if (bytesRead > 0) {
        // Process the received data
        std::cout << "Received data: " << buffer << std::endl;
        // Here you would typically parse the received data and react accordingly
    } else if (bytesRead == 0) {
        // The client has closed the connection
        std::cout << "Client disconnected." << std::endl;
        close(clientSocket); // Close the socket
        // Here you should also remove the socket from your list of monitored FDs in the poll() loop
    } else {
        // An error occurred
        perror("Error reading from client socket");
        close(clientSocket);
        // Similar cleanup as above
    }
}

void IRCServer::handleClientWrite(int clientSocket) {
    const char* message = "Message from server\n";

    ssize_t bytesWritten = write(clientSocket, message, strlen(message));
    if (bytesWritten > 0) {
        std::cout << "Sent message to client." << std::endl;
    } else {
        perror("Error writing to client socket");
        close(clientSocket);
    }
}
//
//void IRCServer::printHexRepresentation(const char *buffer, int length) {
//    for (int i = 0; i < length; ++i) {
//        if (isprint(static_cast<unsigned char>(buffer[i]))) {
//            std::cout << buffer[i];
//        } else {
//            std::cout << std::hex << std::setw(2) << std::setfill('0')
//                      << static_cast<int>(static_cast<unsigned char>(buffer[i]))
//                      << std::dec << " ";
//        }
//    }
//}
//
//void IRCServer::echoMessages(int socket) {
//    char buffer[1024] = {0};
//    std::cout << "Welcome message sent\n";
//
//    while (true) {
//        int read_val = read(socket, buffer, sizeof(buffer) - 1);
//        if (read_val > 0) {
//            buffer[read_val] = '\0';
//            std::cout << "Received: [";
//            printHexRepresentation(buffer, read_val);
//            std::cout << "]" << std::endl;
//            send(socket, buffer, read_val, 0);
//        } else if (read_val == 0) {
//            std::cout << "Client disconnected." << std::endl;
//            break;
//        } else {
//            perror("read failed");
//            break;
//        }
//    }
//}

#endif