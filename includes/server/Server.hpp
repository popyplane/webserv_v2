
#ifndef SERVER_HPP
# define SERVER_HPP

# include "../config/ServerStructures.hpp"
# include "Socket.hpp"
# include "Connection.hpp" // Make sure Connection.hpp is included
# include "divers.hpp" // For POLLIN, POLLOUT etc. (or webserv.hpp if it holds these)

# include <vector>
# include <map>
# include <stdexcept> // For std::runtime_error
# include <poll.h> // For pollfd struct and poll()

class Connection; // Forward declaration (already present)

class Server {
private:
    std::vector<ServerConfig>               _serverConfigs; // Renamed from _configs to match usage
    std::map<int, Socket*>                  _listenSockets; // Changed to map from vector for direct FD access
    std::vector<struct pollfd>              _pfds; // File descriptors to monitor with poll.
    std::map<int, Connection*>              _connections; // Map of client FD to Connection object.
    // _fdToIndexMap is no longer necessary if iterating pollfds directly by index or using map
    // std::map<int, size_t>                   _fdToIndexMap; // Map FD to its index in _pfds.

    // Map CGI pipe FDs back to their parent Connection objects.
    std::map<int, Connection*>              _cgiFdsToConnection; // Keep as Connection* for direct access

    bool                                    _running;       // Server running flag
    int                                     _timeout_ms;    // Poll timeout in milliseconds

    // Private helper methods
    bool _setupListeners(); // Renamed from setupListenSockets, changed return type to bool
    void _acceptNewConnection(int listen_fd);
    void _handleClientEvent(int client_fd, short revents); // New helper to consolidate client event handling
    void _handleCgiEvent(int cgi_fd, short revents);       // New helper to consolidate CGI event handling
    void _reapClosedConnections();

public:
    // Constructor and Destructor
    Server(const std::vector<ServerConfig>& configs);
    ~Server();

    // Main server loop
    void run();

    // Public methods for Connection to interact with Server
    const std::vector<ServerConfig>& getConfigs() const; // Renamed from _configs to _serverConfigs
    void updateFdEvents(int fd, short events);
    void _addFdToPoll(int fd, short events);
    void _removeFdFromPoll(int fd);

    // Methods for Connection to register/unregister CGI pipe FDs
    void registerCgiFd(int fd, Connection* conn, short events);
    void unregisterCgiFd(int fd);
};

#endif
