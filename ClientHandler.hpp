#ifndef CLIENT_HANDLER_HPP
#define CLIENT_HANDLER_HPP

#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/socket.h>

class ClientHandler {
public:
    ClientHandler(int socket) : clientSocket(socket), active(true) {}
    ~ClientHandler() { close(clientSocket); }
    void processInput();
    void sendMessage(const std::string& message);
    bool isActive() const { return active; }
    void deactivate() { active = false; }
    void handleDisconnect();

private:
    int clientSocket;
    bool active;
    std::string nickname;
    std::string username;

    void processCommand(const std::string& fullCommand);
    void parseCommand(const std::string& command, const std::string& parameters);
    void handleNickCommand(const std::string& parameters);
    void handleUserCommand(const std::string& parameters);
    void handleJoinCommand(const std::string& parameters);
    void handlePrivMsgCommand(const std::string& parameters);
};

void ClientHandler::processInput() {
    char buffer[1024];
    ssize_t bytesRead = read(clientSocket, buffer, sizeof(buffer) - 1);
    if (bytesRead > 0) {
        buffer[bytesRead] = '\0';
        std::cout << "Received: " << buffer << std::endl;
        processCommand(std::string(buffer));
    } else if (bytesRead == 0) {
        std::cout << "Client disconnected." << std::endl;
        handleDisconnect();
    }
}

void ClientHandler::handleDisconnect() {
    deactivate();
    // Additional cleanup or notification can go here
}

void ClientHandler::sendMessage(const std::string& message) {
    std::cout << "Sending: " << message << std::endl;
    if (send(clientSocket, message.c_str(), message.size(), 0) == -1) {
        std::cerr << "Failed to send message." << std::endl;
    }
}

void ClientHandler::processCommand(const std::string& fullCommand) {
    size_t spacePos = fullCommand.find(' ');
    std::string command = fullCommand.substr(0, spacePos);
    std::string parameters = (spacePos != std::string::npos) ? fullCommand.substr(spacePos + 1) : "";

    parseCommand(command, parameters);
}

void ClientHandler::parseCommand(const std::string& command, const std::string& parameters) {
    if (command == "NICK") {
        handleNickCommand(parameters);
    } else if (command == "USER") {
        handleUserCommand(parameters);
    } else if (command == "JOIN") {
        handleJoinCommand(parameters);
    } else if (command == "PRIVMSG") {
        handlePrivMsgCommand(parameters);
    }
}

void ClientHandler::handleNickCommand(const std::string& parameters) {
    nickname = parameters;
    sendMessage(":Server NOTICE " + nickname + " :Nickname set to " + nickname + "\r\n");
}

void ClientHandler::handleUserCommand(const std::string& parameters) {
    size_t spacePos = parameters.find(' ');
    username = parameters.substr(0, spacePos);
    sendMessage(":Server NOTICE " + username + " :Username set to " + username + "\r\n");
}

void ClientHandler::handleJoinCommand(const std::string& parameters) {
    sendMessage(":Server NOTICE " + nickname + " :You joined " + parameters + "\r\n");
}

void ClientHandler::handlePrivMsgCommand(const std::string& parameters) {
    size_t spacePos = parameters.find(' ');
    std::string recipient = parameters.substr(0, spacePos);
    std::string message = parameters.substr(spacePos + 1);

    sendMessage(":Server PRIVMSG " + recipient + " :" + message + "\r\n");
}

#endif // CLIENT_HANDLER_HPP
