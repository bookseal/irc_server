#ifndef IRCSERVER_HPP
#define IRCSERVER_HPP

#include <iostream>
#include <cstring>
#include <unistd.h>
#include <vector>
#include <poll.h>
#include <map>
#include <sys/socket.h>
#include <netinet/in.h>
#include "ClientHandler.hpp"

class IRCServer {
public:
    IRCServer(int port) : port(port), serverSocket(-1) {
        // Initialize server upon construction if needed
    }
    ~IRCServer() {
        // Proper cleanup on destruction
        close(serverSocket);
        for (std::map<int, ClientHandler*>::iterator it = clientHandlers.begin(); it != clientHandlers.end(); ++it) {
            delete it->second;
        }
    }

    bool initializeServerSocket();
    void cleanUpInactiveHandlers();
    void run();

private:
    int port;
    int serverSocket;
    std::vector<struct pollfd> fds;
    std::map<int, ClientHandler*> clientHandlers; // Map from socket descriptors to client handlers

    void acceptNewClient();
};

bool IRCServer::initializeServerSocket() {
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Failed to create socket." << std::endl;
        return false;
    }

    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Error setting socket options." << std::endl;
        return false;
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Failed to bind to port " << port << "." << std::endl;
        return false;
    }

    if (listen(serverSocket, 10) < 0) {
        std::cerr << "Failed to listen on socket." << std::endl;
        return false;
    }

    struct pollfd listenFD;
    listenFD.fd = serverSocket;
    listenFD.events = POLLIN;
    fds.push_back(listenFD);

    return true;
}

void IRCServer::run() {
    if (!initializeServerSocket()) {
        std::cerr << "Server initialization failed." << std::endl;
        return;
    }

    std::cout << "Server running on port " << port << std::endl;

    while (true) {
        int pollCount = poll(&fds[0], fds.size(), -1);
        if (pollCount < 0) {
            std::cerr << "Poll error." << std::endl;
            break;
        }

        for (size_t i = 0; i < fds.size(); i++) {
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == serverSocket) {
                    acceptNewClient();
                } else {
                    ClientHandler* handler = clientHandlers[fds[i].fd];
                    if (handler) {
                        handler->processInput();
                    }
                }
            }
        }

        cleanUpInactiveHandlers();  // Clean up any handlers that are no longer active
    }
}

void IRCServer::acceptNewClient() {
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    int clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientAddrLen);
    if (clientSocket < 0) {
        std::cerr << "Error accepting new connection." << std::endl;
        return;
    }

    ClientHandler* newHandler = new ClientHandler(clientSocket);
    clientHandlers[clientSocket] = newHandler;

    struct pollfd clientFD;
    clientFD.fd = clientSocket;
    clientFD.events = POLLIN;
    fds.push_back(clientFD);

    std::cout << "New client connected: " << clientSocket << std::endl;
}

void IRCServer::cleanUpInactiveHandlers() {
    std::map<int, ClientHandler*>::iterator it = clientHandlers.begin();
    while (it != clientHandlers.end()) {
        if (!it->second->isActive()) {
            std::cout << "Cleaning up client handler for socket: " << it->first << std::endl;
            close(it->first); // Close the socket
            delete it->second; // Delete the handler
            it = clientHandlers.erase(it); // Remove from the map and advance the iterator
        } else {
            ++it;
        }
    }
}

#endif

















//#include "ServerSocket.hpp"
//#include <iostream>
//#include <iomanip>
//#include <unistd.h>
//#include <cstring>
//#include <sstream>
//#include <pthread.h>
//#include <vector>
//#include <poll.h>
//#include <queue>
//#include <map>
//
//struct UserDetails {
//    std::string username;
//    std::string realname;
//};
//
//class IRCServer {
//public:
//    IRCServer(int port) : serverSocket(port) {};
//    void run();
//    void initializeServerSocketAndPollFD();
//    void waitForConnectionsAndHandleData();
//    void acceptNewClients();
//    void handleClientEvents();
//    void handleClientData(int clientSocket);
//    void handleClientWrite(int clientSocket);
//    size_t findClientSocketIndex(int clientSocket);
//    void handleNickCommand(int clientSocket, const std::string& nickname);
//    void handleUserCommand(int clientSocket, const std::string &message);
//
//private:
//    ServerSocket serverSocket;
//    std::vector<struct pollfd> fds;
//    std::vector<std::queue<std::string> > messageQueues;
//    std::map<int, std::string> clientNicknames;
//    std::map<int, UserDetails> clientUsers;
//};
//
//void IRCServer::run() {
//    initializeServerSocketAndPollFD();
//    waitForConnectionsAndHandleData();
//}
//
//void IRCServer::initializeServerSocketAndPollFD() {
//    if (serverSocket.initialize() != 0) {
//        std::cerr << "Failed to initialize server socket." << std::endl;
//        return;
//    }
//
//    std::cout << "Server initialized. Waiting for client connections..." << std::endl;
//
//    struct pollfd server_fd;
//    server_fd.fd = serverSocket.getFD();
//    server_fd.events = POLLIN;
//    server_fd.revents = 0;
//    fds.push_back(server_fd);
//}
//
//void IRCServer::waitForConnectionsAndHandleData() {
//    while (true) {
//        if (poll(fds.data(), fds.size(), -1) < 0) {
//            perror("poll");
//            return;
//        }
//        acceptNewClients();
//        handleClientEvents();
//    }
//}
//
//void IRCServer::acceptNewClients() {
//    if (fds[0].revents & POLLIN) {
//        int clientSocket = serverSocket.acceptClient();
//        if (clientSocket >= 0) {
//            std::cout << std::endl << "===================\nNew client connected, socket fd: " << clientSocket << std::endl;
//
//            // Prepare the client's pollfd structure
//            struct pollfd client_fd = {clientSocket, POLLIN | POLLOUT, 0};
//            fds.push_back(client_fd);
//
//            // Create a message queue for the new client
//            messageQueues.push_back(std::queue<std::string>());
//
//            // Send a welcome message to the new client
//            std::string welcomeMessage = ":Server NOTICE AUTH :Welcome to the IRC Server!\r\n";
//            messageQueues.back().push(welcomeMessage); // Add the welcome message to the new client's queue
//            fds.back().events |= POLLOUT; // Ensure the POLLOUT event is set to send the message
//
//            std::cout << "Client added. fds size: " << fds.size() << ", messageQueues size: " << messageQueues.size() << std::endl;
//        } else {
//            std::cerr << "Failed to accept client connection." << std::endl;
//        }
//    }
//}
//
//void IRCServer::handleClientEvents() {
//    size_t i = 1; // Start from 1 to skip the server's own socket
//    while (i < fds.size()) {
//        if (fds[i].revents & POLLIN) {
//            handleClientData(fds[i].fd);
//        }
//
//        if (fds[i].revents & POLLOUT && !messageQueues[i-1].empty()) {
//            handleClientWrite(fds[i].fd);
//        }
//
//        if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
//            if (fds[i].fd != -1) {
//                close(fds[i].fd); // Close the socket safely
//                clientNicknames.erase(fds[i].fd);
//                fds.erase(fds.begin() + i);
//                messageQueues.erase(messageQueues.begin() + i - 1);
//                fds[i].fd = -1;
//                std::cout << "Client socket closed. fds size: " << fds.size() << ", messageQueues size: " << messageQueues.size() << std::endl;
//                continue; // Adjust the index since the vectors have been modified
//            }
//        }
//        i++; // Increment only if no socket was closed
//    }
//}
//
//void IRCServer::handleNickCommand(int clientSocket, const std::string &nickname) {
//    std::cout << "Received NICK command from client " << clientSocket << " with nickname " << nickname << std::endl;
//    for (std::map<int, std::string>::iterator it = clientNicknames.begin(); it != clientNicknames.end(); ++it) {
//        if (it->second == nickname) {
//            std::string errMsg = ":Server NOTICE AUTH :Nickname is already in use\r\n";
//            size_t clientIndex = findClientSocketIndex(clientSocket);
//            if (clientIndex != std::string::npos) {
//                messageQueues[clientIndex].push(errMsg);
//                fds[clientIndex + 1].events |= POLLOUT;
//            }
//            return;
//        }
//    }
//    clientNicknames[clientSocket] = nickname;
//    std::cout << "Nickname change for client " << clientSocket << " to " << nickname << std::endl;
//    std::string nickChangeMsg = ":Server NOTICE AUTH : your nickname has been changed to " + nickname + "\r\n";
//    size_t clientIndex = findClientSocketIndex(clientSocket);
//    if (clientIndex != std::string::npos) {
//        messageQueues[clientIndex].push(nickChangeMsg);
//        fds[clientIndex + 1].events |= POLLOUT;
//    }
//}
//
//void handleUserCommand(int clientSocket, const std::string &message) {
//    std::size_t firstSpace = message.find(' ');
//    std::size_t secondSpace = message.find(' ', firstSpace + 1);
//    std::size_t thirdSpace = message.find(' ', secondSpace + 1);
//    std::size_t colon = message.find(':', thirdSpace);  // Find the colon before the realname
//
//    if (firstSpace == std::string::npos || secondSpace == std::string::npos || thirdSpace == std::string::npos || colon == std::string::npos) {
//        std::cerr << "Error parsing USER command" << std::endl;
//        return;
//    }
//
//    std::string username = message.substr(firstSpace + 1, secondSpace - firstSpace - 1);
//    std::string realname = message.substr(colon + 1);
//
//    std::cout << "Received USER command from client " << clientSocket << " with username " << username << " and realname " << realname << std::endl;
//
//    // Check if the user has already been registered
//    if (clientUsers.find(clientSocket) != clientUsers.end()) {
//        std::string errMsg = ":Server NOTICE AUTH :You are already registered\r\n";
//        messageQueues[findClientSocketIndex(clientSocket)].push(errMsg);
//        fds[findClientSocketIndex(clientSocket) + 1].events |= POLLOUT;
//        return;
//    }
//
//    // Register new user
//    UserDetails newUser;
//    newUser.username = username;
//    newUser.realname = realname;
//    clientUsers[clientSocket] = newUser;
//
//    std::cout << "User registered for client " << clientSocket << " with username " << username << " and realname " << realname << std::endl;
//    std::string successMsg = ":Server NOTICE AUTH :Your user details have been set\r\n";
//    messageQueues[findClientSocketIndex(clientSocket)].push(successMsg);
//    fds[findClientSocketIndex(clientSocket) + 1].events |= POLLOUT;
//}
//
//void IRCServer::handleClientData(int clientSocket) {
//    char buffer[1024] = {0};
//    ssize_t bytesRead = read(clientSocket, buffer, sizeof(buffer) - 1);
//
//    if (bytesRead > 0) {
//        buffer[bytesRead] = '\0';  // Ensure null-termination of the string
//        std::cout << "Received data from client: " << buffer << std::endl;
//        std::string message(buffer);
//        if (message.rfind("NICK ", 0) == 0) {
//            std::string nickname = message.substr(5);
//            handleNickCommand(clientSocket, nickname);
//        }
//        else if (message.rfind("USER", 0) == 0) {
//            handleUserCommand(clientSocket, message);
//        }
//    } else if (bytesRead == 0) {
//        std::cout << "Client disconnected. 😢" << std::endl;
//    } else {
//        perror("Error reading from client socket");
//        close(clientSocket);
//    }
//}
//
//void IRCServer::handleClientWrite(int clientSocket) {
//    size_t index = findClientSocketIndex(clientSocket);
//    if (index != std::string::npos && !messageQueues[index].empty()) {
//        const std::string& message = messageQueues[index].front();
//        ssize_t bytesWritten = write(clientSocket, message.c_str(), message.length());
//        if (bytesWritten > 0) {
//            messageQueues[index].pop();
//        } else {
//            perror("Error writing to client socket");
//            close(clientSocket);
//            fds.erase(fds.begin() + index + 1);
//            messageQueues.erase(messageQueues.begin() + index);
//            clientNicknames.erase(clientSocket);
//            return;
//        }
//    }
//    if (messageQueues[index].empty()) {
//        fds[index + 1].events = POLLIN;
//    }
//}
//
//size_t IRCServer::findClientSocketIndex(int clientSocket) {
//    for (size_t i = 1; i < fds.size(); ++i) {
//        if (fds[i].fd == clientSocket) {
//            return i - 1;
//        }
//    }
//    return std::string::npos;
//}
//
//#endif