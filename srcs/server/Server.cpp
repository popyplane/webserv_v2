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
    for (std::map<int, Connection*>::iterator it = _connections.begin(); it != _connections.end(); ++it) {
        delete it->second;
    }
}

// Sets up listening sockets based on server configurations.
void Server::setupListenSockets() {
    for (size_t i = 0; i < _configs.size(); ++i) {
        Socket* newSocket = new Socket();
        newSocket->initListenSocket(std::to_string(_configs[i].port).c_str());
        _listenSockets.push_back(newSocket);
        _addFdToPoll(newSocket->getSocketFD(), POLLIN);
    }
}

// Main server loop: Monitors file descriptors for I/O events using poll().
void Server::run() {
    while (true) {
        // Wait indefinitely for an event on any monitored file descriptor.
        int poll_count = poll(_pfds.data(), _pfds.size(), -1);
        if (poll_count < 0) {
            throw std::runtime_error("poll() failed");
        }

        // Iterate through all file descriptors to handle events.
        for (size_t i = 0; i < _pfds.size(); ++i) {
            if (_pfds[i].revents & POLLIN) {
                _handlePollIn(i);
            } else if (_pfds[i].revents & POLLOUT) {
                _handlePollOut(i);
            }
        }
        // Clean up closed connections.
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
    fcntl(fd, F_SETFL, O_NONBLOCK);
}

// Removes a file descriptor from the pollfd list.
void Server::_removeFdFromPoll(int fd) {
    for (std::vector<struct pollfd>::iterator it = _pfds.begin(); it != _pfds.end(); ++it) {
        if (it->fd == fd) {
            _pfds.erase(it);
            break;
        }
    }
}

// Accepts a new client connection on a listening socket.
void Server::_acceptNewConnection(int listen_fd) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        return; // Error accepting connection.
    }

    // Create a new Connection object and add it to the connections map and poll list.
    Connection* conn = new Connection(this);
    conn->setSocketFD(client_fd);
    _connections[client_fd] = conn;
    _addFdToPoll(client_fd, POLLIN);
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
            // If request is CGI, execute it and add CGI pipes to poll.
            if (conn->getState() == Connection::HANDLING_CGI) {
                conn->executeCGI();
                if (conn->getCgiHandler()) {
                    _addFdToPoll(conn->getCgiHandler()->getReadFd(), POLLIN);
                    _addFdToPoll(conn->getCgiHandler()->getWriteFd(), POLLOUT);
                }
            }
        } else if (conn->getState() == Connection::HANDLING_CGI && conn->getCgiHandler()) {
            // Continue reading from CGI output pipe.
            conn->getCgiHandler()->handleRead();
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
            _removeFdFromPoll(fd);
            close(fd);
            delete it->second;
            it = _connections.erase(it);
        } else {
            ++it;
        }
    }
}