#include "ClientHandler.hpp"
#include "IRCServer.hpp"
#include "Channel.hpp"

ClientHandler::ClientHandler(int socket, IRCServer *server) : clientSocket(socket), server(server), active(true) {

}

ClientHandler::~ClientHandler() {
    server->unregisterNickname(nickname);
    server->unregisterUsername(username);
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

void ClientHandler::processCommand(const std::string& fullCommand) {
    // Trim trailing newline and carriage return characters
    std::string trimmedCommand = fullCommand;
    trimmedCommand.erase(std::remove(trimmedCommand.begin(), trimmedCommand.end(), '\r'), trimmedCommand.end());
    trimmedCommand.erase(std::remove(trimmedCommand.begin(), trimmedCommand.end(), '\n'), trimmedCommand.end());

    // Find the first space to separate the command from parameters
    size_t spacePos = trimmedCommand.find(' ');
    std::string command = (spacePos != std::string::npos) ? trimmedCommand.substr(0, spacePos) : trimmedCommand;
    std::string parameters = (spacePos != std::string::npos) ? trimmedCommand.substr(spacePos + 1) : "";

    std::cout << "Command: " << command << ", Parameters: " << parameters << std::endl;
    // Process the command with the clean parameters
    parseCommand(command, parameters);
}

void ClientHandler::parseCommand(const std::string& command, const std::string& parameters) {
    if (command == "NICK") {
        handleNickCommand(parameters);
    } else if (command == "USER") {
        handleUserCommand(parameters);
    } else if (command == "JOIN") {
        handleJoinCommand(parameters);
    } else if (command == "PART") {
        handleLeaveCommand(parameters);
    } else if (command == "PRIVMSG") {
        handleChannelMessage(currentChannel, parameters);
    } else {
        defaultMessageHandling(parameters);
    }
}

void ClientHandler::defaultMessageHandling(const std::string& message) {
    if (!currentChannel.empty()) {
        // Assumes the client has a current channel stored
        handleChannelMessage(currentChannel, message);
    } else {
        sendMessage(":Server ERROR :No channel selected or unrecognized command.\r\n");
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

void ClientHandler::handleNickCommand(const std::string& parameters) {
    if (server->isNicknameAvailable(parameters)) {
        if (!nickname.empty()) {
            server->unregisterNickname(parameters);
        }
        server->registerNickname(parameters, this);
        nickname = parameters;
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
    Channel* channel = server->findChannel(parameters);
    if (channel == nullptr) {
        server->createChannel(parameters);
        channel = server->findChannel(parameters);
    }
    if (channel) {
        channel->addClient(this);
        currentChannel = parameters;  // Update current channel
        sendMessage(":Server NOTICE " + nickname + " :Joined channel " + parameters + "\r\n");
    }
}

void ClientHandler::handleLeaveCommand(const std::string& parameters) {
    Channel* channel = server->findChannel(parameters);
    if (channel) {
        channel->removeClient(this);
        if (parameters == currentChannel) {
            currentChannel.clear();  // Clear current channel if it's the one being left
        }
        sendMessage(":Server NOTICE " + nickname + " :Left channel " + parameters + "\r\n");
    } else {
        sendMessage(":Server ERROR :No such channel " + parameters + "\r\n");
    }
}


void ClientHandler::handlePrivMsgCommand(const std::string& parameters) {
    size_t spacePos = parameters.find(' ');
    if (spacePos == std::string::npos) {
        sendMessage(":Server ERROR :Invalid PRIVMSG format.\r\n");
        return;
    }

    std::string target = parameters.substr(0, spacePos);
    std::string message = parameters.substr(spacePos + 1);

    if (!target.empty() && target[0] == '#') {
        handleChannelMessage(target, message);
    } else {
        // Handling private messages to a user
        std::string fullMessage = ": " + nickname + "!user@host PRIVMSG " + target + " :" + message + "\r\n";
        // Assume server can find and forward to the user
        server->sendMessageToUser(target, fullMessage);
    }
}


void ClientHandler::handleChannelMessage(const std::string& channelName, const std::string& message) {
    Channel* channel = server->findChannel(channelName);
    if (channel && channel->isClientMember(this)) {
        channel->broadcastMessage(": " + nickname + "!" + username + " PRIVMSG " + channelName + " :" + message + "\r\n", this);
    } else {
        sendMessage(":Server ERROR :You are not in channel " + channelName + "\r\n");
    }
}

