#ifndef CLIENT_HANDLER_HPP
#define CLIENT_HANDLER_HPP

#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/socket.h>

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

private:
    IRCServer *server;
    int clientSocket;
    bool active;
    std::string nickname;
    std::string username;

    void processCommand(const std::string& fullCommand);
    void parseCommand(const std::string& command, const std::string& parameters);
    void handleNickCommand(const std::string& parameters);
    void handleUserCommand(const std::string& parameters);
    void handleJoinCommand(const std::string& parameters);
    void handlePrivMsgCommand(const std::string& parameters);
};

#endif // CLIENT_HANDLER_HPP
