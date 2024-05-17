#include "IRCServer.hpp"

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cout << "Usage: ./ircserv <port> <password>" << std::endl;
    return 1;
  }
  std::string password = argv[2];
  std::istringstream iss(argv[1]);
  int port = 0;
  iss >> port;
  if (iss.fail() || !iss.eof() || port <= 0 || password.empty()) {
    std::cout << "Invalid port or password." << std::endl;
    return 1;
  }
  IRCServer server(port, password);
  server.run();
  return 0;
}