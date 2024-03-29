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

// Initializes the server socket and starts listening for connections
// Parameters:
// - address: A reference to a sockaddr_in structure where the server's address details will be stored
// Returns:
// - The file descriptor for the server socket
int initializeServerSocket(struct sockaddr_in &address) {
    int server_fd; // Socket file descriptor that will be used for the server
    int opt = 1;  // An integer flag to specify socket options; here it's set to true

    // Attempt to create a socket for the IPv4 protocol and TCP communication
    // AF_INET: Address family for IPv4
    // SOCK_STREAM: Type of socket for TCP (stream-based communication)
    // 0: Default protocol (TCP in this case)
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        // If socket creation failed, print an error message and exit
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set options on the socket to allow reuse of local addresses (if in TIME_WAIT state)
    // server_fd: The socket file descriptor
    // SOL_SOCKET: The level at which the option is defined (socket level)
    // SO_REUSEADDR: The name of the option; allows reuse of local addresses
    // &opt: A pointer to the option value
    // sizeof(opt): The size of the option value
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        // If setting the socket option failed, print an error message and exit
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Configure the server address structure
    address.sin_family = AF_INET;         // Set address family to IPv4
    address.sin_addr.s_addr = INADDR_ANY; // Listen on any network interface
    address.sin_port = htons(6667);       // Set the port number to 6667, converting it to network byte order

    // Bind the socket to the address and port we've configured above
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        // If binding failed, print an error message and exit
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Start listening on the socket for incoming connections, with a maximum of 3 pending connections
    if (listen(server_fd, 3) < 0) {
        // If listening failed, print an error message and exit
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // Return the server socket file descriptor for further use
    return server_fd;
}

// Accepts a client trying to connect to the server
// Parameters:
// - server_fd: The file descriptor of the server socket that's listening for connections
// - address: A reference to a sockaddr_in structure filled with the server's address details
// Returns:
// - The file descriptor for the new socket that's connected to the client
int acceptClient(int server_fd, struct sockaddr_in &address) {
    // The length of the address is initialized for the accept call
    socklen_t addrlen = sizeof(address);

    // Wait for a client to connect and accept the connection
    // server_fd: The server's socket file descriptor, listening for connections
    // (struct sockaddr *)&address: A pointer to the sockaddr_in structure cast to sockaddr
    // &addrlen: A pointer to the length of the address, which accept will fill
    int new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);

    // Check if the accept call was successful
    if (new_socket < 0) {
        // If accept failed, print an error message and exit
        perror("accept");
        exit(EXIT_FAILURE);
    }

    // Return the file descriptor of the new socket connected to the client
    return new_socket;
}

// Handles the echoing of messages from a connected client
// Parameters:
// - socket: The file descriptor for the socket connected to the client
void echoMessages(int socket) {
    char buffer[1024] = {0}; // Buffer to store the received messages
    std::cout << "Welcome message sent\n"; // Log the sending of the welcome message

    // Loop indefinitely to read messages from the client and echo them back
    while (true) {
        // Attempt to read data from the socket into the buffer
        int read_val = read(socket, buffer, sizeof(buffer) - 1);
        if (read_val > 0) {
            buffer[read_val] = '\0'; // Null-terminate the string after reading

            // Log the received message with a hex representation for any non-printable characters
            std::cout << "Received: [";
            printHexRepresentation(buffer, read_val);
            std::cout << "]" << std::endl;

            // Echo the received message back to the client
            send(socket, buffer, read_val, 0);
        } else if (read_val == 0) {
            // If read returns 0, it means the client has disconnected
            std::cout << "Client disconnected." << std::endl;
            break; // Exit the loop if the client has disconnected
        } else {
            // If read returns a negative value, an error occurred
            perror("read failed"); // Log the error
            break; // Exit the loop if an error has occurred
        }
    }
}

// Function to handle the initial IRC handshake
void handleIRCRegistration(int socket) {
    char buffer[1024] = {0};
    std::string nickname;
    std::string username;

    // Read and parse NICK and USER commands
    while (nickname.empty() || username.empty()) {
        int read_val = read(socket, buffer, sizeof(buffer) - 1);
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

            // Clear the buffer for the next read
            memset(buffer, 0, sizeof(buffer));
        } else if (read_val <= 0) {
            // On error or disconnect, exit
            close(socket);
            throw std::runtime_error("Client disconnected or read failed during registration.");
        }
    }

    // Send a welcome message after successful registration
    std::string welcome = ":server 001 " + nickname + " :Welcome to the IRC network, " + nickname + "!\r\n";
    send(socket, welcome.c_str(), welcome.size(), 0);
}

int main() {
    struct sockaddr_in address;
    int server_fd = initializeServerSocket(address);
    int new_socket = acceptClient(server_fd, address);

    try {
        // First handle the IRC registration handshake
        handleIRCRegistration(new_socket);

        // After registration, start echoing messages
        echoMessages(new_socket);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    close(new_socket);
    close(server_fd);
    return 0;
}
