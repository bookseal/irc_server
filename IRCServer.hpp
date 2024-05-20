#ifndef IRCSERVER_HPP
#define IRCSERVER_HPP

#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

class ClientHandler;
class Channel;

class IRCServer {
 public:
  IRCServer(const int port, const std::string password);
  ~IRCServer();

  bool initializeServerSocket();
  void cleanUpInactiveHandlers();
  void run();
  void acceptNewClient();

  bool isNicknameAvailable(const std::string& nickname);
  void registerNickname(const std::string& nickname, ClientHandler* handler);
  void unregisterNickname(const std::string& nickname);
  ClientHandler* findClientHandlerByNickname(const std::string& nickname);

  void createChannel(const std::string& channelName);
  Channel* findChannel(const std::string& channelName);

  void sendMessageToUser(const std::string& senderNickname,
                         const std::string& recipientNickname,
                         const std::string& message);
  const std::string getPassword() const;
  static void handleSigtstp(int signum);
  static void handleSigcont(int signum);

 private:
  const int port;  // 큰 빌딩의 사무실 번호 (RC 서버가 포트 6667에 바인드 됩.
                   // 6667: 빌딩번호)
  std::string password;  // 사무실 문 앞에 있는 비밀번호
  int serverSocket;      // 서버의 "문" 역할
  std::vector<struct pollfd> fds;
  std::map<int, ClientHandler*> clientHandlers;
  std::map<std::string, ClientHandler*> activeNicknames;
  std::map<std::string, Channel*> channels;
  static struct termios orig_termios;  // 터미널 상태를 저장
};

#endif
