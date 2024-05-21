#include "IRCServer.hpp"

#include "Channel.hpp"
#include "ClientHandler.hpp"
struct termios IRCServer::orig_termios;

// IRC서버에서 특정 사용자에게 메세지 전송
void IRCServer::sendMessageToUser(const std::string& senderNickname,
                                  const std::string& recipientNickname,
                                  const std::string& message) {
  std::cout << "senderNickname   : " << senderNickname
            << std::endl;  // 보내는 사람
  std::cout << "recipientNickname: " << recipientNickname
            << std::endl;  // 받는 사람
  std::cout << "message          : " << message << "\n";

  // IRC 프로토콜 형식에 맞춰 메시지를 구성
  std::string newMessage = ":" + senderNickname + "!user@host PRIVMSG " +
                           recipientNickname + " :" + message;

  if (activeNicknames.count(recipientNickname) >
      0) {  // 수신인의 이름이 목록에 있는지 확인하기
    ClientHandler* recipientHandler = activeNicknames[recipientNickname];
    if (recipientHandler) {  // 수신인 찾으면 메시지를 전달함
      recipientHandler->sendMessage(newMessage);
    } else {  // 배달원이 수신인 집에 도착했지만 수신인을 찾을 수 없는 경우
      std::cerr << "Handler for nickname '" << recipientNickname
                << "' not found." << std::endl;
      ClientHandler* senderHandler = activeNicknames[senderNickname];
      if (senderHandler) {
        senderHandler->sendMessage(
            ":Server ERROR :Internal error, recipient handler not found.\r\n");
      }
    }
  } else {  // 수신자의 닉네임이 목록에 없는 경우
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
  tcgetattr(STDIN_FILENO, &orig_termios);
  signal(SIGTSTP, handleSigtstp);
  signal(SIGCONT, handleSigcont);
}

IRCServer::~IRCServer() {
  close(serverSocket);
  for (std::map<int, ClientHandler*>::iterator it = clientHandlers.begin();
       it != clientHandlers.end(); ++it) {
    delete it->second;
  }
  for (std::map<std::string, Channel*>::iterator it = channels.begin();
       it != channels.end(); ++it) {
    delete it->second;
  }
}

bool IRCServer::initializeServerSocket() {
  // 소켓을 생성합니다. (우체국에서 새로운 우편함을 설치)
  // 소켓: 네트워크 통신을 위해 사용되는 특별한 형태의 fd. 데이터를 주고 받을 수
  // 있는 통로. AF_INET은 IPv4 인터넷 프로토콜을 사용함을 의미하고,
  // SOCK_STREAM은 '연결 지향형 TCP 소켓'임을 의미합니다. "연결 지향형" 이라는
  // 말은 통신을 시작하기 전에 두 장치 간에 먼저 연결을 설정해야 함을 의미합니다
  serverSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (serverSocket < 0) {
    std::cerr << "Failed to create socket." << std::endl;
    return false;
  }

  // 소켓 옵션 설정: SO_REUSEADDR은 소켓을 재사용할 수 있도록 설정합니다.
  // '빠른 재사용 주차 공간'과 비슷
  int opt = 1;
  if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) <
      0) {
    std::cerr << "Error setting socket options." << std::endl;
    return false;
  }

  // 서버 주소 구조체를 초기화합니다. (사무실 건물의 리셉션 데스크 설정에
  // 비유됨)
  struct sockaddr_in serverAddr;
  memset(&serverAddr, 0, sizeof(serverAddr));  // 메모리 영역을 0으로 초기화
  serverAddr.sin_family = AF_INET;  // 주소 체계를 IPv4로 설정
  serverAddr.sin_addr.s_addr =
      INADDR_ANY;  // 모든 인터페이스의 IP 주소에서 수신하도록 설정 (누구나
                   // 접근할 수 있도록 공공 장소에 설치되는 것)
  serverAddr.sin_port =
      htons(port);  // 사용할 포트 번호 설정. htons는 호스트 바이트 순서를
                    // 네트워크 바이트 순서로 변환합니다.

  // 생성한 소켓을 위에서 설정한 주소에 바인드합니다. (우편함에 주소를 부여함)
  if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) <
      0) {
    std::cerr << "Failed to bind to port " << port << "." << std::endl;
    return false;
  }

  // 리슨: 서버가 포트를 통해 들어오는 연결을 기다림
  // 소켓을 듣기 모드로 설정합니다. 최대 10개의 연결 요청을 대기 큐에 넣을 수
  // 있습니다.
  if (listen(serverSocket, 10) <
      0) {  // 리슨: 우체국 직원이 우편함에서 편지가 들어오는 것을 기다리는 것
    std::cerr << "Failed to listen on socket." << std::endl;
    return false;
  }

  // 소켓을 파일 디스크립터 배열에 추가합니다.
  // POLLIN은 입력 데이터가 가능할 때 발생하는 이벤트를 나타냅니다.
  // 이 과정을 경비원이 여러 감시 카메라를 동시에 모니터링하는 상황에 비유할 수
  // 있습니다:
  struct pollfd listenFD;
  listenFD.fd =
      serverSocket;  // fd: 감시할 파일 디스크립터를 지정. serverSocket, 즉
                     // 클라이언트의 연결 요청을 기다리는 소켓입니다.
  listenFD.events = POLLIN;  // 감시할 이벤트의 유형을 지정. POLLIN은 수신할
                             // 데이터가 있을 때, 즉 클라이언트로부터 연결
                             // 요청이 들어올 때 발생하는 이벤트를 나타냅니다.
  fds.push_back(listenFD);  // 이 구조체를 fds 벡터에 추가
  /*
  이 과정을 경비원이 여러 감시 카메라를 동시에 모니터링하는 상황에 비유할 수
  있습니다: 경비원은 poll() 함수에 해당하고, 여러 감시 지점(카메라)을 동시에
  주시합니다. 감시 카메라는 여러 pollfd 구조체에 해당하며, 각각의 카메라는 특정
  장소(여기서는 serverSocket)를 감시합니다.

  **감시할 이벤트(POLLIN)**는 카메라가 움직임을 감지했을 때 경비원에게 알려주는
  것과 유사하며, 서버 소켓에 연결 요청이 들어오는 것을 감지하는 것에 해당합니다.
  */

  return true;
}

void IRCServer::run() {
  if (!initializeServerSocket()) {  // 서버 소켓을 설정
    std::cerr << "Server initialization failed." << std::endl;
    return;
  }
  std::cout << "Server running on port " << port << std::endl;

  // 폴: 상점에서 여러 창구가 있고,
  // 각 창구에서 고객의 요청(구매, 반품 등)을 처리할 준비가 되어 있는지 상점
  // 직원이 확인하는 것과 비슷합니다. 창구에서 고객 요청이 준비되었는지 확인하는
  // 상점 직원의 역할을 합니다.
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
      if (fds[i].revents & POLLIN) {  // POLLIN: 전화기가 울림
        // 손님의 예약 요청이 있으면 새 클라이언트 연결 수락
        if (fds[i].fd == serverSocket) {  // serverSocket: 손님의 예약 요청을
                                          // 받아들이는 전용 전화
          acceptNewClient();
        } else {  // 기존 손님의 전화가 울린 경우
          // 해당 소켓을 관리하는 클라이언트 핸들러를 찾음
          ClientHandler* handler =
              clientHandlers[fds[i].fd];  // 어느 손님인지 찾기
          if (handler) {  // 핸들러: 서버와 클라이언트 간의 모든 통신을 중개하고
                          // 관리하는 중요한 구성 요소
            // 클라이언트 핸들러가 존재하면 입력 처리 메소드 호출
            handler->processInput();
          }
        }
      }
    }
    cleanUpInactiveHandlers();  // 더 이상 활성 상태가 아닌 핸들러들을 정리
  }
}

// 새 손님(클라이언트) 도착해서 받아들이는 과정
void IRCServer::acceptNewClient() {
  // 클라이언트의 주소 정보를 저장할 구조채 선언하기.
  struct sockaddr_in clientAddr;  // 클라이언트의 기본 정보(주소)받기
  socklen_t clientAddrLen = sizeof(clientAddr);

  // 손님 맞이: 서버 소켓을 통해 클라이언트의 연결 요청을 수락함.
  int clientSocket =
      accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
  if (clientSocket < 0) {  // 연결 수락 실패
    std::cerr << "Error accepting new connection." << std::endl;
    return;
  }

  // 새 손님을 테이블로 안내: 새 클라이언트를 처리할 '클라이언트 핸들러 객체'
  // 생성하기.
  ClientHandler* newHandler = new ClientHandler(clientSocket, this);
  clientHandlers[clientSocket] = newHandler;

  // 클라이언트 소켓을 감시할 폴fd를 설정하고 이벤트 목록에 추가함.
  struct pollfd
      clientFD;  // 손님 예약 카드 만들기: 손님이 어떤 서비스를 원하는지 기록함
  clientFD.fd = clientSocket;  // 손님의 위치 저장하기 (폴 함수가 감시할
                               // 클라이언트 소켓 지정)
  clientFD.events = POLLIN;  // 폴인: 데이터 수신 가능 이벤트 (직원은 손님이
                             // 주문 준비가 되었음을 알게됨)
  fds.push_back(clientFD);  // 완성된 예약 카드를 예약 대장에 추가함. 직원은
                            // 언제든 확인 가능함

  std::cout << "New client connected: " << clientSocket << std::endl;
}

void IRCServer::cleanUpInactiveHandlers() {
  std::map<int, ClientHandler*>::iterator it = clientHandlers.begin();
  while (it != clientHandlers.end()) {
    if (!it->second->isActive()) {
      std::cout << "Cleaning up client handler for socket: " << it->first
                << std::endl;
      close(it->first);   // Close the socket


      delete it->second;  // Delete the handler
      std::map<int, ClientHandler*>::iterator temp =
          it;  // 임시 이터레이터를 사용하여
      ++it;    // 이터레이터를 다음 항목으로 이동합니다
      clientHandlers.erase(temp);  // 맵에서 해당 항목을 제거합니다
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
  // activeNicknames 맵에서 닉네임을 키로 사용해 핸들러 객체 찾기
  std::map<std::string, ClientHandler*>::iterator it =
      activeNicknames.find(nickname);

  // 해당 닉네임의 핸들러 객체가 존재한면(찾은 이터레이터가 맵의 끝이 아니라면)
  if (it != activeNicknames.end()) {
    return it->second;  // 해당 핸들러 객체의 포인터 리턴하기
  }

  // 닉네임에 해당하는 핸들러 객체를 찾지 못했다면 NULL을 반환
  return NULL;
}

void IRCServer::createChannel(const std::string& name) {
  // 해당 이름의 채널이 이미 존재하지 않는 경우 새 채널 생성하기
  if (channels.find(name) == channels.end()) {
    channels[name] = new Channel(name);
  }
}

Channel* IRCServer::findChannel(const std::string& name) {
  std::map<std::string, Channel*>::iterator it = channels.find(name);

  // 채널이 존재하는 경우 해당 채널의 객체 포인터 반환하기
  return it != channels.end() ? it->second : NULL;
}

const std::string IRCServer::getPassword() const { return password; }

void IRCServer::handleSigtstp(int signum) {
  (void)signum;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
  signal(SIGTSTP, SIG_DFL);
  raise(SIGTSTP);
}

void IRCServer::handleSigcont(int signum) {
  (void)signum;
  tcgetattr(STDIN_FILENO, &orig_termios);
}
