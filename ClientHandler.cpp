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
  std::string trimmedCommand = fullCommand;

  trimmedCommand.erase(
      std::remove(trimmedCommand.begin(), trimmedCommand.end(), '\r'),
      trimmedCommand.end());
  trimmedCommand.erase(
      std::remove(trimmedCommand.begin(), trimmedCommand.end(), '\n'),
      trimmedCommand.end());

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
  } else if (command == "PING") {
    sendMessage(":Server PONG Server :Server");
  } else if (command == "CAP" || command == "WHOIS" || command == "PASS") {
    ;
  } else if (command == "KICK") {
    handleKickCommand(parameters);
  } else if (command == "INVITE") {
    handleInviteCommand(parameters);
  } else if (command == "TOPIC")
    handleTopicCommand(parameters); 
  else {
    defaultMessageHandling(command + " " + parameters);
  }
}

void ClientHandler::handleModeCommand(const std::string& parameters) {
  std::string target;
  std::string mode;
  size_t spacePos = parameters.find(' ');
  if (spacePos == std::string::npos) {
    if (parameters.empty()) {
      sendMessage(":Server ERROR :Invalid MODE command format.");
      return;
    }
    target = nickname;
    mode = parameters;
  } else {
    target = parameters.substr(0, spacePos);
    mode = parameters.substr(spacePos + 1);
  }

  if (target == nickname) {
    sendMessage(":" + nickname + "!" + username + "@" + hostname + " MODE " +
                nickname + " " + mode);
    return;
  }

  Channel* channel = server->findChannel(target);
  if (channel) {
    channel->setMode(mode, this);
  } else {
    sendMessage(":Server 403 " + nickname + " " + target + " :No such channel");
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
    sendMessage(":Server ERROR :Invalid PRIVMSG format.");
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
        ":Server ERROR :Please choose a nickname with the NICK command.");
    return;
  }
  if (!currentChannel.empty()) {
    handleChannelMessage(currentChannel, message);
  } else {
    sendMessage(":Server ERROR :No channel selected or unrecognized command.");
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
    sendMessage(":Server ERROR :You are not in channel " + channelName);
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
        if (joinChannel(channel, channelName, password)) { 
        }
    } else {
        sendMessage(":Server 403 " + channelName + " :No such channel");
    }
}

bool ClientHandler::isAlreadyInChannel(const std::string& channelName) {
  if (channels.find(channelName) != channels.end()) {
    sendMessage(":Server ERROR :You are already in channel " + channelName +
                "\r\n");
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

bool ClientHandler::joinChannel(Channel* channel, const std::string& channelName, const std::string& password) {
    if (channel->isInviteOnly()) {
        sendMessage(":Server 473 " + nickname + " " + channelName + " :Cannot join channel (+i) - invite only");
        return false;
    } else if (channel->isFull()) {
        sendMessage(":Server 471 " + nickname + " " + channelName + " :Cannot join channel (+l) - channel is full");
        return false;
    } else if (channel->checkPassword(password)) {
        channel->addClient(this, password);
        channels.insert(channelName);
        broadcastJoinMessage(channel, channelName);
        return true;
    } else {
        sendMessage(":Server 475 " + nickname + " " + channelName + " :Cannot join channel (+k) - bad key");
        return false;
    }
}

void ClientHandler::broadcastJoinMessage(Channel* channel, const std::string& channelName) {
    std::string message = ":" + nickname + "!" + username + "@" + hostname + 
                          " JOIN :" + channelName + "\r\n";
    message += ":Server 353 " + nickname + " = " + channelName + " :" +
               channel->getClientList() + "\r\n";
    message += ":Server 366 " + nickname + " " + channelName + " :End of /NAMES list.";
    sendMessage(message);

    channel->broadcastMessage(message, this);
    std::string welcomeMessage = "-------------------------- Welcome to " + channelName + ", " + nickname + "!" + 
                                " We're glad to have you here. Here are a few things to get you started: Be respectful and follow the channel rules." + 
                                " If you need any help, feel free to ask the operators! Enjoy your stay!" + 
                                " ----------------------------";

    sendMessage(":" + nickname + "!" + username + "@" + hostname + " PRIVMSG " + channelName + " :" + welcomeMessage);
}


void ClientHandler::handleLeaveCommand(const std::string& parameters) {
  if (channels.find(parameters) == channels.end()) {
    sendMessage(":Server ERROR :You are not in channel " + parameters);
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
    sendMessage(":Server ERROR :Invalid KICK command format.");
    return;
  }
  Channel* channel = server->findChannel(channelName);
  if (channel == nullptr) {
    return;
  }
  ClientHandler* target = server->findClientHandlerByNickname(targetName);

  // kick하는 주체가 그 채널의 operator인지 확인
  if (!channel->isOperator(this)) {
    sendMessage("Server ERROR :You are not an operator in channel " +
                channelName + "\r\n");
    // 그 채널에 target이 있는지 확인
  } else if (channel->isClientMember(target)) {
    std::string message = ":" + nickname + "!" + username + "@" + hostname +
                          " KICK " + channel->getChannelName() + " " +
                          targetName;
    sendMessage(message);
    channel->broadcastMessage(message, this);
    channel->removeClient(target);
    target->eraseChannel(channel);
  } else {
    sendMessage("Server ERROR :" + targetName + " is not on channel " +
                channelName);
  }
}

void ClientHandler::handleTopicCommand(const std::string& parameters) {
  std::cout << "hihi\n";
    size_t spacePos = parameters.find(' ');
    std::string channelName = (spacePos != std::string::npos) ? parameters.substr(0, spacePos) : parameters;
    std::string newTopic = (spacePos != std::string::npos) ? parameters.substr(spacePos + 1) : "";
    
    Channel* channel = server->findChannel(channelName);
    if (!channel) {
        sendMessage(":Server 403 " + nickname + " " + channelName + " :No such channel");
        return;
    }

    if (!channel->isClientMember(this)) {
        sendMessage(":Server 442 " + channelName + " :You're not on that channel");
        return;
    }

    if (channel->getTopicControl() && !channel->isOperator(this)) {
        sendMessage(":Server 482 " + channelName + " :You're not channel operator");
        return;
    }

    channel->setTopic(newTopic, nickname);
    std::string topicMessage = ":" + nickname + "!" + username + "@" + hostname + " TOPIC " + channelName + " :" + newTopic;

    channel->broadcastMessage(topicMessage, NULL);
}

void ClientHandler::handleInviteCommand(const std::string& parameters) {
  std::string targetName;
  std::string channelName;
  std::istringstream paramStream(parameters);
  paramStream >> targetName >> channelName;
  if (targetName.empty() || channelName.empty()) {
    sendMessage(":Server ERROR :Invalid INVITE command format.");
    return;
  }
  ClientHandler* target = server->findClientHandlerByNickname(targetName);
  Channel* channel = server->findChannel(channelName);
  if (target == nullptr) {
    sendMessage(":Server ERROR :No such nick " + targetName);
  } else if (channel == nullptr) {
    sendMessage(":Server ERROR :No such channel " + channelName);
  } else if (!channel->isClientMember(this)) {
    sendMessage(":Server ERROR :You are not on channel " + channelName);
  } else if (channel->isClientMember(target)) {
    sendMessage(":Server ERROR :" + targetName + " is already on channel " +
                channelName + "\r\n");
  } else if (!channel->isOperator(this)) {
    sendMessage(
        ":Server ERROR :You must be a channel op or higher to send an "
        "invite.\r\n");
  } else {
    std::string message = ":" + nickname + "!" + username + "@" + hostname +
                          " INVITE " + targetName + " " + channelName;
    target->sendMessage(message);
    channel->broadcastMessage(message, this);
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

void ClientHandler::deactivate() { active = false; }

std::string ClientHandler::getNickname() const { return nickname; }

std::string ClientHandler::getUsername() const { return username; }

std::string ClientHandler::getHostname() const { return hostname; }

bool ClientHandler::isActive() const { return active; }

void ClientHandler::eraseChannel(Channel* channel) { channels.erase(channel->getName()); }