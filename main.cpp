#include "IRCServer.hpp"

int main(int argc, char **argv) {
  int port = 0;
  std::string password;
  if (argc != 3) {
    std::cout << "Usage: ./ircserv <port> <password>" << std::endl;
    return 1;
  }
  port = atoi(argv[1]);
  password = argv[2];
  if ((port == 0 && strcmp(argv[1], "0")) || port < 0 || password.empty()) {
    std::cout << "Invalid port or password." << std::endl;
    return 1;
  }
  IRCServer server(port, password);
  server.run();
  return 0;
}