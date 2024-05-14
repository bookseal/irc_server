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
    void addClient(ClientHandler* client, const std::string& password = "");
    void removeClient(ClientHandler* client);
    bool isClientMember(ClientHandler* client) const;
    bool isOperator(ClientHandler* client) const; // Check if a client is an operator
    void addOperator(ClientHandler* client); // Add an operator
    void removeOperator(ClientHandler* client); // Remove an operator
    void broadcastMessage(const std::string& message, ClientHandler* sender);
    bool isEmpty() const;
    std::string getClientList() const; // Return list of clients in the channel
    std::string getChannelName() const { return name; }; // Return list of channel names

    // Mode settings
    void setInviteOnly(bool inviteOnly);
    bool isInviteOnly() const;
    void setMode(const std::string& mode, ClientHandler* operatorHandler); // Set specific mode
    void setTopicControl(bool mode); // Set topic control mode
    bool getTopicControl() const; // Get topic control status
    void setInviteMode(bool enable, ClientHandler* operatorHandler);
    void setTopicControlMode(bool enable, ClientHandler* operatorHandler);
    void setPasswordMode(const std::string& password, ClientHandler* operatorHandler);
    void removePasswordMode(ClientHandler* operatorHandler);
    void sendModeChangeMessage(ClientHandler* operatorHandler, const std::string& modeChange);

    // Password management
    void setChannelPassword(const std::string& password);
    void removeChannelPassword();
    bool checkPassword(const std::string& password) const;
    bool hasPassword() const; // Check if the channel has a password

private:
    std::string name;
    std::map<ClientHandler*, bool> clients; // Maps clients to a bool (typically if they are active/not banned)
    std::set<ClientHandler*> operators; // Set of operators in this channel
    bool inviteOnly; // Whether the channel is invite-only
    bool topicControl; // Whether topic control is restricted to operators
    std::string channelPassword; // Optional password for the channel
};

#endif // CHANNEL_HPP
