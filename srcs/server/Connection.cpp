// srcs/server/Connection.cpp (Updated)
#include "../../includes/server/Connection.hpp"
#include "../../includes/server/Server.hpp"
#include "../../includes/webserv.hpp" // Brings in all necessary headers and constants
#include "../../includes/http/RequestDispatcher.hpp"
#include "../../includes/http/HttpRequestHandler.hpp"
#include "../../includes/http/CGIHandler.hpp" // Added for CGIHandler usage


#include <unistd.h> // For read, write, close, waitpid, kill
#include <iostream> // For std::cerr, std::cout
#include <stdexcept> // For std::runtime_error
#include <string> // For std::string
#include <sstream> // For std::ostringstream
#include <vector> // For std::vector
#include <cstring> // For memset
#include <sys/socket.h> // For recv, send
#include <sys/wait.h> // For waitpid, WNOHANG, WIFEXITED, WEXITSTATUS

// Constructor: Initializes a new connection.
Connection::Connection(Server* server)
	: _state(READING), _server(server), _cgiHandler(NULL), _isCgiRequest(false),
	  _bytesSentFromRawResponse(0)
{
	_parser.reset();
	
}

// Destructor: Cleans up the CGI handler if it exists.
Connection::~Connection() {
	if (_cgiHandler) {
		std::cerr << "WARNING: CGIHandler still exists in Connection destructor for FD: " << getSocketFD() << ". Force-deleting and attempting FD cleanup." << std::endl;
		// The cleanup method of CGIHandler should handle unregistering FDs and closing pipes.
		_cgiHandler->cleanup();
		delete _cgiHandler;
		_cgiHandler = NULL;
	}
	// The client socket FD is closed by Server::_reapClosedConnections()
	// or when this Connection object is destroyed if not explicitly closed before.
	std::cout << "SOCKET " << getSocketFD() << " CLOSED (Connection dtor finished)" << std::endl;
}

// Handles reading data from the client socket.
void Connection::handleRead() {
	char buffer[BUFF_SIZE];
	memset(buffer, 0, BUFF_SIZE); // Ensure buffer is null-terminated for string operations
	ssize_t bytes_read = recv(getSocketFD(), buffer, BUFF_SIZE - 1, 0); // Use recv for sockets

	if (bytes_read > 0) {
		_parser.appendData(buffer, bytes_read); // Pass data to parser
	} else if (bytes_read == 0) { // Client closed connection
		if (!_parser.isComplete()) {
			std::cerr << "WARNING: Client closed connection on FD " << getSocketFD() << ", but request was incomplete. Sending 400 Bad Request." << std::endl;
			HttpRequestHandler handler;
			_response = handler._generateErrorResponse(400, this->getServerBlock(), NULL); // Bad Request
			setState(WRITING); // Send the 400 response
		} else {

			setState(CLOSING); // Mark for closing
		}
		return; // Exit early as connection is closing or response is being sent
	} else { // bytes_read < 0 (error)
		std::cerr << "Error reading from socket FD: " << getSocketFD() << ". Marking for CLOSING." << std::endl;
		setState(CLOSING);
		return; // Exit early on error
	}

	// Always attempt to parse after receiving data or if connection closed
	_parser.parse(); // Call parse() here

	if (_parser.isComplete()) {
		_request = _parser.getRequest();
		_processRequest();
	} else if (_parser.hasError()) {
		std::cerr << "ERROR: Request parsing error for FD: " << getSocketFD() << ". Closing connection." << std::endl;
		HttpRequestHandler handler;
		_response = handler._generateErrorResponse(400, this->getServerBlock(), NULL); // Bad Request
		setState(WRITING);
	}
}

// Handles writing data to the client socket.
void Connection::handleWrite() {
	if (_bytesSentFromRawResponse == 0) {
		_rawResponseToSend = _response.toString();
	}

	size_t remaining_to_send = _rawResponseToSend.length() - _bytesSentFromRawResponse;
	if (remaining_to_send == 0) {
		_resetForNextRequest(); // Prepare for next request if keep-alive, else close.
		return; // Early exit, state transition handled by reset.
	}

	// Use send for sockets
	ssize_t bytes_sent = send(getSocketFD(), _rawResponseToSend.c_str() + _bytesSentFromRawResponse, remaining_to_send, 0);

	if (bytes_sent < 0) { // treat as error
		std::cerr << "Error writing to socket FD: " << getSocketFD() << ". Closing connection." << std::endl;
		setState(CLOSING);
	} else if (bytes_sent == 0) {
		// No bytes were sent. This is not an error for non-blocking sockets;
		// it means the send buffer is full. Try again later.
		// Do nothing, just return. The poll loop will re-poll for POLLOUT.
	} else {
		_bytesSentFromRawResponse += bytes_sent;
		if (static_cast<size_t>(bytes_sent) == remaining_to_send || _bytesSentFromRawResponse >= _rawResponseToSend.length()) {
			std::cout << "Response sent completely on FD: " << getSocketFD() << std::endl;
			_resetForNextRequest(); // Response fully sent, prepare for next request
		} else {
			std::cout << "Partial write on FD: " << getSocketFD() << ". Sent " << bytes_sent << " of " << remaining_to_send << " remaining. Total sent: " << _bytesSentFromRawResponse << "/" << _rawResponseToSend.length() << std::endl;
		}
	}
}

// Processes the parsed HTTP request.
void Connection::_processRequest() {
	// Fix: Declared as const ServerConfig* to match getServerBlock() return type
	const ServerConfig* currentServerConfig = this->getServerBlock();
	if (!currentServerConfig) {
		std::cerr << "ERROR: Connection::_processRequest: currentServerConfig is NULL for FD: " << getSocketFD() << ". Cannot process request." << std::endl;
		HttpRequestHandler handler;
		_response = handler._generateErrorResponse(500, NULL, NULL);
		setState(WRITING);
		// _server->updateFdEvents(getSocketFD(), POLLOUT); // State change already handles this
		_bytesSentFromRawResponse = 0; // Reset for new response
		// _rawResponseToSend = _response.toString(); // Will be generated in handleWrite
		return;
	}

	MatchedConfig matchedConfig;
	matchedConfig.server_config = currentServerConfig;

	matchedConfig.location_config = RequestDispatcher::findMatchingLocation(_request, *currentServerConfig);

	const LocationConfig* location = matchedConfig.location_config;

	if (location && !location->cgiExecutables.empty()) {
		size_t dot_pos = _request.path.rfind('.');
		if (dot_pos != std::string::npos) {
			std::string file_extension = _request.path.substr(dot_pos);
			// Check if the exact extension is configured for CGI in this location
			if (location->cgiExecutables.count(file_extension)) {
				_isCgiRequest = true;
				setState(HANDLING_CGI); // Transition to a CGI specific state
				executeCGI();
			} else {
				// Not a matching CGI extension, handle as non-CGI
				_isCgiRequest = false;
				HttpRequestHandler handler;
				_response = handler.handleRequest(_request, matchedConfig);
				setState(WRITING);
				std::cout << "Request processed (non-CGI, but CGI configured location, no matching extension), transitioning to WRITING on FD: " << getSocketFD() << std::endl;
			}
		} else {
			// No file extension, handle as non-CGI
			_isCgiRequest = false;
			HttpRequestHandler handler;
			_response = handler.handleRequest(_request, matchedConfig);
			setState(WRITING);
			// _server->updateFdEvents(getSocketFD(), POLLOUT); // State change already handles this
			std::cout << "Request processed (non-CGI, no extension), transitioning to WRITING on FD: " << getSocketFD() << std::endl;
		}
	} else {
		// No CGI configured for this location or no location matched, handle as non-CGI
		_isCgiRequest = false;
		HttpRequestHandler handler;
		_response = handler.handleRequest(_request, matchedConfig);
		setState(WRITING);

		// _server->updateFdEvents(getSocketFD(), POLLOUT); // State change already handles this
		std::cout << "Request processed (non-CGI), transitioning to WRITING on FD: " << getSocketFD() << std::endl;
	}
}

// Initiates the CGI process for the current request.
void Connection::executeCGI() {
	// Fix: Declared as const ServerConfig* to match getServerBlock() return type
	const ServerConfig* currentServerConfig = this->getServerBlock();
	if (!currentServerConfig) {
		std::cerr << "ERROR: Connection::executeCGI: currentServerConfig is NULL for FD: " << getSocketFD() << ". Cannot execute CGI." << std::endl;
		HttpRequestHandler handler;
		_response = handler._generateErrorResponse(500, NULL, NULL);
		setState(WRITING);
		return;
	}

	MatchedConfig matchedConfig;
	matchedConfig.server_config = currentServerConfig;
	matchedConfig.location_config = RequestDispatcher::findMatchingLocation(_request, *currentServerConfig);

	// Fix: Pass _server (Server*) to CGIHandler constructor
	_cgiHandler = new CGIHandler(_request, matchedConfig.server_config, matchedConfig.location_config, _server);

	if (_cgiHandler->getState() == CGIState::CGI_PROCESS_ERROR) {
		std::cerr << "ERROR: CGIHandler failed to initialize (e.g., pipes, fork setup) for FD: " << getSocketFD() << std::endl;
		HttpRequestHandler handler;
		_response = handler._generateErrorResponse(500, matchedConfig.server_config, matchedConfig.location_config);
		setState(WRITING);
		delete _cgiHandler; // Clean up the failed handler
		_cgiHandler = NULL;
		return;
	}

	// The start() method will create pipes, fork, and execve.
	// It should also set pipe FDs to non-blocking.
	if (!_cgiHandler->start()) {
		std::cerr << "ERROR: CGI process failed to start (fork/pipe error) for FD: " << getSocketFD() << std::endl;
		HttpRequestHandler handler;
		_response = handler._generateErrorResponse(500, matchedConfig.server_config, matchedConfig.location_config);
		setState(WRITING);
		// _server->updateFdEvents(getSocketFD(), POLLOUT); // State change already handles this
		delete _cgiHandler;
		_cgiHandler = NULL;
		// _bytesSentFromRawResponse = 0; // Will be set in handleWrite
		// _rawResponseToSend = _response.toString(); // Will be generated in handleWrite
	} else {
		// Immediately update client socket to stop polling for its events while CGI runs
		_server->updateFdEvents(getSocketFD(), 0); // Stop polling client FD

		int cgiReadFd = _cgiHandler->getReadFd();
		if (cgiReadFd != -1) {
			_server->registerCgiFd(cgiReadFd, this, POLLIN); // Register CGI stdout (read end) for reading
		} else {
			std::cerr << "ERROR: CGI Read FD is invalid (start() succeeded but getReadFd() returned -1). Generating 500." << std::endl;
			_cgiHandler->setState(CGIState::CGI_PROCESS_ERROR); // Mark CGI as errored
			HttpRequestHandler handler;
			_response = handler._generateErrorResponse(500, matchedConfig.server_config, matchedConfig.location_config);
			setState(WRITING);
			delete _cgiHandler; _cgiHandler = NULL;
			return;
		}

		// Only register write pipe if there's a body to send (POST/PUT requests)
		// CGIHandler's state should indicate if it's expecting to write input
		if (_cgiHandler->getState() == CGIState::WRITING_INPUT) {
			int cgiWriteFd = _cgiHandler->getWriteFd();
			if (cgiWriteFd != -1) {
				_server->registerCgiFd(cgiWriteFd, this, POLLOUT); // Register CGI stdin (write end) for writing
			} else {
				std::cerr << "ERROR: CGI Write FD is invalid (start() succeeded but getWriteFd() returned -1 for POST). Generating 500." << std::endl;
				_cgiHandler->setState(CGIState::CGI_PROCESS_ERROR); // Mark CGI as errored
				HttpRequestHandler handler;
				_response = handler._generateErrorResponse(500, matchedConfig.server_config, matchedConfig.location_config);
				setState(WRITING);
				delete _cgiHandler; _cgiHandler = NULL;
				return;
			}
		}
	}
}

// Finalizes CGI handling and prepares the response.
void Connection::finalizeCGI() {
	if (_cgiHandler) {
		// Get the response generated by the CGIHandler (includes parsing CGI headers)
		_response = _cgiHandler->getHttpResponse();

		if (_cgiHandler->getState() != CGIState::COMPLETE) {
			std::cerr << "ERROR: CGI for FD " << getSocketFD() << " did not finish successfully (state: " << _cgiHandler->getState() << "). Generating 500 response." << std::endl;
			HttpRequestHandler handler;
			// Use _server_block (which is `const ServerConfig*`) for error response
			_response = handler._generateErrorResponse(500, this->getServerBlock(), NULL);
		}

		_cgiHandler->cleanup(); // CGIHandler's cleanup method handles process reaping and FD closure
		delete _cgiHandler;
		_cgiHandler = NULL;

		// Transition back to writing state and prepare response for sending
		setState(WRITING);
		_bytesSentFromRawResponse = 0; // Reset byte counter for client response
		// _rawResponseToSend will be generated in handleWrite()

		// Re-enable polling for client socket (POLLOUT)
		// This is handled by setState(WRITING) which calls _server->updateFdEvents
	} else {
		std::cerr << "ERROR: finalizeCGI called but _cgiHandler is NULL. This should not happen." << std::endl;
		// Generate a 500 error if CGI handler mysteriously disappeared
		HttpRequestHandler handler;
		_response = handler._generateErrorResponse(500, this->getServerBlock(), NULL);
		setState(WRITING);
	}
}

// Reset connection for a new request (e.g., for keep-alive)
void Connection::_resetForNextRequest() {
	_parser.reset();
	_request = HttpRequest(); // Reset request object
	_response = HttpResponse(); // Reset response object
	_requestBuffer.clear(); // Clear any buffered request data
	_rawResponseToSend.clear(); // Clear raw response
	_bytesSentFromRawResponse = 0; // Reset byte counter
	_isCgiRequest = false;
	// _server_block remains the same for the connection's lifetime
	// _cgiHandler should be NULL already after finalizeCGI
	if (_cgiHandler) { // Double check, if for some reason it's not NULL (e.g., error path)
		_cgiHandler->cleanup(); // Ensure FDs are cleaned up if not already
		delete _cgiHandler;
		_cgiHandler = NULL;
	}
	setState(READING); // Transition back to reading
}

// Returns the read file descriptor for the CGI's stdout pipe.
int Connection::getCgiReadFd() const {
	if (_cgiHandler) return _cgiHandler->getReadFd();
	return -1;
}

// Returns the write file descriptor for the CGI's stdin pipe.
int Connection::getCgiWriteFd() const {
	if (_cgiHandler) return _cgiHandler->getWriteFd();
	return -1;
}

// Returns the current state of the connection.
Connection::ConnectionState Connection::getState() const {
	return _state;
}

// Sets the state of the connection.
void Connection::setState(ConnectionState state) {
	_state = state;

	short events = 0;
	if (state == READING) {
		events = POLLIN;
	} else if (state == WRITING) {
		events = POLLOUT;
	} else if (state == CLOSING || state == HANDLING_CGI) {
		events = 0;
	}
	// Update poll events for the client socket FD
	_server->updateFdEvents(getSocketFD(), events);
}

// Checks if the current request is a CGI request.
bool Connection::isCGI() const {
	return _isCgiRequest;
}

// Returns a pointer to the CGI handler.
CGIHandler* Connection::getCgiHandler() const {
	return _cgiHandler;
}

bool Connection::hasActiveCGI() const {
	return _cgiHandler != NULL && !_cgiHandler->isFinished();
}
