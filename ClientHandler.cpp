#include "IRCServer.hpp"
#include "ClientHandler.hpp"
#include "Channel.hpp"

ClientHandler::ClientHandler(int socket, IRCServer *server) : clientSocket(socket), server(server), active(true) {

}

ClientHandler::~ClientHandler() {
    server->unregisterNickname(nickname);
    server->unregisterUsername(username);
    close(clientSocket);
}

void ClientHandler::processInput() {
    const size_t bufferSize = 1024;
    char buffer[bufferSize];
    static std::string accumulatedInput;

    ssize_t bytesRead = read(clientSocket, buffer, sizeof(buffer) - 1);

    if (bytesRead > 0) {
        buffer[bytesRead] = '\0'; // Null-terminate the string

        // Append to the accumulated input
        accumulatedInput += buffer;

        // Find the position of \r\n
        size_t pos = 0;
        while ((pos = accumulatedInput.find("\r\n")) != std::string::npos) {
            // Extract the command up to \r\n
            std::string command = accumulatedInput.substr(0, pos);
            std::cout << "Received: " << command << "$" << std::endl;

            // Process the command
            processCommand(command);

            // Erase the processed command from accumulated input
            accumulatedInput.erase(0, pos + 2); // +2 to remove the \r\n as well
        }
    } else if (bytesRead == 0) {
        // Handle disconnection
        std::cout << "Client disconnected." << std::endl;
        handleDisconnect();
    } else {
        // Handle read error
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
    } else if (command == "CAP" || command == "WHOIS" || command == "PING" || command == "PASS") {
        handleCapCommand(parameters);
    } else {
        defaultMessageHandling(command + " " + parameters);
    }
}

void ClientHandler::handleCapCommand(const std::string& parameters) {
    // CAP command is not implemented
    sendMessage(":Server ERROR :this command is not implemented.\r\n");
}

void ClientHandler::handlePrivMsgCommand(const std::string& parameters) {
    // Find the position of the first space character
    size_t spacePos = std::string::npos;
    for (size_t i = 0; i < parameters.size(); ++i) {
        if (parameters[i] == ' ') {
            spacePos = i;
            break;
        }
    }

    // Check if a space character was found
    if (spacePos == std::string::npos) {
        sendMessage(":Server ERROR :Invalid PRIVMSG format.\r\n");
        return;
    }

    // Extract the target and message from the parameters
    std::string target = parameters.substr(0, spacePos);
    std::string message = parameters.substr(spacePos + 1);
    // Check if the target is a channel or a user
    if (!target.empty() && target[0] == '#') {
        handleChannelMessage(target, message);
    } else {
        // Use the sender's nickname as the first parameter
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
        channel->broadcastMessage(": " + nickname + "!" + username + " PRIVMSG " + channelName + " :" + message + "\r\n", this);
    } else {
        sendMessage(":Server ERROR :You are not in channel " + channelName + "\r\n");
    }
}

void ClientHandler::handleNickCommand(const std::string& parameters) {
    // Trim leading and trailing whitespace from the nickname
    std::string newNickname = parameters;
    newNickname.erase(0, newNickname.find_first_not_of(" \t\n"));
    newNickname.erase(newNickname.find_last_not_of(" \t\n") + 1);

    // Check if the nickname is not empty after trimming
    if (newNickname.empty()) {
        sendMessage(":Server 431 * :No nickname given");
        return;
    }

    // Check if the new nickname is available
    if (server->isNicknameAvailable(newNickname)) {
        if (!nickname.empty() && nickname != newNickname) {
            // Unregister the old nickname if it exists and is different
            server->unregisterNickname(nickname);
        }
        // Register the new nickname
        server->registerNickname(newNickname, this);
        nickname = newNickname; // Update the current nickname
        sendMessage(":Server NICK :" + nickname);
        sendMessage(":Server NOTICE " + nickname + " :Nickname set to " + nickname);
    } else {
        sendMessage(":Server 433 * " + newNickname + " :Nickname is already in use");
    }
}

void ClientHandler::handleUserCommand(const std::string& parameters) {
    size_t spacePos = std::string::npos;
    for (size_t i = 0; i < parameters.size(); ++i) {
        if (parameters[i] == ' ') {
            spacePos = i;
            break;
        }
    }

    std::string newUsername;
    if (spacePos != std::string::npos) {
        newUsername = parameters.substr(0, spacePos);
    } else {
        newUsername = parameters; // If no space found, use the entire parameters as the username
    }

    // Directly update the username without checking its availability
    if (!username.empty() && username != newUsername) {
        // Optionally unregister the old username if it exists and is different
        server->unregisterUsername(username);
    }

    // Register the new username
    server->registerUsername(newUsername, this);
    username = newUsername;  // Update the current username
    sendMessage(":Server NOTICE " + nickname + " :Username set to " + username);
    sendMessage(":Server 001 " + username + " :Welcome to the server, " + username + "!");
}


void ClientHandler::handleJoinCommand(const std::string& parameters) {
    if (currentChannel == parameters) {
        sendMessage(":Server ERROR :You are already in channel " + parameters + "\r\n");
        return;
    }
    if (!currentChannel.empty()) {
        handleLeaveCommand(currentChannel);
    }
    if (parameters == ":" || parameters.empty()) {
        sendMessage(":Server ERROR :Invalid JOIN command.");
        return;
    }
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
    if (nickname.empty()) {
        sendMessage(":Server ERROR :Please choose a nickname with the NICK command.\r\n");
        return;
    }

    Channel* channel = server->findChannel(parameters);
    std::cout << "parameters: " << parameters << std::endl;
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

void ClientHandler::handleDisconnect() {
    deactivate();
}

void ClientHandler::sendMessage(const std::string& message) {
    std::cout << "Sending : " << message << std::endl;
    std::string fullMessage = message + "\r\n";
    if (send(clientSocket, fullMessage.c_str(), fullMessage.size(), 0) == -1) {
        std::cerr << "Failed to send message." << std::endl;
    }
}
