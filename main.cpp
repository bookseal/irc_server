#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <iomanip>
#include <string>
#include <sstream>

void printHexRepresentation(const char* buffer, int length) {
    for (int i = 0; i < length; ++i) {
        if (isprint(static_cast<unsigned char>(buffer[i]))) {
            std::cout << buffer[i];
        } else {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(static_cast<unsigned char>(buffer[i]))
                      << std::dec << " ";
        }
    }
}

int initializeServerSocket(struct sockaddr_in &address) {
    int server_fd; // Socket file descriptor that will be used for the server
    int opt = 1;  // An integer flag to specify socket options; here it's set to true

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;         // Set address family to IPv4
    address.sin_addr.s_addr = INADDR_ANY; // Listen on any network interface
    address.sin_port = htons(6667);       // Set the port number to 6667, converting it to network byte order

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    return server_fd;
}

int acceptClient(int server_fd, struct sockaddr_in &address) {
    socklen_t addrlen = sizeof(address);

    int new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
    if (new_socket < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }
    return new_socket;
}

void echoMessages(int socket) {
    char buffer[1024] = {0}; // Buffer to store the received messages
    std::cout << "Welcome message sent\n"; // Log the sending of the welcome message

    while (true) {
        int read_val = read(socket, buffer, sizeof(buffer) - 1);
        if (read_val > 0) {
            buffer[read_val] = '\0'; // Null-terminate the string after reading
            std::cout << "Received: [";
            printHexRepresentation(buffer, read_val);
            std::cout << "]" << std::endl;
            send(socket, buffer, read_val, 0);
        } else if (read_val == 0) {
             std::cout << "Client disconnected." << std::endl;
            break; // Exit the loop if the client has disconnected
        } else {
            perror("read failed"); // Log the error
            break; // Exit the loop if an error has occurred
        }
    }
}

void handleIRCRegistration(int socket) {
    char buffer[1024] = {0};
    std::string nickname;
    std::string username;

    std::cout << "Waiting for client registration\n";
    // Read and parse NICK and USER commands
    while (nickname.empty() || username.empty()) {
        int read_val = read(socket, buffer, sizeof(buffer) - 1);
        std::cout << "Received: [";
        printHexRepresentation(buffer, read_val);
        std::cout << "]" << std::endl;
        if (read_val > 0) {
            buffer[read_val] = '\0'; // Null-terminate the string

            std::istringstream iss(buffer);
            std::string command;
            iss >> command;

            if (command == "NICK") {
                iss >> nickname; // Extract the nickname
                nickname.erase(0, 1); // Remove the leading ':'
            } else if (command == "USER") {
                iss >> username >> std::ws; // Extract the username
            }

            memset(buffer, 0, sizeof(buffer));
        } else if (read_val <= 0) {
            close(socket);
            throw std::runtime_error("Client disconnected or read failed during registration.");
        }
    }
    std::cout << "Nickname: " << nickname << ", Username: " << username << std::endl;
    std::string welcome = ":server 001 " + nickname + " :Welcome to the IRC network, " + nickname + "!\r\n";
    send(socket, welcome.c_str(), welcome.size(), 0);
}

int main() {
    struct sockaddr_in address;
    int server_fd = initializeServerSocket(address);
    int new_socket = acceptClient(server_fd, address);

    std::cout << "Client connected\n";
    try {
        handleIRCRegistration(new_socket);
        std::cout << "Registration complete\n";
        echoMessages(new_socket);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    close(new_socket);
    close(server_fd);
    return 0;
}
