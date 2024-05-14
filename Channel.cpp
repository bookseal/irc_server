#include "Channel.hpp"
#include "IRCServer.hpp"
#include "ClientHandler.hpp"

void Channel::setMode(const std::string& mode, ClientHandler* operatorHandler) {
    if (!isOperator(operatorHandler)) {
        operatorHandler->sendMessage(":Server 482 " + operatorHandler->getNickname() + " " + name + " :You must be a channel op or higher to set channel mode.");
        return;
    }

    std::string modeFlag = mode.substr(0, 2); // Store the mode flag for easier comparison

    if (modeFlag == "+i") {
        setInviteMode(true, operatorHandler);
    } else if (modeFlag == "-i") {
        setInviteMode(false, operatorHandler);
    } else if (modeFlag == "+t") {
        setTopicControlMode(true, operatorHandler);
    } else if (modeFlag == "-t") {
        setTopicControlMode(false, operatorHandler);
    } else if (modeFlag == "+k") {
        std::string password = mode.substr(3); // Get the password following "+k "
        if (!password.empty()) {
            setPasswordMode(password, operatorHandler);
        }
    } else if (modeFlag == "-k" && hasPassword()) {
        removePasswordMode(operatorHandler);
    } else if (modeFlag == "+l") {
        std::string limit = mode.substr(3); // Get the limit following "+l "
        if (!limit.empty()) {
            int limitValue = std::stoi(limit);
            setLimit(limitValue, operatorHandler);
        }
    } else if (modeFlag == "-l") {
        setLimit(0, operatorHandler);
    } else if (mode.substr(0, 2) == "-k" && hasPassword()) {
            removePasswordMode(operatorHandler);
    } else if (mode.substr(0, 2) == "+o") {
        setOperatorMode(true, mode.substr(3), operatorHandler);
    } else if (mode.substr(0, 2) == "-o") {
        setOperatorMode(false, mode.substr(3), operatorHandler);
    }
}

void Channel::setLimit(int limit, ClientHandler* operatorHandler) {
    if (limit < 0) {
        operatorHandler->sendMessage(":Server 473 " + operatorHandler->getNickname() + " " + name + " :Channel limit must be greater than 0.");
        return;
    }
    std::string change = limit > 0 ? "+l " + std::to_string(limit) : "-l";
    maxClients = limit;
    sendModeChangeMessage(operatorHandler, change);
}

bool Channel::isFull() const {
    return maxClients > 0 && clients.size() >= maxClients;
}

bool Channel::isInviteOnly() const {
    return inviteOnly;
}

void Channel::setOperatorMode(bool enable, const std::string& nickname, ClientHandler* operatorHandler) {
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
    operatorHandler->sendMessage(":Server 441 " + operatorHandler->getNickname() + " " + name + " " + nickname + " :They aren't on that channel.");
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

void Channel::setPasswordMode(const std::string& password, ClientHandler* operatorHandler) {
    setChannelPassword(password);
    sendModeChangeMessage(operatorHandler, "+k :" + password);
}

void Channel::removePasswordMode(ClientHandler* operatorHandler) {
    removeChannelPassword();
    sendModeChangeMessage(operatorHandler, "-k");
}

void Channel::sendModeChangeMessage(ClientHandler* operatorHandler, const std::string& modeChange) {
    std::string message = ":" + operatorHandler->getNickname() + "!" + operatorHandler->getUsername() + "@" + operatorHandler->getHostname() + " MODE " + name + " :" + modeChange;
    broadcastMessage(message, nullptr);
}

void Channel::setChannelPassword(const std::string& password) {
    channelPassword = password;
}

void Channel::removeChannelPassword() {
    channelPassword.clear();
}

bool Channel::checkPassword(const std::string& password) const {
    return password == channelPassword;
}

bool Channel::hasPassword() const {
    return !channelPassword.empty();
}

Channel::Channel(const std::string& name) : name(name), inviteOnly(false), topicControl(false), maxClients(0) {
}

Channel::~Channel() {}

const std::string& Channel::getName() const {
    return name;
}

void Channel::addClient(ClientHandler* client, const std::string& password) {
    clients.insert(std::make_pair(client, true));
    if (operators.empty()) {
        addOperator(client);
    }
}

void Channel::removeClient(ClientHandler* client) {
    clients.erase(client);
    if (isOperator(client))
        removeOperator(client);
}

bool Channel::isClientMember(ClientHandler* client) const {
    return clients.find(client) != clients.end();
}

bool Channel::isOperator(ClientHandler* client) const {
    return operators.find(client) != operators.end();
}

void Channel::addOperator(ClientHandler* client) {
    operators.insert(client);
    broadcastMessage(":" + client->getNickname() + "!" + client->getUsername() + "@" + client->getHostname() + " MODE " + name + " +o :" + client->getNickname(), nullptr);
}

void Channel::removeOperator(ClientHandler* client) {
    operators.erase(client);
    if (client->isActive()) {
        broadcastMessage(":" + client->getNickname() + "!" + client->getUsername() + "@" + client->getHostname() + " MODE " + name + " -o :" + client->getNickname(), nullptr);
    }
}

void Channel::broadcastMessage(const std::string& message, ClientHandler* sender) {
    std::map<ClientHandler*, bool>::iterator it;
    for (it = clients.begin(); it != clients.end(); ++it) {
        if (sender == nullptr || it->first != sender) {
            it->first->sendMessage(message);
        }
    }
}

bool Channel::isEmpty() const {
    return clients.empty();
}

std::string Channel::getClientList() const {
    std::string list;
    std::map<ClientHandler*, bool>::const_iterator it;
    for (it = clients.begin(); it != clients.end(); ++it) {
        if (it->second && isOperator(it->first)) {
            list += "@";
        }
        else
        {
            list += " ";
        }
        list += it->first->getNickname() + " ";
    }
    return list;
}
