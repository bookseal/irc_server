#include "Server.hpp"
#include "ClientSession.hpp"
#include <netinet/in.h>
#include <thread>

void handleClient(int clientSocket) {
    ClientSession session(clientSocket);
    session.handleIRCRegistration();
    session.echoMessages();
}

int main() {
    Server server(6667); // Initialize server on port 6667
    server.initializeServerSocket();

    while (true) {
        struct sockaddr_in clientAddress;
        int clientSocket = server.acceptClient(clientAddress);
        std::thread clientThread(handleClient, clientSocket);
        clientThread.detach(); // Detach the thread to allow independent execution
    }

    return 0;
}
