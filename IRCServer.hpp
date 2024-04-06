#ifndef IRCSERVER_HPP
#define IRCSERVER_HPP
#include "ServerSocket.hpp"
#include <iostream>
#include <iomanip>
#include <unistd.h> // For close function
#include <cstring>  // For memset
#include <sstream>  // For istringstream
#include <pthread.h>

class IRCServer {
public:
    IRCServer(int port) : serverSocket(port) {};
    void run();
    static void *handleClient(void *socket);

    void printHexRepresentation(const char* buffer, int length);
    void echoMessages(int socket);


private:

    ServerSocket serverSocket;
};

struct ClientData {
    int socket;
    IRCServer* serverInstance;

    ClientData(int socket, IRCServer* serverInstance)
            : socket(socket), serverInstance(serverInstance) {}
};

void IRCServer::run() {
    if (serverSocket.initialize() != 0) {
        std::cerr << "Failed to initialize server socket." << std::endl;
        return;
    }

    std::cout << "Server initialized. Waiting for client connections..." << std::endl;

    while (true) {
        int clientSocket = serverSocket.acceptClient();
        if (clientSocket < 0) {
            std::cerr << "Failed to accept client connection." << std::endl;
            continue; // Optionally, add a way to break out of this loop
        }

        ClientData *data = new ClientData(clientSocket, this);
        pthread_t clientThread;
        if (pthread_create(&clientThread, NULL, handleClient, data) != 0) {
            std::cerr << "Failed to create client thread." << std::endl;
            delete data;
            continue;
        }

        pthread_detach(clientThread);
    }
}

void* IRCServer::handleClient(void *arg) {
    ClientData *data = static_cast<ClientData*>(arg);
    int clientSocket = data->socket;
    IRCServer* serverInstance = data->serverInstance;

    std::cout << "Client connected\n";
    try {
        std::cout << "Registration complete\n";
        serverInstance->echoMessages(clientSocket);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    close(clientSocket);
    delete data; // Clean up the data allocation
    return NULL;
}

void IRCServer::printHexRepresentation(const char *buffer, int length) {
    for (int i = 0; i < length; ++i) {
        if (isprint(static_cast<unsigned char>(buffer[i]))) {
            std::cout << buffer[i];
        } else {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(static_cast<unsigned char>(buffer[i]))
                      << std::dec << " ";
        }
    }
}

void IRCServer::echoMessages(int socket) {
    char buffer[1024] = {0};
    std::cout << "Welcome message sent\n";

    while (true) {
        int read_val = read(socket, buffer, sizeof(buffer) - 1);
        if (read_val > 0) {
            buffer[read_val] = '\0';
            std::cout << "Received: [";
            printHexRepresentation(buffer, read_val);
            std::cout << "]" << std::endl;
            send(socket, buffer, read_val, 0);
        } else if (read_val == 0) {
            std::cout << "Client disconnected." << std::endl;
            break;
        } else {
            perror("read failed");
            break;
        }
    }
}

#endif