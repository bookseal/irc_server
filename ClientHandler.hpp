#ifndef CLIENT_HANDLER_HPP
#define CLIENT_HANDLER_HPP

#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <sstream>
#include <set>

class Channel;
class IRCServer;

class ClientHandler {
public:
    ClientHandler(int socket, IRCServer* server);
    ~ClientHandler();
    void processInput();
    void sendMessage(const std::string& message);
    bool isActive() const { return active; }
    void deactivate() { active = false; }
    void handleDisconnect();
    std::string getNickname() const { return nickname; };
    std::string getUsername() const { return username; };
    std::string getHostname() const { return hostname; };
    IRCServer *getServer() { return server; }

private:
    IRCServer *server;
    int clientSocket;
    bool active;
    std::string nickname;
    std::string username;
    std::string hostname;
    std::string currentChannel;
    std::set<std::string> channels;

    void processCommand(const std::string& fullCommand);
    void defaultMessageHandling(const std::string& message);
    void parseCommand(const std::string& command, const std::string& parameters);
    void handleNickCommand(const std::string& parameters);
    void handleUserCommand(const std::string& parameters);
    void handleJoinCommand(const std::string& parameters);
    void handleLeaveCommand(const std::string& parameters);
    void handleChannelMessage(const std::string& channelName, const std::string& message);
    void handlePrivMsgCommand(const std::string& parameters);
    void handleModeCommand(const std::string& parameters);
};

#endif // CLIENT_HANDLER_HPP
