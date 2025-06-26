#ifndef SERVER_HPP
# define SERVER_HPP

# include <vector>
# include <map> // Required for std::map
# include <poll.h> // Required for struct pollfd
# include "../config/ServerStructures.hpp"
# include "Socket.hpp"
# include "Connection.hpp" // Forward declaration moved to .cpp if not needed in .hpp for direct member usage.
                         // But if Connection* is a member, it's needed here. Keeping it for now.

// Forward declaration to avoid circular dependency (if Server is passed to Connection constructor).
class Connection;

// Main server class responsible for managing listening sockets and client connections.
class Server {
private:
    std::vector<ServerConfig>   _configs; // All server configurations loaded from file.
    std::vector<Socket*>        _listenSockets; // Sockets for listening on configured ports.
    std::vector<struct pollfd>  _pfds; // File descriptors monitored by poll().
    std::map<int, Connection*>  _connections; // Map of client FD to Connection object.
    
    // NEW: Map to quickly find the index of a file descriptor in _pfds
    std::map<int, size_t>       _fdToIndexMap; 

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
    // Destructor: Cleans up server resources.
    ~Server();

    // Main server loop: Monitors file descriptors for I/O events.
    void run();
    // Returns the server configurations.
    const std::vector<ServerConfig>& getConfigs() const;

    // NEW: Updates the poll events for a given file descriptor.
    void updateFdEvents(int fd, short events);
};

#endif