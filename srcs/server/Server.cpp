#include "../../includes/server/Server.hpp"
#include <iostream>
#include <stdexcept>
#include <unistd.h>
#include <fcntl.h>

// Constructor: Initializes the server with configurations and sets up listening sockets.
Server::Server(const std::vector<ServerConfig>& configs) : _configs(configs) {
    setupListenSockets();
}

// Destructor: Cleans up listening sockets and active connections.
Server::~Server() {
    for (size_t i = 0; i < _listenSockets.size(); ++i) {
        delete _listenSockets[i];
    }
    // Iterate using a temporary copy of keys to avoid iterator invalidation
    // if _connections.erase() triggers rebalancing (though less likely with map).
    // More robust way to clear connections map:
    std::map<int, Connection*>::iterator it = _connections.begin();
    while (it != _connections.end()) {
        delete it->second;
        ++it;
    }
    _connections.clear(); // Explicitly clear the map after deleting objects
}

// Sets up listening sockets based on server configurations.
void Server::setupListenSockets() {
    for (size_t i = 0; i < _configs.size(); ++i) {
        Socket* newSocket = new Socket();
        // Check for duplicate ports if you plan to support multiple server blocks
        // on the same host:port, otherwise only the first bind succeeds.
        // For now, simply attempt to bind all configured ports.
        std::cout << "Setting up listener on port: " << _configs[i].port << std::endl; // Debug print
        newSocket->initListenSocket((StringUtils::longToString(_configs[i].port)).c_str());
        newSocket->setServerBlock(&_configs[i]); // Associate config with socket
        _listenSockets.push_back(newSocket);
        _addFdToPoll(newSocket->getSocketFD(), POLLIN); // Listening sockets only need POLLIN
    }
}

// Main server loop: Monitors file descriptors for I/O events using poll().
void Server::run() {
    std::cout << "Server running and listening..." << std::endl; // Debug print
    while (true) {
        // Wait indefinitely for an event on any monitored file descriptor.
        int poll_count = poll(_pfds.data(), _pfds.size(), -1);
        if (poll_count < 0) {
            // Note: Subject forbids errno. This is a generic error message.
            throw std::runtime_error("poll() failed");
        }

        // Iterate through all file descriptors to handle events.
        // Iterate backwards if you modify _pfds (e.g., remove FDs),
        // or iterate forwards and manage index changes.
        // For loop is okay if _reapClosedConnections handles removals carefully,
        // or if you copy _pfds for iteration, but current _pfds access is common.
        for (size_t i = 0; i < _pfds.size(); ++i) {
            if (_pfds[i].revents & POLLIN) {
                _handlePollIn(i);
            }
            if (_pfds[i].revents & POLLOUT) { // Use 'if' not 'else if' to handle POLLIN and POLLOUT on same cycle if needed
                _handlePollOut(i);
            }
            // Check for POLLERR, POLLHUP (connection hung up), POLLNVAL (invalid request)
            if (_pfds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                 int fd = _pfds[i].fd;
                 std::cerr << "Error or hangup on FD: " << fd << std::endl;
                 if (_connections.count(fd)) {
                     _connections[fd]->setState(Connection::CLOSING);
                 } else { // It's a listening socket with an error
                     close(fd);
                     _removeFdFromPoll(fd);
                     // Consider re-initializing the listening socket or exiting.
                 }
            }
        }
        // Clean up closed connections. This happens after all current events are processed.
        _reapClosedConnections();
    }
}

// Returns the server configurations.
const std::vector<ServerConfig>& Server::getConfigs() const {
    return _configs;
}

// Adds a file descriptor to the pollfd list and sets it to non-blocking mode.
void Server::_addFdToPoll(int fd, short events) {
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = events;
    pfd.revents = 0;
    _pfds.push_back(pfd);
    _fdToIndexMap[fd] = _pfds.size() - 1; // Store the index
    fcntl(fd, F_SETFL, O_NONBLOCK);
    std::cout << "Added FD " << fd << " to poll (events: " << events << ")" << std::endl; // Debug print
}

// Removes a file descriptor from the pollfd list.
void Server::_removeFdFromPoll(int fd) {
    if (_fdToIndexMap.count(fd)) {
        size_t index_to_remove = _fdToIndexMap[fd];
        _pfds.erase(_pfds.begin() + index_to_remove);
        _fdToIndexMap.erase(fd);
        std::cout << "Removed FD " << fd << " from poll." << std::endl; // Debug print

        // Rebuild the map for subsequent elements if any were shifted
        // This is crucial because erasing from vector shifts elements
        for (size_t i = index_to_remove; i < _pfds.size(); ++i) {
            _fdToIndexMap[_pfds[i].fd] = i;
        }
    }
}

// NEW FUNCTION: Updates the poll events for a given file descriptor.
void Server::updateFdEvents(int fd, short events) {
    if (_fdToIndexMap.count(fd)) {
        size_t index = _fdToIndexMap[fd];
        _pfds[index].events = events;
        std::cout << "Updated FD " << fd << " poll events to: " << events << std::endl; // Debug print
    } else {
        std::cerr << "Warning: Attempted to update events for non-existent FD " << fd << std::endl;
    }
}

// Accepts a new client connection on a listening socket.
void Server::_acceptNewConnection(int listen_fd) {
    struct sockaddr_storage client_addr; // Use sockaddr_storage for generic address
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        std::cerr << "Error accepting connection." << std::endl; // Debug print
        return; // Error accepting connection.
    }
    std::cout << "Accepted new connection on FD: " << client_fd << std::endl; // Debug print

    // Find the ServerConfig associated with this listening socket
    ServerConfig* associatedConfig = NULL;
    for (size_t i = 0; i < _listenSockets.size(); ++i) {
        if (_listenSockets[i]->getSocketFD() == listen_fd) {
            associatedConfig = _listenSockets[i]->getServerBlock();
            break;
        }
    }
    
    // Create a new Connection object and add it to the connections map and poll list.
    Connection* conn = new Connection(this);
    conn->setSocketFD(client_fd);
    conn->setServerBlock(associatedConfig); // Pass the associated server block to the connection
    _connections[client_fd] = conn;
    _addFdToPoll(client_fd, POLLIN); // Client connections start by reading
}

// Handles POLLIN events (data available for reading or new connection).
void Server::_handlePollIn(size_t i) {
    int fd = _pfds[i].fd;
    bool is_listen_socket = false;
    // Check if the event is on a listening socket.
    for (size_t j = 0; j < _listenSockets.size(); ++j) {
        if (_listenSockets[j]->getSocketFD() == fd) {
            is_listen_socket = true;
            break;
        }
    }

    if (is_listen_socket) {
        _acceptNewConnection(fd);
    } else if (_connections.count(fd)) {
        Connection* conn = _connections[fd];
        // Handle client connection based on its state.
        if (conn->getState() == Connection::READING) {
            conn->handleRead();
            // If request is complete AND not CGI, it will set state to WRITING and update poll events.
            // If it's CGI, it will set state to HANDLING_CGI and executeCGI will be called next loop or directly.
            // No need to explicitly change poll events here if Connection::handleRead ultimately calls _processRequest
            // which then calls updateFdEvents.
        } else if (conn->getState() == Connection::HANDLING_CGI && conn->getCgiHandler()) {
            // Continue reading from CGI output pipe.
            // Note: If CGI handler has its own FDs, they should be managed by Server as well.
            conn->getCgiHandler()->handleRead();
            if (conn->getCgiHandler()->isFinished()) {
                conn->finalizeCGI(); // Finalize CGI once all output is read
            }
        }
    }
}

// Handles POLLOUT events (socket ready for writing).
void Server::_handlePollOut(size_t i) {
    int fd = _pfds[i].fd;
    if (_connections.count(fd)) {
        Connection* conn = _connections[fd];
        // Handle client connection based on its state.
        if (conn->getState() == Connection::WRITING) {
            conn->handleWrite();
        } else if (conn->getState() == Connection::HANDLING_CGI && conn->getCgiHandler()) {
            // Continue writing to CGI input pipe.
            conn->getCgiHandler()->handleWrite();
        }
    }
}

// Cleans up and removes closed connections.
void Server::_reapClosedConnections() {
    for (std::map<int, Connection*>::iterator it = _connections.begin(); it != _connections.end(); ) {
        if (it->second->getState() == Connection::CLOSING) {
            int fd = it->first;
            std::cout << "Reaping closed connection FD: " << fd << std::endl; // Debug print
            _removeFdFromPoll(fd); // Remove from pollfds and map
            // The Socket's closeSocket handles the actual close()
            it->second->closeSocket(); // Call Socket's close to handle errors/logging
            delete it->second;
            _connections.erase(it++); // Erase from map and advance iterator
        } else {
            ++it;
        }
    }
}