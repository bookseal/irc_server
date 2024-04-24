#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <set>
#include <map>

class ClientHandler; // Forward declaration

class Channel {
public:
    Channel(const std::string& name);
    ~Channel();

    const std::string& getName() const;
    void addClient(ClientHandler* client);
    void removeClient(ClientHandler* client);
    bool isClientMember(ClientHandler* client) const;
    void broadcastMessage(const std::string& message, ClientHandler* sender);
    bool isEmpty() const;

private:
    std::string name;
    std::map<ClientHandler*, bool> clients; // Set to keep track of clients in this channel
};

#endif // CHANNEL_HPP
