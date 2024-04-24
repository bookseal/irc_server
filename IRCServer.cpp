#include "IRCServer.hpp"
#include "ClientHandler.hpp"

IRCServer::IRCServer(int port) : port(port), serverSocket(-1) {}

IRCServer::~IRCServer() {
    close(serverSocket);
    for (std::map<int, ClientHandler*>::iterator it = clientHandlers.begin(); it != clientHandlers.end(); ++it) {
        delete it->second;
    }
}

bool IRCServer::initializeServerSocket() {
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Failed to create socket." << std::endl;
        return false;
    }

    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Error setting socket options." << std::endl;
        return false;
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Failed to bind to port " << port << "." << std::endl;
        return false;
    }

    if (listen(serverSocket, 10) < 0) {
        std::cerr << "Failed to listen on socket." << std::endl;
        return false;
    }

    struct pollfd listenFD;
    listenFD.fd = serverSocket;
    listenFD.events = POLLIN;
    fds.push_back(listenFD);

    return true;
}

void IRCServer::run() {
    if (!initializeServerSocket()) {
        std::cerr << "Server initialization failed." << std::endl;
        return;
    }

    std::cout << "Server running on port " << port << std::endl;

    while (true) {
        int pollCount = poll(&fds[0], fds.size(), -1);
        if (pollCount < 0) {
            std::cerr << "Poll error." << std::endl;
            break;
        }

        for (size_t i = 0; i < fds.size(); i++) {
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == serverSocket) {
                    acceptNewClient();
                } else {
                    ClientHandler* handler = clientHandlers[fds[i].fd];
                    if (handler) {
                        handler->processInput();
                    }
                }
            }
        }

        cleanUpInactiveHandlers();  // Clean up any handlers that are no longer active
    }
}

void IRCServer::acceptNewClient() {
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    int clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientAddrLen);
    if (clientSocket < 0) {
        std::cerr << "Error accepting new connection." << std::endl;
        return;
    }

    ClientHandler* newHandler = new ClientHandler(clientSocket, this);
    clientHandlers[clientSocket] = newHandler;

    struct pollfd clientFD;
    clientFD.fd = clientSocket;
    clientFD.events = POLLIN;
    fds.push_back(clientFD);

    std::cout << "New client connected: " << clientSocket << std::endl;
}

void IRCServer::cleanUpInactiveHandlers() {
    std::map<int, ClientHandler*>::iterator it = clientHandlers.begin();
    while (it != clientHandlers.end()) {
        if (!it->second->isActive()) {
            std::cout << "Cleaning up client handler for socket: " << it->first << std::endl;
            close(it->first); // Close the socket
            delete it->second; // Delete the handler
            it = clientHandlers.erase(it); // Remove from the map and advance the iterator
        } else {
            ++it;
        }
    }
}

bool IRCServer::isNicknameAvailable(const std::string& nickname) {
    return activeNicknames.find(nickname) == activeNicknames.end();
}
bool IRCServer::isUsernameAvailable(const std::string& username) {
    return activeUsernames.find(username) == activeUsernames.end();
}
void IRCServer::registerNickname(const std::string& nickname, ClientHandler* handler) {
    activeNicknames[nickname] = handler;
}
void IRCServer::registerUsername(const std::string& username, ClientHandler* handler) {
    activeUsernames[username] = handler;
}
void IRCServer::unregisterNickname(const std::string& nickname) {
    activeNicknames.erase(nickname);
}
void IRCServer::unregisterUsername(const std::string& username) {
    activeUsernames.erase(username);
}