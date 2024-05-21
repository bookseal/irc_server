#include "Channel.hpp"

#include "ClientHandler.hpp"
#include "IRCServer.hpp"

void Channel::setMode(const std::string& mode, ClientHandler* operatorHandler) {
  if (!isOperator(operatorHandler)) {
    operatorHandler->sendMessage(
        ":Server 482 " + operatorHandler->getNickname() + " " + name +
        " :You must be a channel op or higher to set channel mode.");
    return;
  }

  std::string modeFlag = mode.substr(0, 2);
  if (modeFlag == "+i") {
    setInviteMode(true, operatorHandler);
  } else if (modeFlag == "-i") {
    setInviteMode(false, operatorHandler);
  } else if (modeFlag == "+t") {
    setTopicControlMode(true, operatorHandler);
  } else if (modeFlag == "-t") {
    setTopicControlMode(false, operatorHandler);
  } else if (modeFlag == "+k") {
    if (mode.length() > 2) {
      std::string password = mode.substr(3);
      if (!password.empty())
        setPasswordMode(password, operatorHandler);
    }
    else
      operatorHandler->sendMessage(":Server 461 " + operatorHandler->getNickname() + " " + name + " :Not enough parameters");
  } else if (modeFlag == "-k" && hasPassword()) {
    removePasswordMode(operatorHandler);
  } else if (modeFlag == "+l") {
     if (mode.length() > 2) {  // Check if there are more characters after "+l"
        std::string limit = mode.substr(3);  // Get the limit following "+l"
        int limitValue = 0;
        bool valid = true;
        for (size_t i = 0; i < limit.length(); ++i) {
            if (!isdigit(limit[i])) {
                valid = false;
                break;
            }
            limitValue = limitValue * 10 + (limit[i] - '0');
        }
        if (valid && limitValue > 0) {
            setLimit(limitValue, operatorHandler);
        } else {
            operatorHandler->sendMessage(":Server 473 " + operatorHandler->getNickname() + " :Channel limit must be a number greater than 0.");
        }
    } else {
        setLimit(0, operatorHandler); // 제한을 0으로 둬서 -l 효과가 됨.
    }
  } else if (modeFlag == "-l") {
    setLimit(0, operatorHandler);
  } else if (modeFlag == "+o") {
    if (mode.length() > 2)
      setOperatorMode(true, mode.substr(3), operatorHandler);
  } else if (modeFlag == "-o") {
    if (mode.length() > 2)
      setOperatorMode(false, mode.substr(3), operatorHandler);
  }
}

void Channel::setLimit(int limit, ClientHandler* operatorHandler) {
  if (limit < 0) {
    operatorHandler->sendMessage(":Server 473 " +
                                 operatorHandler->getNickname() + " " + name +
                                 " :Channel limit must be greater than 0.");
    return;
  }
  std::string change;
  if (limit > 0) {
    std::ostringstream oss;
    oss << "+l " << limit;
    change = oss.str();
  } else {
    change = "-l";
  }
  maxClients = limit;
  sendModeChangeMessage(operatorHandler, change);
}

bool Channel::isFull() const {
  return maxClients > 0 && clients.size() >= maxClients;
}

bool Channel::isInviteOnly() const { return inviteOnly; }

void Channel::setOperatorMode(bool enable, const std::string& nickname,
                              ClientHandler* operatorHandler) {
  std::map<ClientHandler*, bool>::iterator it;
  for (it = clients.begin(); it != clients.end(); ++it) {
    if (it->first->getNickname() == nickname) {
      if (enable) {
        addOperator(it->first);
      } else {
        removeOperator(it->first);
      }
      return;
    }
  }
  operatorHandler->sendMessage(":Server 441 " + operatorHandler->getNickname() +
                               " " + name + " " + nickname +
                               " :They aren't on that channel.");
}

void Channel::setInviteMode(bool enable, ClientHandler* operatorHandler) {
  std::string change = enable ? "+i" : "-i";
  inviteOnly = enable;
  sendModeChangeMessage(operatorHandler, change);
}

void Channel::setTopicControlMode(bool enable, ClientHandler* operatorHandler) {
  std::string change = enable ? "+t" : "-t";
  topicControl = enable;
  sendModeChangeMessage(operatorHandler, change);
}

void Channel::setPasswordMode(const std::string& password,
                              ClientHandler* operatorHandler) {
  setChannelPassword(password);
  sendModeChangeMessage(operatorHandler, "+k :" + password);
}

void Channel::removePasswordMode(ClientHandler* operatorHandler) {
  removeChannelPassword();
  sendModeChangeMessage(operatorHandler, "-k");
}

void Channel::sendModeChangeMessage(ClientHandler* operatorHandler,
                                    const std::string& modeChange) {
  std::string message = ":" + operatorHandler->getNickname() + "!" +
                        operatorHandler->getUsername() + "@" +
                        operatorHandler->getHostname() + " MODE " + name +
                        " :" + modeChange;
  broadcastMessage(message, NULL);
}

void Channel::setChannelPassword(const std::string& password) {
  channelPassword = password;
}

void Channel::removeChannelPassword() { channelPassword.clear(); }

bool Channel::checkPassword(const std::string& password) const {
  return password == channelPassword;
}

bool Channel::hasPassword() const { return !channelPassword.empty(); }

Channel::Channel(const std::string& name)
    : name(name), inviteOnly(false), topicControl(true), maxClients(0) {}

Channel::~Channel() {}

const std::string& Channel::getName() const { return name; }

void Channel::addClient(ClientHandler* client) {
  clients.insert(std::make_pair(client, true));
  if (operators.empty()) {
    addOperator(client);
  }
  if (!topic.empty()) {
    std::string topicMessage =
        ":Server 332 " + client->getNickname() + " " + name + " :" + topic;
    client->sendMessage(topicMessage);
    topicMessage =
        ":Server 333 " + client->getNickname() + " " + name + " " + topicSetter;
    client->sendMessage(topicMessage);
  }
}

void Channel::removeClient(ClientHandler* client) {
  clients.erase(client);
  if (isOperator(client)) removeOperator(client);
}

bool Channel::isClientMember(ClientHandler* client) const {
  return clients.find(client) != clients.end();
}

bool Channel::isOperator(ClientHandler* client) const {
  return operators.find(client) != operators.end();
}

void Channel::addOperator(ClientHandler* client) {
  operators.insert(client);
  broadcastMessage(":" + client->getNickname() + "!" + client->getUsername() +
                       "@" + client->getHostname() + " MODE " + name +
                       " +o :" + client->getNickname(),
                   NULL);
}

void Channel::removeOperator(ClientHandler* client) {
  operators.erase(client);
  if (client->isActive()) {
    broadcastMessage(":" + client->getNickname() + "!" + client->getUsername() +
                         "@" + client->getHostname() + " MODE " + name +
                         " -o :" + client->getNickname(),
                     NULL);
  }
}



void Channel::broadcastMessage(const std::string& message,
                               ClientHandler* sender) {
  std::map<ClientHandler*, bool>::iterator it;
  for (it = clients.begin(); it != clients.end(); ++it) {
    if (sender == NULL || it->first != sender) {
      it->first->sendMessage(message);
    }
  }
}

bool Channel::isEmpty() const { return clients.empty(); }

std::string Channel::getClientList() const {
  std::string list;
  std::map<ClientHandler*, bool>::const_iterator it;
  for (it = clients.begin(); it != clients.end(); ++it) {
    if (it->second && isOperator(it->first)) {
      list += "@";
    } else {
      list += " ";
    }
    list += it->first->getNickname() + " ";
  }
  return list;
}

// New methods for handling topics

const std::string& Channel::getTopic() const { return topic; }

const std::string& Channel::getTopicSetter() const { return topicSetter; }

// std::time_t Channel::getTopicTimestamp() const { return topicTimestamp; }

void Channel::setTopic(const std::string& newTopic, const std::string& setter) {
    topic = newTopic;
    topicSetter = setter;
    std::string topicMessage = ":" + setter + " TOPIC " + name + " :" + newTopic;
}

bool Channel::getTopicControl() const { return topicControl; }

bool Channel::checkInvitation(ClientHandler *client) {
    std::cout << "Checking invitation for " << client->getNickname() << std::endl;
    std::cout << "Invited size: " << invited.size() << std::endl;
    std::cout << "Invited contains client: " << (invited.find(client) != invited.end()) << std::endl;
    return invited.find(client) != invited.end();
}
void Channel::inviteClient(ClientHandler *client){
    invited.insert(client);
}
void Channel::removeInvitation(ClientHandler *client){
    if (invited.find(client) != invited.end()) {
        invited.erase(client);
    }
}