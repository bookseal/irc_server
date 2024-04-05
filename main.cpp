#include "IRCServer.hpp" // Make sure this path is correct

int main() {
    IRCServer server(6667); // Create an IRC server object on port 6667
    server.run(); // Run the server
    return 0;
}
