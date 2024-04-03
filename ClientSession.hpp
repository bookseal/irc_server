#ifndef CLIENTSESSION_H
#define CLIENTSESSION_H

#include <string>

class ClientSession {
public:
    ClientSession(int clientSocket);
    ~ClientSession();

    void handleIRCRegistration();
    void echoMessages();

private:
    int socket;
    std::string nickname;
    std::string username;

    void printHexRepresentation(const char* buffer, int length);
};

#endif // CLIENTSESSION_H
