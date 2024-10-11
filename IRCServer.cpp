#include "IRCServer.hpp"

#include "Channel.hpp"
#include "ClientHandler.hpp"
struct termios IRCServer::orig_termios;

// Send a message to a specific user on the IRC server
void IRCServer::sendMessageToUser(const std::string& senderNickname,
                                  const std::string& recipientNickname,
                                  const std::string& message) {
  std::cout << "senderNickname   : " << senderNickname
            << std::endl;  // The person sending the message
  std::cout << "recipientNickname: " << recipientNickname
            << std::endl;  // The person receiving the message
  std::cout << "message          : " << message << "\n";

  // Create the message in the right IRC format
  std::string newMessage = ":" + senderNickname + "!user@host PRIVMSG " +
                           recipientNickname + " :" + message;

  if (activeNicknames.count(recipientNickname) > 0) {  // Check if the recipient exists
    ClientHandler* recipientHandler = activeNicknames[recipientNickname];
    if (recipientHandler) {  // If recipient is found, send the message
      recipientHandler->sendMessage(newMessage);
    } else {  // If the recipient cannot be found
      std::cerr << "Handler for nickname '" << recipientNickname
                << "' not found." << std::endl;
      ClientHandler* senderHandler = activeNicknames[senderNickname];
      if (senderHandler) {
        senderHandler->sendMessage(
            ":Server ERROR :Internal error, recipient handler not found.\r\n");
      }
    }
  } else {  // If the recipient's nickname is not found
    std::cerr << "No user with nickname '" << recipientNickname << "' found."
              << std::endl;
    ClientHandler* senderHandler = activeNicknames[senderNickname];
    if (senderHandler) {
      senderHandler->sendMessage(":Server ERROR :No such nick/channel.\r\n");
    }
  }
}

IRCServer::IRCServer(const int port, const std::string password)
    : port(port), password(password), serverSocket(-1) {
  tcgetattr(STDIN_FILENO, &orig_termios);  // Save terminal settings
  signal(SIGTSTP, handleSigtstp);
  signal(SIGCONT, handleSigcont);
}

IRCServer::~IRCServer() {
  close(serverSocket);  // Close the server socket
  // Clean up client handlers
  for (std::map<int, ClientHandler*>::iterator it = clientHandlers.begin();
       it != clientHandlers.end(); ++it) {
    delete it->second;
  }
  // Clean up channels
  for (std::map<std::string, Channel*>::iterator it = channels.begin();
       it != channels.end(); ++it) {
    delete it->second;
  }
}

bool IRCServer::initializeServerSocket() {
  // Create a socket (like setting up a mailbox for communication)
  // AF_INET: Using IPv4; SOCK_STREAM: TCP socket (reliable connection)
  serverSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (serverSocket < 0) {
    std::cerr << "Failed to create socket." << std::endl;
    return false;
  }

  // Set the socket options: Allow reusing the address (like quickly reusing a parking spot)
  int opt = 1;
  if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    std::cerr << "Error setting socket options." << std::endl;
    return false;
  }

  // Set up the server address (like setting up the reception desk in an office)
  struct sockaddr_in serverAddr;
  memset(&serverAddr, 0, sizeof(serverAddr));  // Set memory to 0
  serverAddr.sin_family = AF_INET;  // Use IPv4 addresses
  serverAddr.sin_addr.s_addr = INADDR_ANY;  // Listen on all interfaces
  serverAddr.sin_port = htons(port);  // Set the port number

  // Bind the socket to the address (like assigning an address to the mailbox)
  if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
    std::cerr << "Failed to bind to port " << port << "." << std::endl;
    return false;
  }

  // Listen: Wait for connections (like a post office waiting for mail)
  if (listen(serverSocket, 10) < 0) {  // Listen for up to 10 connections
    std::cerr << "Failed to listen on socket." << std::endl;
    return false;
  }

  // Add the socket to the list of file descriptors to monitor
  struct pollfd listenFD;
  listenFD.fd = serverSocket;  // Set the file descriptor to the server socket
  listenFD.events = POLLIN;  // Monitor for incoming data
  fds.push_back(listenFD);  // Add it to the list of descriptors
  return true;
}

void IRCServer::run() {
  if (!initializeServerSocket()) {  // Set up the server socket
    std::cerr << "Server initialization failed." << std::endl;
    return;
  }
  std::cout << "Server running on port " << port << std::endl;

  while (true) {
    // Poll: Check for events on file descriptors (like a store clerk checking if customers need help)
    int pollCount = poll(&fds[0], fds.size(), -1);
    if (pollCount < 0) {
      std::cerr << "Poll error." << std::endl;
      break;
    }

    // Go through each file descriptor and check for incoming data
    for (size_t i = 0; i < fds.size(); i++) {
      if (fds[i].revents & POLLIN) {  // If there is incoming data
        if (fds[i].fd == serverSocket) {  // If the server socket is receiving a new connection
          acceptNewClient();
        } else {  // If an existing client is sending data
          ClientHandler* handler = clientHandlers[fds[i].fd];  // Find the handler for that client
          if (handler) {  // If the handler is found, process the input
            handler->processInput();
          }
        }
      }
    }
    cleanUpInactiveHandlers();  // Clean up any inactive client handlers
  }
}

// Accept a new client
void IRCServer::acceptNewClient() {
  struct sockaddr_in clientAddr;  // Store client address
  socklen_t clientAddrLen = sizeof(clientAddr);

  // Accept a new client connection
  int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
  if (clientSocket < 0) {  // If accepting the connection fails
    std::cerr << "Error accepting new connection." << std::endl;
    return;
  }

  // Create a new handler for the client
  ClientHandler* newHandler = new ClientHandler(clientSocket, this);
  clientHandlers[clientSocket] = newHandler;

  // Set up pollfd to monitor this client's socket for incoming data
  struct pollfd clientFD;
  clientFD.fd = clientSocket;  // Set the client's file descriptor
  clientFD.events = POLLIN;  // Monitor for incoming data
  fds.push_back(clientFD);  // Add this client to the list of descriptors to monitor

  std::cout << "New client connected: " << clientSocket << std::endl;
}

void IRCServer::cleanUpInactiveHandlers() {
  std::map<int, ClientHandler*>::iterator it = clientHandlers.begin();
  while (it != clientHandlers.end()) {
    if (!it->second->isActive()) {  // If the handler is no longer active
      std::cout << "Cleaning up client handler for socket: " << it->first << std::endl;
      close(it->first);  // Close the socket
      delete it->second;  // Delete the handler
      std::map<int, ClientHandler*>::iterator temp = it;  // Use a temporary iterator
      ++it;  // Move to the next item
      clientHandlers.erase(temp);  // Remove the handler from the map
    } else {
      ++it;
    }
  }
}

bool IRCServer::isNicknameAvailable(const std::string& nickname) {
  return activeNicknames.find(nickname) == activeNicknames.end();
}

void IRCServer::registerNickname(const std::string& nickname,
                                 ClientHandler* handler) {
  activeNicknames[nickname] = handler;
}

void IRCServer::unregisterNickname(const std::string& nickname) {
  activeNicknames.erase(nickname);
}

ClientHandler* IRCServer::findClientHandlerByNickname(
    const std::string& nickname) {
  // Find the handler for the given nickname
  std::map<std::string, ClientHandler*>::iterator it =
      activeNicknames.find(nickname);

  // If the handler exists, return it
  if (it != activeNicknames.end()) {
    return it->second;
  }

  // If not found, return NULL
  return NULL;
}

void IRCServer::createChannel(const std::string& name) {
  // If the channel doesn't exist, create a new one
  if (channels.find(name) == channels.end()) {
    channels[name] = new Channel(name);
  }
}

Channel* IRCServer::findChannel(const std::string& name) {
  std::map<std::string, Channel*>::iterator it = channels.find(name);

  // If the channel exists, return it
  return it != channels.end() ? it->second : NULL;
}

const std::string IRCServer::getPassword() const { return password; }

void IRCServer::handleSigtstp(int signum) {
  (void)signum;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);  // Restore terminal settings
  signal(SIGTSTP, SIG_DFL);
  raise(SIGTSTP);
}

void IRCServer::handleSigcont(int signum) {
  (void)signum;
  tcgetattr(STDIN_FILENO, &orig_termios);  // Save terminal settings again
}
