
#include "../../includes/server/Server.hpp"
#include "../../includes/server/Connection.hpp"
#include "../../includes/webserv.hpp" // For POLL_TIMEOUT_MS, BUFF_SIZE, etc.
#include "../../includes/http/HttpRequestHandler.hpp" // For error responses

// No longer needed explicitly if webserv.hpp pulls them
// #include <iostream>
// #include <unistd.h>
// #include <algorithm>
// #include <cerrno> // Forbidden
// #include <cstring> // Forbidden

// Constructor: Initializes the server with configurations.
Server::Server(const std::vector<ServerConfig>& configs)
    : _serverConfigs(configs),
      _running(false),
      _timeout_ms(POLL_TIMEOUT_MS)
{
    // _pfds.reserve(MAX_CONNECTIONS); // C++98 compatible reserve
}

// Destructor: Cleans up all connections and poll file descriptors.
Server::~Server() {
    std::cout << "Server shutting down. Closing all open sockets." << std::endl;

    for (std::map<int, Connection*>::iterator it = _connections.begin(); it != _connections.end(); ++it) {
        delete it->second; // Calls ~Connection() which handles its socket and CGI FDs
    }
    _connections.clear();

    for (std::map<int, Socket*>::iterator it = _listenSockets.begin(); it != _listenSockets.end(); ++it) {
        std::cout << "Closing listen socket FD: " << it->first << std::endl;
        // The Socket destructor already calls close(_sockfd), so no explicit close here is needed.
        delete it->second;
    }
    _listenSockets.clear();

    _pfds.clear();
    _cgiFdsToConnection.clear();
}

// Sets up listener sockets based on server configurations.
bool Server::_setupListeners() {
    bool success = true;
    for (std::vector<ServerConfig>::const_iterator it = _serverConfigs.begin(); it != _serverConfigs.end(); ++it) {
        bool port_already_listening = false;
        for (std::map<int, Socket*>::const_iterator ls_it = _listenSockets.begin(); ls_it != _listenSockets.end(); ++ls_it) {
            if (ls_it->second->getPort() == it->port) {
                port_already_listening = true;
                break;
            }
        }

        if (port_already_listening) {
            std::cout << "DEBUG: Port " << it->port << " is already being listened on. Skipping duplicate listener setup." << std::endl;
            continue;
        }

        Socket* listenSocket = new Socket();
        // initListenSocket returns true on success, false on failure
        if (!listenSocket->initListenSocket(StringUtils::longToString(it->port).c_str())) {
            std::cerr << "Failed to initialize listen socket on port " << it->port << std::endl;
            delete listenSocket;
            success = false;
            continue;
        }
        std::cout << "DEBUG: Setting up listener on port: " << it->port << std::endl;
        listenSocket->setServerBlock(&(*it));
        _listenSockets[listenSocket->getSocketFD()] = listenSocket;
        _addFdToPoll(listenSocket->getSocketFD(), POLLIN);
        std::cout << "listen socket : " << listenSocket->getSocketFD() << std::endl;
    }
    return success;
}

// Adds a file descriptor to the pollfd list.
void Server::_addFdToPoll(int fd, short events) {
    pollfd pfd;
    pfd.fd = fd;
    pfd.events = events;
    pfd.revents = 0;
    _pfds.push_back(pfd);
    std::cout << "DEBUG: _addFdToPoll: Added FD " << fd << " to poll (events: " << events << "), total pfds: " << _pfds.size() << std::endl;
}

// Updates the events for an existing file descriptor in the pollfd list.
void Server::updateFdEvents(int fd, short new_events) {
    for (size_t i = 0; i < _pfds.size(); ++i) {
        if (_pfds[i].fd == fd) {
            if (_pfds[i].events != new_events) {
                _pfds[i].events = new_events;
                std::cout << "DEBUG: updateFdEvents: Updated FD " << fd << " poll events to: " << new_events << std::endl;
            }
            return;
        }
    }
    std::cerr << "WARNING: updateFdEvents: Attempted to update events for non-existent FD: " << fd << std::endl;
}

// Removes a file descriptor from the pollfd list.
void Server::_removeFdFromPoll(int fd) {
    for (std::vector<pollfd>::iterator it = _pfds.begin(); it != _pfds.end(); ++it) {
        if (it->fd == fd) {
            _pfds.erase(it);
            std::cout << "DEBUG: _removeFdFromPoll: Removed FD " << fd << " from poll. Remaining pfds: " << _pfds.size() << std::endl;
            return;
        }
    }
    std::cerr << "WARNING: _removeFdFromPoll: Attempted to remove non-existent FD: " << fd << std::endl;
}

// Registers a CGI file descriptor with its associated connection.
void Server::registerCgiFd(int cgi_fd, Connection* conn, short events) {
    if (cgi_fd == -1) {
        std::cerr << "ERROR: registerCgiFd: Attempted to register invalid CGI FD (-1)." << std::endl;
        return;
    }
    if (_cgiFdsToConnection.count(cgi_fd)) {
        std::cout << "DEBUG: registerCgiFd: Attempting to register FD " << cgi_fd << " for conn FD " << conn->getSocketFD() << ". _cgiFdsToConnection size (before): " << _cgiFdsToConnection.size() << std::endl;
        updateFdEvents(cgi_fd, events);
    } else {
        std::cout << "DEBUG: registerCgiFd: Attempting to register FD " << cgi_fd << " for conn FD " << conn->getSocketFD() << ". _cgiFdsToConnection size (before): " << _cgiFdsToConnection.size() << std::endl;
        _cgiFdsToConnection[cgi_fd] = conn;
        _addFdToPoll(cgi_fd, events);
    }
    std::cout << "DEBUG: registerCgiFd: Registered CGI FD " << cgi_fd << " for connection FD " << conn->getSocketFD() << " with events " << events << ". _cgiFdsToConnection size (after): " << _cgiFdsToConnection.size() << std::endl;
}

// Unregisters a CGI file descriptor.
void Server::unregisterCgiFd(int cgi_fd) {
    std::cout << "DEBUG: unregisterCgiFd: Attempting to unregister FD " << cgi_fd << ". _cgiFdsToConnection size (before): " << _cgiFdsToConnection.size() << std::endl;
    if (_cgiFdsToConnection.count(cgi_fd)) {
        _cgiFdsToConnection.erase(cgi_fd);
        _removeFdFromPoll(cgi_fd);
        std::cout << "DEBUG: unregisterCgiFd: Unregistered CGI FD " << cgi_fd << ". _cgiFdsToConnection size (after): " << _cgiFdsToConnection.size() << std::endl;
    } else {
        std::cerr << "WARNING: unregisterCgiFd: Attempted to unregister non-existent CGI FD " << cgi_fd << std::endl;
    }
}

// Accepts a new client connection.
void Server::_acceptNewConnection(int listen_fd) {
    ServerConfig* associatedConfig = NULL;
    if (_listenSockets.count(listen_fd)) {
        associatedConfig = _listenSockets[listen_fd]->getServerBlock();
    } else {
        std::cerr << "ERROR: _acceptNewConnection: Listen FD " << listen_fd << " not found in _listenSockets map. Cannot get config." << std::endl;
    }

    // acceptConnection returns a new client_fd or -1 on error
    int client_fd = _listenSockets[listen_fd]->acceptConnection(listen_fd);
    if (client_fd > 0) {
        Connection* newConnection = new Connection(this);
        newConnection->setSocketFD(client_fd);
        newConnection->setServerBlock(associatedConfig);
        _connections[client_fd] = newConnection;
        _addFdToPoll(client_fd, POLLIN);
        std::cout << "DEBUG: Accepted new connection on FD: " << client_fd << std::endl;
        std::cout << "DEBUG: _acceptNewConnection: _connections size (after adding client FD " << client_fd << "): " << _connections.size() << std::endl;
    } else if (client_fd == -1) { // Error accepting
        std::cerr << "Error accepting new connection on listen FD " << listen_fd << "." << std::endl;
    }
    // If client_fd is 0 (non-blocking, no pending connections), do nothing.
}

// Handles events on client sockets.
void Server::_handleClientEvent(int client_fd, short revents) {
    if (_connections.count(client_fd) == 0) {
        std::cerr << "ERROR: _handleClientEvent: Client FD " << client_fd << " not found in _connections map. Removing from poll." << std::endl;
        _removeFdFromPoll(client_fd);
        if (client_fd != -1) close(client_fd);
        return;
    }

    Connection* conn = _connections[client_fd];
    std::cout << "DEBUG: FD " << client_fd << " is a client connection. State: " << conn->getState() << std::endl;

    if (revents & POLLHUP) {
        std::cout << "Client FD " << client_fd << " hung up." << std::endl;
        conn->setState(Connection::CLOSING);
    } else if (revents & POLLERR) {
        std::cerr << "Error on client FD " << client_fd << ". Revents: " << revents << std::endl;
        conn->setState(Connection::CLOSING);
    } else if (revents & POLLIN && conn->getState() == Connection::READING) {
        conn->handleRead();
    } else if (revents & POLLOUT && conn->getState() == Connection::WRITING) {
        conn->handleWrite();
    } else {
        std::cout << "DEBUG: Client FD " << client_fd << " has revents " << revents << " but not handled in current state " << conn->getState() << "." << std::endl;
    }
}

// Handles events on CGI pipes.
void Server::_handleCgiEvent(int cgi_fd, short revents) {
    if (_cgiFdsToConnection.count(cgi_fd) == 0) {
        std::cerr << "ERROR: CGI pipe FD " << cgi_fd << " found in poll but not in _cgiFdsToConnection map. Removing from poll and closing." << std::endl;
        _removeFdFromPoll(cgi_fd);
        if (cgi_fd != -1) close(cgi_fd);
        return;
    }

    Connection* conn = _cgiFdsToConnection[cgi_fd];
    if (!conn) {
        std::cerr << "ERROR: CGI pipe FD " << cgi_fd << " associated with NULL Connection*. Removing from poll and closing." << std::endl;
        _removeFdFromPoll(cgi_fd);
        _cgiFdsToConnection.erase(cgi_fd);
        if (cgi_fd != -1) close(cgi_fd);
        return;
    }

    if (_connections.count(conn->getSocketFD()) == 0) {
        std::cerr << "ERROR: CGI pipe FD " << cgi_fd << ": Associated client connection FD " << conn->getSocketFD() << " not found. Removing CGI FD from poll and closing." << std::endl;
        _removeFdFromPoll(cgi_fd);
        _cgiFdsToConnection.erase(cgi_fd);
        if (cgi_fd != -1) close(cgi_fd);
        return;
    }

    CGIHandler* cgiHandler = conn->getCgiHandler();

    if (!cgiHandler) {
        std::cerr << "ERROR: CGI pipe FD " << cgi_fd << " has no associated CGIHandler. Removing from poll and closing." << std::endl;
        _removeFdFromPoll(cgi_fd);
        _cgiFdsToConnection.erase(cgi_fd);
        if (cgi_fd != -1) close(cgi_fd);
        return;
    }

    std::cout << "DEBUG: FD " << cgi_fd << " is a CGI pipe (found in _cgiFdsToConnection). Associated Client FD: " << conn->getSocketFD() << std::endl;

    if (revents & POLLIN) {
        std::cout << "DEBUG: CGI pipe FD " << cgi_fd << ": POLLIN detected (read end of stdout pipe)." << std::endl;
        cgiHandler->handleRead();
    }
    if (revents & POLLOUT) {
        std::cout << "DEBUG: CGI pipe FD " << cgi_fd << ": POLLOUT detected (write end of stdin pipe)." << std::endl;
        cgiHandler->handleWrite();
    }
    if (revents & (POLLERR | POLLNVAL)) {
        std::cerr << "ERROR: CGI pipe FD " << cgi_fd << " received POLLERR/POLLNVAL. Revents: " << revents << ". Marking CGI as error." << std::endl;
        cgiHandler->setState(CGIState::CGI_PROCESS_ERROR);
    }
    if (revents & POLLHUP) {
        std::cout << "DEBUG: CGI pipe FD " << cgi_fd << ": POLLHUP detected (write end of pipe closed by CGI). Ensuring all data read and allowing pollCGIProcess to finalize." << std::endl;
        if (revents & POLLIN) {
            cgiHandler->handleRead();
        }
    }

    cgiHandler->pollCGIProcess();

    if (cgiHandler->isFinished()) {
        std::cout << "DEBUG: Server loop: CGI for client FD " << conn->getSocketFD() << " finished. Finalizing connection." << std::endl;
        conn->finalizeCGI();
    }
}

// Reaps connections marked for closing.
void Server::_reapClosedConnections() {
    std::vector<int> fds_to_reap;
    for (std::map<int, Connection*>::iterator it = _connections.begin(); it != _connections.end(); ++it) {
        if (it->second->getState() == Connection::CLOSING) {
            fds_to_reap.push_back(it->first);
        }
    }

    for (size_t i = 0; i < fds_to_reap.size(); ++i) {
        int client_fd = fds_to_reap[i];
        if (_connections.count(client_fd) == 0) {
            continue;
        }

        std::cout << "DEBUG: Reaping closed connection FD: " << client_fd << std::endl;

        // Connection destructor handles its CGIHandler cleanup.
        _removeFdFromPoll(client_fd); // Remove client_fd from main poll list
        // No explicit close(client_fd) here, as Connection destructor handles it
        delete _connections[client_fd];
        _connections.erase(client_fd);
        std::cout << "DEBUG: _reapClosedConnections: _connections size (after reaping FD " << client_fd << "): " << _connections.size() << std::endl;
    }

    std::vector<int> cgi_fds_to_remove;
    for (std::map<int, Connection*>::iterator it = _cgiFdsToConnection.begin(); it != _cgiFdsToConnection.end(); ++it) {
        // If the associated client connection is no longer in _connections, this CGI FD is orphaned.
        if (it->second && _connections.count(it->second->getSocketFD()) == 0) {
            std::cout << "DEBUG: Found CGI pipe FD " << it->first << " orphaned from client FD " << it->second->getSocketFD() << std::endl;
            cgi_fds_to_remove.push_back(it->first);
        }
    }
    for (size_t i = 0; i < cgi_fds_to_remove.size(); ++i) {
        int cgi_fd = cgi_fds_to_remove[i];
        std::cerr << "WARNING: Found orphaned CGI FD " << cgi_fd << ". Removing from poll and closing." << std::endl;
        _removeFdFromPoll(cgi_fd);
        _cgiFdsToConnection.erase(cgi_fd);
        if (cgi_fd != -1) close(cgi_fd);
    }
}


// Main server loop.
void Server::run() {
    if (!_setupListeners()) {
        std::cerr << "Failed to set up listeners. Exiting." << std::endl;
        return;
    }

    _running = true;
    std::cout << "Server running and listening..." << std::endl;

    while (_running) {
        std::cout << "DEBUG: --- Poll Cycle Start ---" << std::endl;
        std::cout << "DEBUG: _pfds size: " << _pfds.size() << ", _connections size: " << _connections.size() << ", _cgiFdsToConnection size: " << _cgiFdsToConnection.size() << std::endl;

        int num_events = poll(_pfds.data(), _pfds.size(), _timeout_ms);

        if (num_events < 0) {
            std::cerr << "Poll error. Server shutting down." << std::endl;
            // No errno check for EINTR
            _running = false;
            break;
        }

        if (num_events == 0) {
            // Poll timeout. No events.
        } else {
            // Iterate backwards for safe removal
            for (long i = _pfds.size() - 1; i >= 0; --i) {
                int current_fd = _pfds[i].fd;
                short revents = _pfds[i].revents;

                if (revents == 0) {
                    continue;
                }

                std::cout << "DEBUG: Processing FD " << current_fd << " with revents " << revents << std::endl;

                if (_listenSockets.count(current_fd)) {
                    std::cout << "DEBUG: FD " << current_fd << " is a listen socket." << std::endl;
                    if (revents & POLLIN) {
                        _acceptNewConnection(current_fd);
                    }
                }
                else if (_connections.count(current_fd)) {
                    _handleClientEvent(current_fd, revents);
                }
                else if (_cgiFdsToConnection.count(current_fd)) {
                    _handleCgiEvent(current_fd, revents);
                }
                else {
                    std::cerr << "WARNING: Unknown FD " << current_fd << " with revents " << revents << " in poll list. Attempting to remove and close." << std::endl;
                    _removeFdFromPoll(current_fd);
                    if (current_fd != -1) close(current_fd);
                }
            }
        }
        _reapClosedConnections();
        std::cout << "DEBUG: --- Poll Cycle End ---" << std::endl;
    }
}

// Public method to get server configurations.
const std::vector<ServerConfig>& Server::getConfigs() const {
    return _serverConfigs;
}
