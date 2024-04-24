#ifndef IRCSERVER_HPP
#define IRCSERVER_HPP

#include <iostream>
#include <cstring>
#include <unistd.h>
#include <vector>
#include <poll.h>
#include <map>
#include <sys/socket.h>
#include <netinet/in.h>

class ClientHandler;
class Channel;

class IRCServer {
public:
    IRCServer(int port);
    ~IRCServer();

    bool initializeServerSocket();
    void cleanUpInactiveHandlers();
    void run();
    void acceptNewClient();

    bool isNicknameAvailable(const std::string& nickname);
    bool isUsernameAvailable(const std::string& username);
    void registerNickname(const std::string& nickname, ClientHandler* handler);
    void registerUsername(const std::string& username, ClientHandler* handler);
    void unregisterNickname(const std::string& nickname);
    void unregisterUsername(const std::string& username);
    ClientHandler* findClientHandlerByNickname(const std::string& nickname);

    void createChannel(const std::string& channelName);
    void deleteChannel(const std::string& channelName);
    Channel* findChannel(const std::string& channelName);



private:
    int port;
    int serverSocket;
    std::vector<struct pollfd> fds;
    std::map<int, ClientHandler*> clientHandlers; // Map from socket descriptors to client handlers
    std::map<std::string, ClientHandler*> activeNicknames;
    std::map<std::string, ClientHandler*> activeUsernames;
    std::map<std::string, Channel*> channels;

};


#endif
