#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <set>
#include <map>

class ClientHandler; // Forward declaration
class IRCServer; // Forward declaration

class Channel {
public:
    Channel(const std::string& name);
    ~Channel();

    const std::string& getName() const;
    void addClient(ClientHandler* client);
    void removeClient(ClientHandler* client);
    bool isClientMember(ClientHandler* client) const;
    bool isOperator(ClientHandler* client) const; // Check if a client is an operator
    void addOperator(ClientHandler* client); // Add an operator
    void removeOperator(ClientHandler* client); // Remove an operator
    void broadcastMessage(const std::string& message, ClientHandler* sender);
    bool isEmpty() const;
    std::string getChannelName() const { return name; }

private:
    std::string name;
    std::map<ClientHandler*, bool> clients; // Map to keep track of clients in this channel
    std::set<ClientHandler*> operators; // Set to keep track of operators in this channel
};

#endif // CHANNEL_HPP