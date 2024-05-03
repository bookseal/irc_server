#include "Channel.hpp"
#include "IRCServer.hpp"
#include "ClientHandler.hpp"

Channel::Channel(const std::string& name) : name(name) {
}

Channel::~Channel() {}

const std::string& Channel::getName() const {
    return name;
}

void Channel::addClient(ClientHandler* client) {
    // Assuming 'true' is the value you want to associate with the client
    clients.insert(std::make_pair(client, true));
}

void Channel::removeClient(ClientHandler* client) {
    clients.erase(client);
}

bool Channel::isClientMember(ClientHandler* client) const {
    return clients.find(client) != clients.end();
}

void Channel::broadcastMessage(const std::string& message, ClientHandler* sender) {
    std::map<ClientHandler*, bool>::iterator it;
    for (it = clients.begin(); it != clients.end(); ++it) {
        if (it->first != sender) {
            it->first->sendMessage(message);
        }
    }
}

bool Channel::isEmpty() const {
    return clients.empty();
}
