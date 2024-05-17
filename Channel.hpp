#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <map>
#include <set>
#include <sstream>
#include <string>

class ClientHandler;  // Forward declaration
class IRCServer;      // Forward declaration

class Channel {
 public:
  Channel(const std::string& name);
  ~Channel();

  const std::string& getName() const;
  void addClient(ClientHandler* client);
  void removeClient(ClientHandler* client);
  bool isClientMember(ClientHandler* client) const;
  bool isOperator(
      ClientHandler* client) const;         // Check if a client is an operator
  void addOperator(ClientHandler* client);  // Add an operator
  void removeOperator(ClientHandler* client);  // Remove an operator
  void broadcastMessage(const std::string& message, ClientHandler* sender);
  bool isEmpty() const;
  std::string getClientList() const;  // Return list of clients in the channel
  std::string getChannelName() const {
    return name;
  };  // Return list of channel names

  // Mode settings
  void setInviteOnly(bool inviteOnly);
  bool isInviteOnly() const;
  void setMode(const std::string& mode,
               ClientHandler* operatorHandler);  // Set specific mode
  void setTopicControl(bool mode);               // Set topic control mode
  bool getTopicControl() const;                  // Get topic control status
  void setInviteMode(bool enable, ClientHandler* operatorHandler);
  void setTopicControlMode(bool enable, ClientHandler* operatorHandler);
  void setPasswordMode(const std::string& password,
                       ClientHandler* operatorHandler);
  void removePasswordMode(ClientHandler* operatorHandler);
  void sendModeChangeMessage(ClientHandler* operatorHandler,
                             const std::string& modeChange);
  void setOperatorMode(bool enable, const std::string& nickname,
                       ClientHandler* operatorHandler);

  // Password management
  void setChannelPassword(const std::string& password);
  void removeChannelPassword();
  bool checkPassword(const std::string& password) const;
  bool hasPassword() const;  // Check if the channel has a password

  // User limit management
  void setLimit(int limit, ClientHandler* operatorHandler);
  bool isFull() const;

  const std::string& getTopic() const;  // Get the current topic
  const std::string& getTopicSetter() const;
  // Get the nickname of the user who set the topic
  // std::time_t getTopicTimestamp() const;  // Get the timestamp when the topic
  // was set
  void setTopic(const std::string& newTopic,
                const std::string& setter);  // Set a new topic

 private:
  std::string name;
  std::map<ClientHandler*, bool> clients;  // Maps clients to a bool (typically
                                           // if they are active/not banned)
  std::set<ClientHandler*> operators;      // Set of operators in this channel
  bool inviteOnly;                         // Whether the channel is invite-only
  bool topicControl;  // Whether topic control is restricted to operators
  std::string channelPassword;  // Optional password for the channel
  size_t maxClients;        // Maximum number of clients allowed in the channel
  std::string topic;        // The current topic of the channel
  std::string topicSetter;  // The nickname of the user who set the topic
  // std::time_t topicTimestamp;  // The time when the topic was set
};

#endif  // CHANNEL_HPP
