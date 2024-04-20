#ifndef IRCSERVER_HPP
#define IRCSERVER_HPP
#include "ServerSocket.hpp"
#include <iostream>
#include <iomanip>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <pthread.h>
#include <vector>
#include <poll.h>
#include <queue>
#include <map>


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
    size_t findClientSocketIndex(int clientSocket);
    void handleNickCommand(int clientSocket, const std::string& nickname);

private:
    ServerSocket serverSocket;
    std::vector<struct pollfd> fds;
    std::vector<std::queue<std::string> > messageQueues;
    std::map<int, std::string> clientNicknames;
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
            std::cout << "New client connected, socket fd: " << clientSocket << std::endl;

            // Prepare the client's pollfd structure
            struct pollfd client_fd = {clientSocket, POLLIN | POLLOUT, 0};
            fds.push_back(client_fd);

            // Create a message queue for the new client
            messageQueues.push_back(std::queue<std::string>());

            // Send a welcome message to the new client
            std::string welcomeMessage = ":Server NOTICE AUTH :Welcome to the IRC Server!\r\n";
            messageQueues.back().push(welcomeMessage); // Add the welcome message to the new client's queue
            fds.back().events |= POLLOUT; // Ensure the POLLOUT event is set to send the message

            std::cout << "Client added. fds size: " << fds.size() << ", messageQueues size: " << messageQueues.size() << std::endl;
        } else {
            std::cerr << "Failed to accept client connection." << std::endl;
        }
    }
}

void IRCServer::handleClientEvents() {
    for (size_t i = 1; i < fds.size(); ++i) {
        if (i-1 >= messageQueues.size()) {
            std::cerr << "Index out of range for messageQueues" << std::endl;
            continue;
        }

        if (fds[i].revents & POLLIN) {
            handleClientData(fds[i].fd);
        }

        if ((fds[i].revents & POLLOUT) && !messageQueues[i-1].empty()) {
            handleClientWrite(fds[i].fd);
        }

        if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            close(fds[i].fd);
            fds.erase(fds.begin() + i);
            messageQueues.erase(messageQueues.begin() + i - 1);
            --i;
        }
    }
}

void IRCServer::handleClientData(int clientSocket) {
    char buffer[1024] = {0};
    ssize_t bytesRead = read(clientSocket, buffer, sizeof(buffer) - 1);

    if (bytesRead > 0) {
        buffer[bytesRead] = '\0';  // Ensure null-termination of the string
        std::cout << "Received data from client: " << buffer << std::endl;
        std::string message(buffer);
        if (message.rfind("NICK ", 0) == 0) {
            std::string nickname = message.substr(5);
            handleNickCommand(clientSocket, nickname);
        }

    } else if (bytesRead == 0) {
        std::cout << "Client disconnected." << std::endl;
        size_t index = findClientSocketIndex(clientSocket);
        if (index != std::string::npos) {
            close(clientSocket);
            fds.erase(fds.begin() + index + 1);
            messageQueues.erase(messageQueues.begin() + index);
        }
    } else {
        perror("Error reading from client socket");
        close(clientSocket);
    }
}

void IRCServer::handleNickCommand(int clientSocket, const std::string &nickname) {
    // Check if the nickname is already in use
    std::map<int, std::string>::iterator it;
    for (it = clientNicknames.begin(); it != clientNicknames.end(); ++it) {
        if (it->second == nickname) {
            // Nickname in use, send an error message back to the client
            std::string errMsg = ":Server 433 * " + nickname + " :Nickname is already in use\r\n";
            size_t clientIndex = findClientSocketIndex(clientSocket);
            if (clientIndex != std::string::npos) {
                messageQueues[clientIndex].push(errMsg);
                fds[clientIndex + 1].events |= POLLOUT; // Set POLLOUT to send the error message
            }
            return;
        }
    }

    // If the nickname is not in use, update the client's nickname
    clientNicknames[clientSocket] = nickname;
    std::string nickChangeMsg = ":Server NOTICE AUTH : NICK changed to " + nickname + "\r\n";

    // Broadcast the nickname change to all clients
    for (size_t i = 1; i < fds.size(); ++i) {
        if (fds[i].fd != serverSocket.getFD()) { // Ensure it's a client socket
            messageQueues[i-1].push(nickChangeMsg);
            fds[i].events |= POLLOUT; // Set POLLOUT to trigger sending the message
        }
    }
    std::cout << "Nickname change for client " << clientSocket << " to " << nickname << std::endl;
}



void IRCServer::handleClientWrite(int clientSocket) {
    size_t index = findClientSocketIndex(clientSocket);
    std::cout << "Attempting to write to client at index: " << index << std::endl;

    if (index != std::string::npos && !messageQueues[index].empty()) {
        const std::string& message = messageQueues[index].front();
        ssize_t bytesWritten = write(clientSocket, message.c_str(), message.length());
        std::cout << "Trying to send message: " << message << std::endl;

        if (bytesWritten > 0) {
            std::cout << "Sent message to client." << std::endl;
            messageQueues[index].pop();
        } else {
            perror("Error writing to client socket");
            close(clientSocket);
            fds.erase(fds.begin() + index + 1);
            messageQueues.erase(messageQueues.begin() + index);
            return;
        }
    }

    if (messageQueues[index].empty()) {
        fds[index + 1].events = POLLIN;
    }
}

size_t IRCServer::findClientSocketIndex(int clientSocket) {
    for (size_t i = 1; i < fds.size(); ++i) {
        if (fds[i].fd == clientSocket) {
            return i - 1;
        }
    }
    return std::string::npos;
}

#endif