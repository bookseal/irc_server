#include "ClientSession.hpp"
#include <iostream>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <iomanip>

ClientSession::ClientSession(int clientSocket) : socket(clientSocket) {
}

ClientSession::~ClientSession() {
    close(socket);
}

void ClientSession::handleIRCRegistration() {
    char buffer[1024] = {0};
    std::cout << "Waiting for client registration" << std::endl;

    while (nickname.empty() || username.empty()) {
        int read_val = read(socket, buffer, sizeof(buffer) - 1);
        if (read_val > 0) {
            buffer[read_val] = '\0'; // Null-terminate the string
            std::istringstream iss(buffer);
            std::string command;
            iss >> command;

            if (command == "NICK") {
                iss >> nickname;
                nickname.erase(0, 1); // Assuming the first character is ':' which should be removed
            } else if (command == "USER") {
                iss >> username; // In a real-world scenario, you might need to parse further
            }

            std::memset(buffer, 0, sizeof(buffer)); // Clear the buffer for the next message
        } else if (read_val <= 0) {
            throw std::runtime_error("Client disconnected or read failed during registration.");
        }
    }

    std::string welcome = ":server 001 " + nickname + " :Welcome to the IRC network, " + nickname + "!\r\n";
    send(socket, welcome.c_str(), welcome.length(), 0);
    std::cout << "Registration complete for " << nickname << std::endl;
}

void ClientSession::echoMessages() {
    char buffer[1024] = {0};
    while (true) {
        int read_val = read(socket, buffer, sizeof(buffer) - 1);
        if (read_val > 0) {
            buffer[read_val] = '\0'; // Null-terminate the string
            std::cout << "Echoing back: ";
            printHexRepresentation(buffer, read_val);
            send(socket, buffer, strlen(buffer), 0);
        } else if (read_val == 0) {
            std::cout << "Client disconnected." << std::endl;
            break;
        } else {
            perror("read failed");
            break;
        }
    }
}

void ClientSession::printHexRepresentation(const char* buffer, int length) {
    for (int i = 0; i < length; ++i) {
        if (isprint(static_cast<unsigned char>(buffer[i]))) {
            std::cout << buffer[i];
        } else {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(static_cast<unsigned char>(buffer[i]))
                      << std::dec << " ";
        }
    }
    std::cout << std::endl;
}
