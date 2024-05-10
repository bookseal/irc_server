#include "IRCServer.hpp"
#include "ClientHandler.hpp"
#include "Channel.hpp"

ClientHandler::ClientHandler(int socket, IRCServer *server) : clientSocket(socket), server(server), active(true) {

}

ClientHandler::~ClientHandler() {
    server->unregisterNickname(nickname);
    close(clientSocket);
}

void ClientHandler::processInput() {
    const size_t bufferSize = 1024;
    char buffer[bufferSize];
    static std::string accumulatedInput;

    ssize_t bytesRead = read(clientSocket, buffer, sizeof(buffer) - 1);

    if (bytesRead > 0) {
        buffer[bytesRead] = '\0';
        accumulatedInput += buffer;
        size_t pos = 0;
        while ((pos = accumulatedInput.find("\r\n")) != std::string::npos) {
            std::string command = accumulatedInput.substr(0, pos);
            std::cout << "Received: " << command << "$" << std::endl;
            processCommand(command);
            accumulatedInput.erase(0, pos + 2);
        }
    } else if (bytesRead == 0) {
        std::cout << "Client disconnected." << std::endl;
        handleDisconnect();
    } else {
        std::cerr << "Read error." << std::endl;
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
        handlePrivMsgCommand(parameters);
    } else if (command == "MODE") {
        handleModeCommand(parameters);
    } else if (command == "CAP" || command == "WHOIS" || command == "PING" || command == "PASS") {
        ;
    } else {
        defaultMessageHandling(command + " " + parameters);
    }
}

void ClientHandler::handleModeCommand(const std::string& parameters) {
    std::string mode = parameters.substr(0, parameters.find(' '));
    if (mode == nickname) {
        sendMessage(":" + nickname + "!" + username + "@" + hostname + " MODE " + nickname + " :+i");
    } else if (mode == currentChannel) {
        sendMessage(":irc.local 324 " + nickname + " " + currentChannel + " :+nt");
        sendMessage(":irc.local 329 " + nickname + " " + currentChannel + " :");
    } else {
        sendMessage(":Server ERROR :You can only set modes for yourself.\r\n");
    }
}

void ClientHandler::handlePrivMsgCommand(const std::string& parameters) {
    size_t spacePos = std::string::npos;
    for (size_t i = 0; i < parameters.size(); ++i) {
        if (parameters[i] == ' ') {
            spacePos = i;
            break;
        }
    }
    if (spacePos == std::string::npos) {
        sendMessage(":Server ERROR :Invalid PRIVMSG format.\r\n");
        return;
    }
    std::string target = parameters.substr(0, spacePos);
    std::string message = parameters.substr(spacePos + 2);
    if (!target.empty() && target[0] == '#') {
        handleChannelMessage(target, message);
    } else {
        server->sendMessageToUser(nickname, target, message);
    }
}

void ClientHandler::defaultMessageHandling(const std::string& message) {
    if (nickname.empty()) {
        sendMessage(":Server ERROR :Please choose a nickname with the NICK command.\r\n");
        return;
    }
    if (!currentChannel.empty()) {
        handleChannelMessage(currentChannel, message);
    } else {
        sendMessage(":Server ERROR :No channel selected or unrecognized command.\r\n");
    }
}

void ClientHandler::handleChannelMessage(const std::string& channelName, const std::string& message) {
    std::cout << "Channel message: " << message << std::endl;
    Channel* channel = server->findChannel(channelName);
    if (channel && channel->isClientMember(this)) {
        channel->broadcastMessage(":" + nickname + "!" + username + "@" + hostname + " PRIVMSG " + channelName + " :" + message, this);
    } else {
        sendMessage(":Server ERROR :You are not in channel " + channelName + "\r\n");
    }
}

void ClientHandler::handleNickCommand(const std::string& parameters) {
    std::string newNickname = parameters;
    newNickname.erase(0, newNickname.find_first_not_of(" \t\n"));
    newNickname.erase(newNickname.find_last_not_of(" \t\n") + 1);
    if (newNickname.empty()) {
        sendMessage(":Server 431 * :No nickname given");
        return;
    }
    if (server->isNicknameAvailable(newNickname)) {
        if (!nickname.empty() && nickname != newNickname) {
            server->unregisterNickname(nickname);
        }
        server->registerNickname(newNickname, this);
        nickname = newNickname;
        sendMessage(":" + nickname + "!" + username + "@" + hostname + " NICK :" + nickname);
        sendMessage(":Server NOTICE " + nickname + " :Nickname set to " + newNickname);
    } else {
        sendMessage(":Server 433 " + nickname + " " + newNickname + " :Nickname is already in use");
    }
}

void ClientHandler::handleUserCommand(const std::string& parameters) {
    std::vector<std::string> userParams;
    std::string userParam;
    std::istringstream userParamStream(parameters);
    while (std::getline(userParamStream, userParam, ' ')) {
        userParams.push_back(userParam);
    }
    if (userParams.size() < 4) {
        sendMessage(":Server ERROR :Invalid USER command format.\r\n");
        return;
    }
    username = userParams[0];
    hostname = userParams[2];
    sendMessage(":Server 302 " + nickname + " :");
    sendMessage(":Server 001 " + nickname + " :Welcome to the server, " + nickname + "!");
}

void ClientHandler::handleJoinCommand(const std::string& parameters) {
    if (channels.find(parameters) != channels.end()) {
        sendMessage(":Server ERROR :You are already in channel " + parameters + "\r\n");
        return;
    }
    if (parameters == ":" || parameters.empty()) {
        sendMessage(":irc.local 451 * JOIN :You have not specified a channel name.");
        return;
    }
    Channel* channel = server->findChannel(parameters);
    if (channel == nullptr) {
        server->createChannel(parameters);
        channel = server->findChannel(parameters);
    }
    if (channel) {
        channel->addClient(this);
        channels.insert(parameters);
        sendMessage(":" + nickname + "!" + username + "@" + hostname + " JOIN :" + parameters);
        // Send other necessary channel messages
    }
}

void ClientHandler::handleLeaveCommand(const std::string& parameters) {
    if (channels.find(parameters) == channels.end()) {
        sendMessage(":Server ERROR :You are not in channel " + parameters + "\r\n");
        return;
    }
    Channel* channel = server->findChannel(parameters);
    if (channel) {
        channel->removeClient(this);
        channels.erase(parameters);
        sendMessage(":" + nickname + "!" + username + "@" + hostname + " PART :" + parameters);
    }
}

void ClientHandler::handleDisconnect() {
    std::set<std::string>::iterator it;
    for (it = channels.begin(); it != channels.end(); ++it) {
        const std::string& channelName = *it;
        Channel* channel = server->findChannel(channelName);
        if (channel) {
            channel->removeClient(this);
        }
    }
    deactivate();
}

void ClientHandler::sendMessage(const std::string& message) {
    std::cout << "Sending  : " << message << std::endl;
    std::string fullMessage = message + "\r\n";
    if (send(clientSocket, fullMessage.c_str(), fullMessage.size(), 0) == -1) {
        std::cerr << "Failed to send message." << std::endl;
    }
}
