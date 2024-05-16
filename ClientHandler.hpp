#ifndef CLIENT_HANDLER_HPP
#define CLIENT_HANDLER_HPP

#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <set>
#include <sstream>
#include <string>

class Channel;
class IRCServer;

class ClientHandler {
 public:
  // Constructors and destructor
  ClientHandler(int socket, IRCServer* server);
  ~ClientHandler();

  // Input processing
  void processInput();
  void processCommand(const std::string& fullCommand);

  // Command handlers
  void parseCommand(const std::string& command, const std::string& parameters);
  void handleNickCommand(const std::string& parameters);
  void handleUserCommand(const std::string& parameters);
  void handleJoinCommand(const std::string& parameters);
  void handleLeaveCommand(const std::string& parameters);
  void handlePrivMsgCommand(const std::string& parameters);
  void handleModeCommand(const std::string& parameters);
  void handleKickCommand(const std::string& parameters);
  void handleInviteCommand(const std::string& parameters);
  void defaultMessageHandling(const std::string& message);
  void handleChannelMessage(const std::string& channelName,
                            const std::string& message);
  bool isAlreadyInChannel(const std::string& channelName);
  Channel* getOrCreateChannel(const std::string& channelName);
  void joinChannel(Channel* channel, const std::string& channelName,
                   const std::string& password);
  void eraseChannel(Channel* channel);
  void broadcastJoinMessage(Channel* channel, const std::string& channelName);

  // Connection management
  void handleDisconnect();
  void sendMessage(const std::string& message);

  // Status checks
  bool isActive() const;
  void deactivate();

  // Getters
  std::string getNickname() const;
  std::string getUsername() const;
  std::string getHostname() const;
  IRCServer* getServer() const;

 private:
  IRCServer* server;
  int clientSocket;
  bool active;
  std::string nickname;
  std::string username;
  std::string hostname;
  std::string currentChannel;
  std::set<std::string> channels;
};

#endif  // CLIENT_HANDLER_HPP
