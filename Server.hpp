#ifndef SERVER_H
#define SERVER_H

#include <netinet/in.h>

class Server {
public:
    Server(int port);
    ~Server();
    int initializeServerSocket();
    int acceptClient(struct sockaddr_in &address);

private:
    int server_fd;
    int port;
    struct sockaddr_in address;
};

#endif // SERVER_H
