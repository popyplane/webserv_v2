#ifndef SERVER_HPP
# define SERVER_HPP

# include "../webserv.hpp"
# include "Socket.hpp"
# include "Connection.hpp"
# include "../config/ServerStructures.hpp"
# include <vector>
# include <map>
# include <poll.h>

// Represents the main HTTP server, managing listening sockets and client connections.
class Server
{
private:
    std::vector<ServerConfig>       _configs; // Server configurations loaded from file.
    std::vector<Socket*>            _listenSockets; // Sockets listening for new connections.
    std::vector<struct pollfd>      _pfds; // File descriptors monitored by poll().
    std::map<int, Connection*>      _connections; // Map of active client connections.

    // Sets up listening sockets based on server configurations.
    void setupListenSockets();
    // Adds a file descriptor to the pollfd list and sets it to non-blocking mode.
    void _addFdToPoll(int fd, short events);
    // Removes a file descriptor from the pollfd list.
    void _removeFdFromPoll(int fd);
    // Accepts a new client connection on a listening socket.
    void _acceptNewConnection(int listen_fd);
    // Handles POLLIN events (data available for reading or new connection).
    void _handlePollIn(size_t i);
    // Handles POLLOUT events (socket ready for writing).
    void _handlePollOut(size_t i);
    // Cleans up and removes closed connections.
    void _reapClosedConnections();

public:
    // Constructor: Initializes the server with configurations.
    Server(const std::vector<ServerConfig>& configs);
    // Destructor: Cleans up listening sockets and active connections.
    virtual ~Server();

    // Main server loop: Monitors file descriptors for I/O events.
    void run();
    // Returns the server configurations.
    const std::vector<ServerConfig>& getConfigs() const;
};

#endif