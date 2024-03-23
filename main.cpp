#include <iostream>
#include <string>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

const int MAX_CONNECTIONS = 5;
const int BUFFER_SIZE = 512;

int createSocket() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cerr << "Cannot open socket\n";
        exit(EXIT_FAILURE);
    }
    return sockfd;
}

void initializeServerAddr(struct sockaddr_in &server_addr) {
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(6667); // Default IRC port
}

void bindSocket(int server_socket, struct sockaddr_in &server_addr) {
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Cannot bind\n";
        exit(EXIT_FAILURE);
    }
}

void listenForConnections(int server_socket) {
    if (listen(server_socket, MAX_CONNECTIONS) < 0) {
        std::cerr << "Listening failed\n";
        exit(EXIT_FAILURE);
    }
    std::cout << "Server listening on port 6667...\n";
}

int acceptConnection(int server_socket) {
    int client_socket = accept(server_socket, NULL, NULL);
    if (client_socket < 0) {
        std::cerr << "Cannot accept connection\n";
        exit(EXIT_FAILURE);
    }
    std::cout << "Client connected\n";
    return client_socket;
}

void processClientRequests(int client_socket) {
    char buffer[BUFFER_SIZE];
    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t n = read(client_socket, buffer, BUFFER_SIZE - 1);
        if (n <= 0) {
            if (n == 0)
                std::cout << "Client disconnected\n";
            else
                std::cerr << "Cannot read from socket\n";
            break;
        }
        buffer[n] = '\0';
        std::cout << "Received: " << buffer << std::endl;

        if (strncmp(buffer, "NICK", 4) == 0 || strncmp(buffer, "USER", 4) == 0) {
            const char *welcomeMsg = ":server 001 Welcome to the IRC server\r\n";
            write(client_socket, welcomeMsg, strlen(welcomeMsg));
        } else if (strncmp(buffer, "PING", 4) == 0) {
            write(client_socket, "PONG :pong\r\n", 12);
        } else {
            write(client_socket, "ERROR :Unknown command\r\n", 24);
        }
    }
}

int main() {
    int server_socket = createSocket();

    struct sockaddr_in server_addr;
    initializeServerAddr(server_addr);

    bindSocket(server_socket, server_addr);

    listenForConnections(server_socket);

    int client_socket = acceptConnection(server_socket);

    processClientRequests(client_socket);

    close(client_socket);
    close(server_socket);

    return 0;
}
