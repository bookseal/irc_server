#include "Channel.hpp"
#include "IRCServer.hpp"
#include "ClientHandler.hpp"


void Channel::setMode(const std::string& mode, ClientHandler* operatorHandler) {
    if (!isOperator(operatorHandler)) {
        operatorHandler->sendMessage(":Server 482 " + operatorHandler->getNickname() + " " + name + " :You must be a channel op or higher to set channel mode.");
        return;
    }

    std::string message = ":" + operatorHandler->getNickname() + "!" + operatorHandler->getUsername() + "@" + operatorHandler->getHostname() + " MODE " + name;

    if (mode == "+i" && !inviteOnly) {
        inviteOnly = true;
        message += " :+i";
    } else if (mode == "-i" && inviteOnly) {
        inviteOnly = false;
        message += " :-i";
    } else if (mode == "+t" && !topicControl) {
        topicControl = true;
        message += " :+t";
    } else if (mode == "-t" && topicControl) {
        topicControl = false;
        message += " :-t";
    }
    else
        return ;
    operatorHandler->sendMessage(message);
    broadcastMessage(message, operatorHandler);
}

Channel::Channel(const std::string& name) : name(name), inviteOnly(false), topicControl(false) {
}

Channel::~Channel() {}

const std::string& Channel::getName() const {
    return name;
}

void Channel::addClient(ClientHandler* client) {
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
