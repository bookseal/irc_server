#include "ClientHandler.hpp"
#include "IRCServer.hpp"

ClientHandler::ClientHandler(int socket, IRCServer *server) : clientSocket(socket), server(server), active(true) {}

ClientHandler::~ClientHandler() {
    close(clientSocket);
}
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
    std::string newNickname = parameters.substr(0, parameters.size() - 2); // Trim the \r\n
    if (server->isNicknameAvailable(newNickname)) {
        if (!nickname.empty()) {
            server->unregisterNickname(nickname);
        }
        server->registerNickname(newNickname, this);
        nickname = newNickname;
        sendMessage(":Server NOTICE " + nickname + " :Nickname set to " + nickname + "\r\n");
    } else {
        sendMessage(":Server NOTICE " + nickname + " :Nickname is already in use.\r\n");
    }
}

void ClientHandler::handleUserCommand(const std::string& parameters) {
    size_t spacePos = parameters.find(' ');
    std::string newUsername = parameters.substr(0, spacePos);
    if (server->isUsernameAvailable(newUsername)) {
        if (!username.empty()) {
            server->unregisterNickname(username);
        }
        server->registerNickname(newUsername, this);
        username = newUsername;
        sendMessage(":Server NOTICE " + username + " :Username set to " + username + "\r\n");
    } else {
        sendMessage(":Server NOTICE " + username + " :Username is already in use.\r\n");
    }
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

