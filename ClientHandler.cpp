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
    } else if (command == "PART") {
        handleLeaveCommand(parameters);
    } else if (command == "PRIVMSG") {
        // Determine if the message is intended for a channel or a user
        size_t spacePos = parameters.find(' ');
        if (spacePos == std::string::npos) {
            sendMessage(":Server ERROR :Invalid PRIVMSG format.\r\n");
            return;
        }

        std::string target = parameters.substr(0, spacePos);
        if (!target.empty() && target[0] == '#') {
            // It's a channel message
            std::string message = parameters.substr(spacePos + 1);
            handleChannelMessage(target, message);
        } else {
            // It's a private message to another user
            handlePrivMsgCommand(parameters);
        }
    } else {
        // If the command is not recognized
        sendMessage(":Server ERROR :Unknown command.\r\n");
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
    std::string channelName = parameters.substr(0, parameters.size() - 2);
    Channel* channel = server->findChannel(channelName);
    if (!channel) {
        server->createChannel(channelName);
        channel = server->findChannel(channelName);
        sendMessage(":Server NOTICE " + username + " :Channel " + channelName + " created and joined.\r\n");
    } else {
        sendMessage(":Server NOTICE " + username + "  :Joined existing channel " + channelName + ".\r\n");
    }
    channel->addClient(this);
}

void ClientHandler::handleLeaveCommand(const std::string& parameters) {
    std::string channelName = parameters.substr(0, parameters.size() - 2);
    Channel* channel = server->findChannel(channelName);
    if (channel) {
        channel->removeClient(this);
        sendMessage(":Server NOTICE " + username + " :You have left the channel " + channelName + ".\r\n");
        if (channel->isEmpty()) {
            server->deleteChannel(channelName);
            sendMessage(":Server NOTICE " + username + " :Channel " + channelName + " deleted.\r\n");
        }
    } else {
        sendMessage(":Server NOTICE " + username + " :Channel " + channelName + " does not exist.\r\n");
    }
}


void ClientHandler::handlePrivMsgCommand(const std::string& parameters) {
    size_t spacePos = parameters.find(' ');
    if (spacePos == std::string::npos) {
        sendMessage(":Server ERROR :Invalid message format.\r\n");
        return;
    }

    std::string recipient = parameters.substr(0, spacePos);
    std::string message = parameters.substr(spacePos + 1);

    // Check if the recipient exists
    ClientHandler* recipientHandler = server->findClientHandlerByNickname(recipient);
    if (recipientHandler) {
        recipientHandler->sendMessage(": " + nickname + "!user@host PRIVMSG " + recipient + " :" + message + "\r\n");
    } else {
        sendMessage(":Server NOTICE :User " + recipient + " does not exist.\r\n");
    }
}

void ClientHandler::handleChannelMessage(const std::string& channelName, const std::string& message) {
    if (channelName.empty() || message.empty()) {
        sendMessage(":Server ERROR :Invalid channel or message.\r\n");
        return;
    }

    Channel* channel = server->findChannel(channelName);
    if (channel) {
        if (channel->isClientMember(this)) {
            std::string fullMessage = ": " + nickname + "!user@host PRIVMSG " + channelName + " :" + message + "\r\n";
            channel->broadcastMessage(fullMessage, this);
        } else {
            sendMessage(":Server NOTICE :You are not a member of the channel " + channelName + ".\r\n");
        }
    } else {
        sendMessage(":Server NOTICE :Channel " + channelName + " does not exist.\r\n");
    }
}
