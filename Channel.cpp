#include "Channel.hpp"
#include "IRCServer.hpp"
#include "ClientHandler.hpp"


void Channel::setMode(const std::string& mode, ClientHandler* operatorHandler) {
    if (!isOperator(operatorHandler)) {
        operatorHandler->sendMessage(":Server 482 " + operatorHandler->getNickname() + " " + name + " :You must be a channel op or higher to set channel mode.");
        return;
    }

    if (mode.substr(0, 2) == "+i") {
        setInviteMode(true, operatorHandler);
    } else if (mode.substr(0, 2) == "-i") {
        setInviteMode(false, operatorHandler);
    } else if (mode.substr(0, 2) == "+t") {
        setTopicControlMode(true, operatorHandler);
    } else if (mode.substr(0, 2) == "-t") {
        setTopicControlMode(false, operatorHandler);
    } else if (mode.substr(0, 2) == "+k") {
        std::string password = mode.substr(3);
        if (!password.empty()) {
            setPasswordMode(password, operatorHandler);
        }
    } else if (mode.substr(0, 2) == "-k" && hasPassword()) {
        removePasswordMode(operatorHandler);
    }
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
    //127.000.000.001.06667-127.000.000.001.35268: :nick1!root@127.0.0.1 MODE #test +k :hello
    sendModeChangeMessage(operatorHandler, "+k :" + password);
}

void Channel::removePasswordMode(ClientHandler* operatorHandler) {
    removeChannelPassword();
    sendModeChangeMessage(operatorHandler, "-k");
}

void Channel::sendModeChangeMessage(ClientHandler* operatorHandler, const std::string& modeChange) {
    std::string message = ":" + operatorHandler->getNickname() + "!" + operatorHandler->getUsername() + "@" + operatorHandler->getHostname() + " MODE " + name + " :" + modeChange;
    operatorHandler->sendMessage(message);
    broadcastMessage(message, operatorHandler);
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

Channel::Channel(const std::string& name) : name(name), inviteOnly(false), topicControl(false) {
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
    broadcastMessage(":" + client->getNickname() + " has been granted operator status in " + name, nullptr);
}

void Channel::removeOperator(ClientHandler* client) {
    operators.erase(client);
    if (client->isActive()) {
        client->sendMessage(":" + client->getNickname() + " operator status revoked in " + name);
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
