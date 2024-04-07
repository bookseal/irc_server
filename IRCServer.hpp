#ifndef IRCSERVER_HPP
#define IRCSERVER_HPP
#include "ServerSocket.hpp"
#include <iostream>
#include <iomanip>
#include <unistd.h> // For close function
#include <cstring>  // For memset
#include <sstream>  // For istringstream
#include <pthread.h>
#include <vector>
#include <poll.h>
#include <queue>


class IRCServer {
public:
    IRCServer(int port) : serverSocket(port) {};
    void run();
    void initializeServerSocketAndPollFD();
    void waitForConnectionsAndHandleData();
    void acceptNewClients();
    void handleClientEvents();
    void handleClientData(int clientSocket);
    void handleClientWrite(int clientSocket);
    size_t findClientSocketIndex(int clientSocket);

//    void printHexRepresentation(const char* buffer, int length);
//    void echoMessages(int socket);
private:
    ServerSocket serverSocket;
    std::vector<struct pollfd> fds;
    std::vector<std::queue<std::string> > messageQueues;
};

void IRCServer::run() {
    initializeServerSocketAndPollFD();
    waitForConnectionsAndHandleData();
}

void IRCServer::initializeServerSocketAndPollFD() {
    if (serverSocket.initialize() != 0) {
        std::cerr << "Failed to initialize server socket." << std::endl;
        return;
    }

    std::cout << "Server initialized. Waiting for client connections..." << std::endl;

    struct pollfd server_fd;
    server_fd.fd = serverSocket.getFD();
    server_fd.events = POLLIN;
    server_fd.revents = 0;
    fds.push_back(server_fd);
}

void IRCServer::waitForConnectionsAndHandleData() {
    while (true) {
        if (poll(fds.data(), fds.size(), -1) < 0) {
            perror("poll");
            return;
        }
        acceptNewClients();
        handleClientEvents();
    }
}

void IRCServer::acceptNewClients() {
    if (fds[0].revents & POLLIN) {
        int clientSocket = serverSocket.acceptClient();
        if (clientSocket >= 0) {
            std::cout << "New client connected, socket fd: " << clientSocket << std::endl;

            // Add the client socket to the array of pollfd structures
            struct pollfd client_fd = {clientSocket, POLLIN | POLLOUT, 0};
            fds.push_back(client_fd);

            // Also, add a new, corresponding empty message queue for this client
            messageQueues.push_back(std::queue<std::string>());

            std::cout << "Client added. fds size: " << fds.size() << ", messageQueues size: " << messageQueues.size() << std::endl;
        } else {
            std::cerr << "Failed to accept client connection." << std::endl;
        }
    }
}


void IRCServer::handleClientEvents() {
    for (size_t i = 1; i < fds.size(); ++i) {
        // 안전한 인덱스 접근을 위한 검사 추가
        if (i-1 >= messageQueues.size()) {
            std::cerr << "Index out of range for messageQueues" << std::endl;
            continue; // 혹은 적절한 오류 처리
        }

        if (fds[i].revents & POLLIN) {
            handleClientData(fds[i].fd);
        }

        if ((fds[i].revents & POLLOUT) && !messageQueues[i-1].empty()) {
            handleClientWrite(fds[i].fd);
        }

        if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            close(fds[i].fd);
            fds.erase(fds.begin() + i);
            messageQueues.erase(messageQueues.begin() + i - 1);
            --i;
        }
    }
}

void IRCServer::handleClientData(int clientSocket) {
    char buffer[1024] = {0};
    ssize_t bytesRead = read(clientSocket, buffer, sizeof(buffer) - 1);

    if (bytesRead > 0) {
        std::cout << "Received data from client: " << buffer << std::endl;
        std::string response = "Some response\n";

        // findClientSocketIndex 함수는 클라이언트의 fd와 일치하는 fds 벡터의 인덱스를 반환합니다.
        // fds[0]은 서버 소켓을 나타내므로, 클라이언트는 fds[1]부터 시작합니다.
        size_t index = findClientSocketIndex(clientSocket);
        if (index != std::string::npos) {
            // messageQueues의 인덱스는 fds의 인덱스와 1 차이납니다. (fds[0]은 서버 소켓)
            messageQueues[index].push(response);
            // fds 배열 내 해당 클라이언트의 이벤트를 POLLOUT으로 설정합니다.
            fds[index + 1].events |= POLLOUT;
        }
    } else if (bytesRead == 0) {
        std::cout << "Client disconnected." << std::endl;
        size_t index = findClientSocketIndex(clientSocket);
        if (index != std::string::npos) {
            close(clientSocket); // 클라이언트 소켓을 닫습니다.
            fds.erase(fds.begin() + index + 1); // fds 벡터에서 해당 소켓 정보를 제거합니다.
            messageQueues.erase(messageQueues.begin() + index); // 메시지 큐에서 해당 큐를 제거합니다.
        }
    } else {
        perror("Error reading from client socket");
        close(clientSocket);
    }
}

void IRCServer::handleClientWrite(int clientSocket) {
    // 클라이언트 소켓에 해당하는 메시지 큐 인덱스 찾기
    size_t index = findClientSocketIndex(clientSocket); // 이전 예제 코드에서 설명한 대로 구현

    // 메시지 큐가 비어있지 않다면 메시지 전송 시도
    if (index != std::string::npos && !messageQueues[index].empty()) {
        const std::string& message = messageQueues[index].front();
        ssize_t bytesWritten = write(clientSocket, message.c_str(), message.length());

        // 성공적으로 메시지를 전송한 경우
        if (bytesWritten > 0) {
            std::cout << "Sent message to client." << std::endl;
            messageQueues[index].pop(); // 메시지 전송 후 큐에서 메시지 제거
        } else {
            // 에러 처리: 클라이언트 소켓 닫기
            perror("Error writing to client socket");
            close(clientSocket);
            fds.erase(fds.begin() + index + 1); // fds 배열에서 클라이언트 제거
            messageQueues.erase(messageQueues.begin() + index); // messageQueues에서 큐 제거
            return; // 추가 작업 중지
        }
    }

    // 메시지 큐가 비었으면, 더 이상 쓸 것이 없으므로 POLLOUT 등록 해제
    if (messageQueues[index].empty()) {
        fds[index + 1].events = POLLIN; // 다시 읽기만 감시
    }
}

size_t IRCServer::findClientSocketIndex(int clientSocket) {
    for (size_t i = 1; i < fds.size(); ++i) {
        if (fds[i].fd == clientSocket) {
            return i - 1; // 실제 메시지 큐 인덱스를 반환하기 위해 -1
        }
    }
    return std::string::npos; // 일치하는 소켓을 찾지 못한 경우
}

//
//void IRCServer::printHexRepresentation(const char *buffer, int length) {
//    for (int i = 0; i < length; ++i) {
//        if (isprint(static_cast<unsigned char>(buffer[i]))) {
//            std::cout << buffer[i];
//        } else {
//            std::cout << std::hex << std::setw(2) << std::setfill('0')
//                      << static_cast<int>(static_cast<unsigned char>(buffer[i]))
//                      << std::dec << " ";
//        }
//    }
//}
//
//void IRCServer::echoMessages(int socket) {
//    char buffer[1024] = {0};
//    std::cout << "Welcome message sent\n";
//
//    while (true) {
//        int read_val = read(socket, buffer, sizeof(buffer) - 1);
//        if (read_val > 0) {
//            buffer[read_val] = '\0';
//            std::cout << "Received: [";
//            printHexRepresentation(buffer, read_val);
//            std::cout << "]" << std::endl;
//            send(socket, buffer, read_val, 0);
//        } else if (read_val == 0) {
//            std::cout << "Client disconnected." << std::endl;
//            break;
//        } else {
//            perror("read failed");
//            break;
//        }
//    }
//}

#endif