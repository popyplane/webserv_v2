// srcs/server/Server.cpp
#include "../../includes/server/Server.hpp"
#include "../../includes/server/Connection.hpp"
#include "../../includes/webserv.hpp" // For POLL_TIMEOUT_MS, BUFF_SIZE, etc.
#include "../../includes/http/HttpRequestHandler.hpp" // For error responses
#include "../../includes/utils/StringUtils.hpp" // For StringUtils::longToString

#include <iostream> // For std::cerr, std::cout
#include <unistd.h> // For close()
#include <algorithm> // For std::find, std::remove
#include <cstring> // For strerror

// Constructor: Initializes the server with configurations.
Server::Server(const std::vector<ServerConfig>& configs)
	: _serverConfigs(configs),
	  _running(false),
	  _timeout_ms(POLL_TIMEOUT_MS)
{}

// Destructor: Cleans up all connections and poll file descriptors.
Server::~Server() {
	std::cout << "Server shutting down. Closing all open sockets." << std::endl;

	// Clean up connections
	for (std::map<int, Connection*>::iterator it = _connections.begin(); it != _connections.end(); ++it) {
		delete it->second; // Calls ~Connection() which handles its socket and CGI FDs
	}
	_connections.clear();

	// Clean up listen sockets
	for (std::map<int, Socket*>::iterator it = _listenSockets.begin(); it != _listenSockets.end(); ++it) {
		std::cout << "Closing listen socket FD: " << it->first << std::endl;
		// The Socket destructor already calls close(_sockfd), so just delete the object.
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
		// Check if this port is already being listened on by another server block
		for (std::map<int, Socket*>::const_iterator ls_it = _listenSockets.begin(); ls_it != _listenSockets.end(); ++ls_it) {
			// Compare by port number, assuming unique listener per port
			if (ls_it->second->getPort() == static_cast<int>(it->port)) { // Cast to int for comparison
				port_already_listening = true;
				break;
			}
		}

		if (port_already_listening) {
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
		listenSocket->setServerBlock(&(*it)); // This line is correct now due to const correctness fix
		_listenSockets[listenSocket->getSocketFD()] = listenSocket;
		_addFdToPoll(listenSocket->getSocketFD(), POLLIN);
		std::cout << "listen socket : " << listenSocket->getSocketFD() << std::endl;
	}
	return success;
}

// Adds a file descriptor to the pollfd list.
void Server::_addFdToPoll(int fd, short events) {
	if (fd == -1) {
		std::cerr << "WARNING: Attempted to add invalid FD (-1) to poll list." << std::endl;
		return;
	}
	// Check if FD already exists to prevent duplicates
	for (size_t i = 0; i < _pfds.size(); ++i) {
		if (_pfds[i].fd == fd) {
			std::cerr << "WARNING: FD " << fd << " already exists in poll list. Updating events instead." << std::endl;
			_pfds[i].events = events; // Update events
			return;
		}
	}

	pollfd pfd;
	pfd.fd = fd;
	pfd.events = events;
	pfd.revents = 0;
	_pfds.push_back(pfd);
}

// Updates the events for an existing file descriptor in the pollfd list.
void Server::updateFdEvents(int fd, short new_events) {
	for (size_t i = 0; i < _pfds.size(); ++i) {
		if (_pfds[i].fd == fd) {
			if (_pfds[i].events != new_events) {
				_pfds[i].events = new_events;
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
		updateFdEvents(cgi_fd, events); // If already exists, just update events
	} else {
		_cgiFdsToConnection[cgi_fd] = conn;
		_addFdToPoll(cgi_fd, events);
	}
}

// Unregisters a CGI file descriptor.
void Server::unregisterCgiFd(int cgi_fd) {
	if (_cgiFdsToConnection.count(cgi_fd)) {
		_cgiFdsToConnection.erase(cgi_fd);
		_removeFdFromPoll(cgi_fd);
		// It's crucial to close the CGI FD here if it was opened by the server
		if (cgi_fd != -1) {
			int close_res = close(cgi_fd);
			if (close_res < 0) {
				perror("Error closing unregistered CGI FD");
			}
		}
	} else {
		std::cerr << "WARNING: unregisterCgiFd: Attempted to unregister non-existent CGI FD " << cgi_fd << std::endl;
	}
}

// Accepts a new client connection.
void Server::_acceptNewConnection(int listen_fd) {
	ServerConfig* associatedConfig = NULL;
	// Find the associated ServerConfig for this listening socket
	if (_listenSockets.count(listen_fd)) {
		associatedConfig = const_cast<ServerConfig*>(_listenSockets[listen_fd]->getServerBlock()); // Remove constness for now if ServerConfig* is expected elsewhere
	} else {
		std::cerr << "ERROR: _acceptNewConnection: Listen FD " << listen_fd << " not found in _listenSockets map. Cannot get config." << std::endl;
		return; // Cannot proceed without config
	}

	// acceptConnection returns a new client_fd or -1 on error
	int client_fd = _listenSockets[listen_fd]->acceptConnection(listen_fd);
	if (client_fd > 0) {
		Connection* newConnection = new Connection(this);
		newConnection->setSocketFD(client_fd);
		newConnection->setServerBlock(associatedConfig);
		_connections[client_fd] = newConnection;
		_addFdToPoll(client_fd, POLLIN); // Start polling for reads on the new connection
	} else if (client_fd == -1) { // Error accepting (e.g., EINTR, EAGAIN/EWOULDBLOCK if non-blocking and no connections)
		// Log the error using a generic message.
		std::cerr << "Error accepting new connection on listen FD " << listen_fd << ". (May be non-blocking)." << std::endl;
	}
}

// Handles events on client sockets.
void Server::_handleClientEvent(int client_fd, short revents) {
	if (_connections.count(client_fd) == 0) {
		std::cerr << "ERROR: _handleClientEvent: Client FD " << client_fd << " not found in _connections map. Removing from poll and closing." << std::endl;
		_removeFdFromPoll(client_fd);
		if (client_fd != -1) close(client_fd); // Ensure closing if orphaned
		return;
	}

	Connection* conn = _connections[client_fd];

	if (revents & POLLHUP) {
		std::cout << "Client FD " << client_fd << " hung up. Marking for CLOSING." << std::endl;
		conn->setState(Connection::CLOSING);
	} else if (revents & (POLLERR | POLLNVAL)) { // POLLNVAL for invalid FD
		std::cerr << "Error or invalid FD on client FD " << client_fd << ". Revents: " << revents << ". Marking for CLOSING." << std::endl;
		conn->setState(Connection::CLOSING);
	} else if (revents & POLLIN && conn->getState() == Connection::READING) {
		conn->handleRead();
	} else if (revents & POLLOUT && conn->getState() == Connection::WRITING) {
		conn->handleWrite();
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

	// Ensure the associated client connection still exists
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

	if (revents & POLLIN) {
		cgiHandler->handleRead();
	}
	if (revents & POLLOUT) {
		cgiHandler->handleWrite();
	}
	if (revents & (POLLERR | POLLNVAL)) { // POLLNVAL for invalid FD, POLLERR for error on FD
		std::cerr << "ERROR: CGI pipe FD " << cgi_fd << " received POLLERR/POLLNVAL. Revents: " << revents << ". Marking CGI as error." << std::endl;
		cgiHandler->setState(CGIState::CGI_PROCESS_ERROR);
	}

	// Call pollCGIProcess to manage child process status (waitpid) and state transitions
	cgiHandler->pollCGIProcess();

	if (cgiHandler->isFinished()) {
		conn->finalizeCGI(); // This should transition connection state and potentially trigger response sending
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
		if (_connections.count(client_fd) == 0) { // Safety check
			continue;
		}

		// Connection destructor handles its CGIHandler cleanup and closes its own socket
		_removeFdFromPoll(client_fd); // Remove client_fd from main poll list
		delete _connections[client_fd];
		_connections.erase(client_fd);
	}

	// Clean up CGI FDs that might be orphaned if their parent connection was reaped earlier
	std::vector<int> cgi_fds_to_remove;
	for (std::map<int, Connection*>::iterator it = _cgiFdsToConnection.begin(); it != _cgiFdsToConnection.end(); ++it) {
		// If the associated client connection is no longer in _connections (i.e., it was reaped), this CGI FD is orphaned.
		// Also check if it->second is NULL, which indicates a serious error or race condition.
		if (!it->second || _connections.count(it->second->getSocketFD()) == 0) {
			cgi_fds_to_remove.push_back(it->first);
		}
	}
	for (size_t i = 0; i < cgi_fds_to_remove.size(); ++i) {
		int cgi_fd = cgi_fds_to_remove[i];
		std::cerr << "WARNING: Found orphaned CGI FD " << cgi_fd << ". Removing from poll and closing." << std::endl;
		_removeFdFromPoll(cgi_fd);
		_cgiFdsToConnection.erase(cgi_fd);
		if (cgi_fd != -1) {
			int close_res = close(cgi_fd);
			if (close_res < 0) {
				perror("Error closing orphaned CGI FD");
			}
		}
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

	while (_running && !stopSig) {
		// Make sure _pfds is not empty before calling poll
		if (_pfds.empty()) {
			std::cout << "INFO: No active file descriptors to poll. Server will idle or exit." << std::endl;
			// Depending on desired behavior, could sleep or exit
			break; // Exit if no FDs to poll
		}

		int num_events = poll(_pfds.data(), _pfds.size(), _timeout_ms);

		if (num_events < 0) {
			std::cerr << "Poll error. Server shutting down." << std::endl;
			_running = false;
			break;
		}

		if (num_events == 0) {
			// Poll timeout. No events.
		} else {
			// Check for CGI timeouts
			for (std::map<int, Connection*>::iterator it = _connections.begin(); it != _connections.end(); ++it) {
				Connection* conn = it->second;
				if (conn->hasActiveCGI()) {
					CGIHandler* cgiHandler = conn->getCgiHandler();
					if (cgiHandler->checkTimeout()) {
						std::cerr << "WARNING: CGI timeout detected for client FD " << conn->getSocketFD() << "." << std::endl;
						cgiHandler->setTimeout();
						conn->finalizeCGI(); // Finalize the connection's CGI handling
					}
				}
			}
			// Iterate backwards to safely remove elements during iteration
			for (long i = _pfds.size() - 1; i >= 0; --i) {
				int current_fd = _pfds[i].fd;
				short revents = _pfds[i].revents;

				if (revents == 0) { // No events on this FD
					continue;
				}

				// Determine if it's a listen socket, client connection, or CGI pipe
				if (_listenSockets.count(current_fd)) {
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
					// This case indicates an FD in _pfds that isn't managed by our maps.
					// This can happen if an FD was removed from _connections or _cgiFdsToConnection
					// but not from _pfds, or if it's an old FD that shouldn't be there.
					std::cerr << "WARNING: Unknown FD " << current_fd << " with revents " << revents << " in poll list. Attempting to remove and close." << std::endl;
					_removeFdFromPoll(current_fd); // Remove from poll list
					if (current_fd != -1) {
						int close_res = close(current_fd); // Close the FD
						if (close_res < 0) {
							perror("Error closing unknown FD");
						}
					}
				}
			}
		}
		_reapClosedConnections(); // Clean up connections marked for closing
	}
}

// Public method to get server configurations.
const std::vector<ServerConfig>& Server::getConfigs() const {
	return _serverConfigs;
}