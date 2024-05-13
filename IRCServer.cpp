#include "IRCServer.hpp"
#include "ClientHandler.hpp"
#include "Channel.hpp"

void IRCServer::sendMessageToUser(const std::string& senderNickname, const std::string& recipientNickname, const std::string& message) {
    std::cout << "senderNickname   : " << senderNickname << std::endl;
    std::cout << "recipientNickname: " << recipientNickname << std::endl;
    std::cout << "message          : " << message << "\n";
    std::string newMessage = ":" + senderNickname + "!user@host PRIVMSG " + recipientNickname + " :" + message;
    if (activeNicknames.count(recipientNickname) > 0) {
        ClientHandler* recipientHandler = activeNicknames[recipientNickname];
        if (recipientHandler) {
            recipientHandler->sendMessage(newMessage);
        } else {
            std::cerr << "Handler for nickname '" << recipientNickname << "' not found." << std::endl;
            ClientHandler* senderHandler = activeNicknames[senderNickname];
            if (senderHandler) {
                senderHandler->sendMessage(":Server ERROR :Internal error, recipient handler not found.\r\n");
            }
        }
    } else {
        std::cerr << "No user with nickname '" << recipientNickname << "' found." << std::endl;
        ClientHandler* senderHandler = activeNicknames[senderNickname];
        if (senderHandler) {
            senderHandler->sendMessage(":Server ERROR :No such nick/channel.\r\n");
        }
    }
}

IRCServer::IRCServer(int port) : port(port), serverSocket(-1) {}

IRCServer::~IRCServer() {
    close(serverSocket);
    for (std::map<int, ClientHandler*>::iterator it = clientHandlers.begin(); it != clientHandlers.end(); ++it) {
        delete it->second;
    }
}

bool IRCServer::initializeServerSocket() {
    // 소켓을 생성합니다. (우체국에서 새로운 우편함을 설치) 
    // 소켓: 네트워크 통신을 위해 사용되는 특별한 형태의 fd. 데이터를 주고 받을 수 있는 통로.
    // AF_INET은 IPv4 인터넷 프로토콜을 사용함을 의미하고, SOCK_STREAM은 '연결 지향형 TCP 소켓'임을 의미합니다.
    // "연결 지향형" 이라는 말은 통신을 시작하기 전에 두 장치 간에 먼저 연결을 설정해야 함을 의미합니다
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Failed to create socket." << std::endl;
        return false;
    }

    // 소켓 옵션 설정: SO_REUSEADDR은 소켓을 재사용할 수 있도록 설정합니다.
    // '빠른 재사용 주차 공간'과 비슷
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Error setting socket options." << std::endl;
        return false;
    }

    // 서버 주소 구조체를 초기화합니다. (사무실 건물의 리셉션 데스크 설정에 비유됨)
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr)); // 메모리 영역을 0으로 초기화
    serverAddr.sin_family = AF_INET; // 주소 체계를 IPv4로 설정
    serverAddr.sin_addr.s_addr = INADDR_ANY; // 모든 인터페이스의 IP 주소에서 수신하도록 설정 (누구나 접근할 수 있도록 공공 장소에 설치되는 것)
    serverAddr.sin_port = htons(port); // 사용할 포트 번호 설정. htons는 호스트 바이트 순서를 네트워크 바이트 순서로 변환합니다.

    // 생성한 소켓을 위에서 설정한 주소에 바인드합니다. (우편함에 주소를 부여함)
    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Failed to bind to port " << port << "." << std::endl;
        return false;
    }

    // 리슨: 서버가 포트를 통해 들어오는 연결을 기다림
    // 소켓을 듣기 모드로 설정합니다. 최대 10개의 연결 요청을 대기 큐에 넣을 수 있습니다.
    if (listen(serverSocket, 10) < 0) { // 리슨: 우체국 직원이 우편함에서 편지가 들어오는 것을 기다리는 것
        std::cerr << "Failed to listen on socket." << std::endl;
        return false;
    }

    // 소켓을 파일 디스크립터 배열에 추가합니다.
    // POLLIN은 입력 데이터가 가능할 때 발생하는 이벤트를 나타냅니다.
    // 이 과정을 경비원이 여러 감시 카메라를 동시에 모니터링하는 상황에 비유할 수 있습니다:
    struct pollfd listenFD;
    listenFD.fd = serverSocket; // fd: 감시할 파일 디스크립터를 지정. serverSocket, 즉 클라이언트의 연결 요청을 기다리는 소켓입니다.
    listenFD.events = POLLIN; // 감시할 이벤트의 유형을 지정. POLLIN은 수신할 데이터가 있을 때, 즉 클라이언트로부터 연결 요청이 들어올 때 발생하는 이벤트를 나타냅니다.
    fds.push_back(listenFD); // 이 구조체를 fds 벡터에 추가
    /*
    이 과정을 경비원이 여러 감시 카메라를 동시에 모니터링하는 상황에 비유할 수 있습니다:
    경비원은 poll() 함수에 해당하고, 여러 감시 지점(카메라)을 동시에 주시합니다.
    감시 카메라는 여러 pollfd 구조체에 해당하며, 각각의 카메라는 특정 장소(여기서는 serverSocket)를 감시합니다.

    **감시할 이벤트(POLLIN)**는 카메라가 움직임을 감지했을 때 경비원에게 알려주는 것과 유사하며, 
    서버 소켓에 연결 요청이 들어오는 것을 감지하는 것에 해당합니다.
    */

    return true;
}


void IRCServer::run() {
    if (!initializeServerSocket()) { // 서버 소켓을 설정
        std::cerr << "Server initialization failed." << std::endl;
        return;
    }
    std::cout << "Server running on port " << port << std::endl;

    // 폴: 상점에서 여러 창구가 있고, 
    // 각 창구에서 고객의 요청(구매, 반품 등)을 처리할 준비가 되어 있는지 상점 직원이 확인하는 것과 비슷합니다.
    // 창구에서 고객 요청이 준비되었는지 확인하는 상점 직원의 역할을 합니다.
    while (true) {
        // 파일 디스크립터들에 대한 poll 함수 호출하여 이벤트가 있는지 체크
        // poll: 모든 파일 디스크립터(상점의 창구)를 검사합니다. 
        // 창구에서 고객 요청이 준비되었는지 확인하는 상점 직원의 역할을 합니다.
        int pollCount = poll(&fds[0], fds.size(), -1);
        if (pollCount < 0) {
            std::cerr << "Poll error." << std::endl;
            break;
        }

        // 모든 파일 디스크립터들을 순회 
        for (size_t i = 0; i < fds.size(); i++) {
            // 각 파일 디스크립터에 대해 입력 가능한 이벤트(POLLIN)가 있는지 검사
            if (fds[i].revents & POLLIN) { // POLLIN: 전화기가 울림
                // 손님의 예약 요청이 있으면 새 클라이언트 연결 수락
                if (fds[i].fd == serverSocket) { // serverSocket: 손님의 예약 요청을 받아들이는 전용 전화
                    acceptNewClient();
                } else { // 기존 손님의 전화가 울린 경우
                    // 해당 소켓을 관리하는 클라이언트 핸들러를 찾음
                    ClientHandler* handler = clientHandlers[fds[i].fd]; // 어느 손님인지 찾기
                    if (handler) { // 핸들러: 서버와 클라이언트 간의 모든 통신을 중개하고 관리하는 중요한 구성 요소
                        // 클라이언트 핸들러가 존재하면 입력 처리 메소드 호출
                        handler->processInput();
                    }
                }
            }
        }
        cleanUpInactiveHandlers();  // 더 이상 활성 상태가 아닌 핸들러들을 정리
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

void IRCServer::registerNickname(const std::string& nickname, ClientHandler* handler) {
    activeNicknames[nickname] = handler;
}

void IRCServer::unregisterNickname(const std::string& nickname) {
    activeNicknames.erase(nickname);
}

ClientHandler* IRCServer::findClientHandlerByNickname(const std::string& nickname) {
    std::map<std::string, ClientHandler*>::iterator it = activeNicknames.find(nickname);
    if (it != activeNicknames.end()) {
        return it->second;
    }
    return NULL;
}

void IRCServer::createChannel(const std::string& name) {
    if (channels.find(name) == channels.end()) {
        channels[name] = new Channel(name);
    }
}

void IRCServer::deleteChannel(const std::string& name) {
    std::map<std::string, Channel*>::iterator it = channels.find(name);
    if (it != channels.end() && it->second->isEmpty()) {
        delete it->second;  // Delete the channel object
        channels.erase(it); // Remove the entry from the map
    }
}

Channel* IRCServer::findChannel(const std::string& name) {
    std::map<std::string, Channel*>::iterator it = channels.find(name);
    return it != channels.end() ? it->second : NULL;
}
