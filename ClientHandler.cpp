#include "ClientHandler.hpp"

#include "Channel.hpp"
#include "IRCServer.hpp"

ClientHandler::ClientHandler(int socket, IRCServer* server)
    : clientSocket(socket), server(server), active(true) {}

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
  trimmedCommand.erase(
      std::remove(trimmedCommand.begin(), trimmedCommand.end(), '\r'),
      trimmedCommand.end());
  trimmedCommand.erase(
      std::remove(trimmedCommand.begin(), trimmedCommand.end(), '\n'),
      trimmedCommand.end());

  // Find the first space to separate the command from parameters
  size_t spacePos = trimmedCommand.find(' ');
  std::string command = (spacePos != std::string::npos)
                            ? trimmedCommand.substr(0, spacePos)
                            : trimmedCommand;
  std::string parameters = (spacePos != std::string::npos)
                               ? trimmedCommand.substr(spacePos + 1)
                               : "";

  parseCommand(command, parameters);
}

void ClientHandler::parseCommand(const std::string& command,
                                 const std::string& parameters) {
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
  } else if (command == "CAP" || command == "WHOIS" || command == "PING" ||
             command == "PASS") {
    ;
  } else if (command == "KICK") {
    handleKickCommand(parameters);
  } else {
    defaultMessageHandling(command + " " + parameters);
  }
}

void ClientHandler::handleModeCommand(const std::string& parameters) {
    std::string target;
    std::string mode;
    size_t spacePos = parameters.find(' ');
    if (spacePos == std::string::npos) {
        // If no space was found, assume the entire parameter is a mode to the client's nickname.
        if (parameters.empty()) {
            sendMessage(":Server ERROR :Invalid MODE command format.\r\n");
            return;
        }
        target = nickname;
        mode = parameters;
    } else {
        target = parameters.substr(0, spacePos);
        mode = parameters.substr(spacePos + 1);
    }

    if (target == nickname) {
        sendMessage(":" + nickname + "!" + username + "@" + hostname + " MODE " + nickname + " " + mode);
        return;
    }

    Channel* channel = server->findChannel(target);
    if (channel) {
        channel->setMode(mode, this);
    } else {
        sendMessage(":Server 403 " + nickname + " " + target + " :No such channel\r\n");
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
    sendMessage(
        ":Server ERROR :Please choose a nickname with the NICK command.\r\n");
    return;
  }
  if (!currentChannel.empty()) {
    handleChannelMessage(currentChannel, message);
  } else {
    sendMessage(
        ":Server ERROR :No channel selected or unrecognized command.\r\n");
  }
}

void ClientHandler::handleChannelMessage(const std::string& channelName,
                                         const std::string& message) {
  std::cout << "Channel message: " << message << std::endl;
  Channel* channel = server->findChannel(channelName);
  if (channel && channel->isClientMember(this)) {
    channel->broadcastMessage(":" + nickname + "!" + username + "@" + hostname +
                                  " PRIVMSG " + channelName + " :" + message,
                              this);
  } else {
    sendMessage(":Server ERROR :You are not in channel " + channelName +
                "\r\n");
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
    sendMessage(":" + nickname + "!" + username + "@" + hostname +
                " NICK :" + nickname);
    sendMessage(":Server NOTICE " + nickname + " :Nickname set to " +
                newNickname);
  } else {
    sendMessage(":Server 433 " + nickname + " " + newNickname +
                " :Nickname is already in use");
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
  sendMessage(":Server 001 " + nickname + " :Welcome to the server, " +
              nickname + "!");
}

void ClientHandler::handleJoinCommand(const std::string& parameters) {
    if (parameters == ":" || parameters.empty()) {
        sendMessage(":Server 451 * JOIN :You have not registered.");
        return;
    }

    std::size_t firstSpace = parameters.find(' ');
    std::string channelName = parameters.substr(0, firstSpace);
    std::string password = (firstSpace != std::string::npos) ? parameters.substr(firstSpace + 1) : "";

    if (channelName.empty()) {
        sendMessage(":Server 451 * JOIN :You have not specified a channel name.");
        return;
    }
    if (isAlreadyInChannel(channelName)) {
        return;
    }

    Channel* channel = getOrCreateChannel(channelName);
    if (channel) {
        joinChannel(channel, channelName, password);
    } else {
        sendMessage(":Server 403 " + channelName + " :No such channel");
    }
}

bool ClientHandler::isAlreadyInChannel(const std::string& channelName) {
    if (channels.find(channelName) != channels.end()) {
        sendMessage(":Server ERROR :You are already in channel " + channelName + "\r\n");
        return true;
    }
    return false;
}

Channel* ClientHandler::getOrCreateChannel(const std::string& channelName) {
    Channel* channel = server->findChannel(channelName);
    if (channel == nullptr) {
        server->createChannel(channelName);
        channel = server->findChannel(channelName);
    }
    return channel;
}

void ClientHandler::joinChannel(Channel* channel, const std::string& channelName, const std::string& password) {
    if (channel->isInviteOnly()) {
        sendMessage(":Server 473 " + nickname + " " + channelName + " :Cannot join channel (+i) - invite only");
    } else if (channel->isFull()) {
        sendMessage(":Server 471 " + nickname + " " + channelName + " :Cannot join channel (+l) - channel is full");
    } else if (channel->checkPassword(password)) {
        channel->addClient(this, password);
        channels.insert(channelName);
        broadcastJoinMessage(channel, channelName);
    } else {
            sendMessage(":Server 475 " + nickname + " " + channelName + " :Cannot join channel (+k) - bad key");
    }
}

void ClientHandler::broadcastJoinMessage(Channel* channel, const std::string& channelName) {
    std::string message = ":" + nickname + "!" + username + "@" + hostname + " JOIN :" + channelName + "\r\n";
    message += ":Server 353 " + nickname + " = " + channelName + " :";
    message += channel->getClientList() + "\r\n";
    message += ":Server 366 " + nickname + " " + channelName + " :End of /NAMES list.";
    sendMessage(message);
    channel->broadcastMessage(message, this);
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
    sendMessage(":" + nickname + "!" + username + "@" + hostname +
                " PART :" + parameters);
  }
}

void ClientHandler::handleKickCommand(const std::string& parameters) {
  std::string channelName;
  std::string targetName;
  std::istringstream paramStream(parameters);
  paramStream >> channelName >> targetName;
  if (channelName.empty() || targetName.empty()) {
    sendMessage(":Server ERROR :Invalid KICK command format.\r\n");
    return;
  }
  Channel* channel = server->findChannel(channelName);
  if (channel == nullptr) {
    return;
  }
  ClientHandler *target = server->findClientHandlerByNickname(targetName);

  // kick하는 주체가 그 채널의 operator인지 확인
  if (!channel->isOperator(this)) {
    sendMessage("Server ERROR :You are not an operator in channel " +
                channelName + "\r\n");
    // 그 채널에 target이 있는지 확인
  } else if (channel->isClientMember(target))
  {
      std::string message = ":" + nickname + "!" + username + "@" + hostname + " KICK " + channel->getChannelName() + " " + targetName;
      sendMessage(message);
      channel->broadcastMessage(message, this);
      channel->removeClient(target);

//      channels.erase(targetName);
//     sendMessage(
//      ":" + nickname + "!" + username + "@" + hostname + " KICK " +targetName);
//      sendMessage(
//      ":" + nickname + "!" + username + "@" + hostname + " NOTICE " + channelName + " " + targetName + " has left "+ channelName);

      // channel->broadcastMessage(
      //   ":" + nickname + "!" + username + "@" + hostname + " NOTICE " + channelName + " " + targetName + " has left "+ channelName,
      //   this);
  } else {
    sendMessage("Server ERROR :" + targetName + " is not in channel " +
                channelName + "\r\n");
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

void ClientHandler::deactivate() {
    active = false;
}

std::string ClientHandler::getNickname() const {
    return nickname;
}

std::string ClientHandler::getUsername() const {
    return username;
}

std::string ClientHandler::getHostname() const {
    return hostname;
}

bool ClientHandler::isActive() const {
    return active;
}
