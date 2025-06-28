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
    std::cout << "DEBUG: Entering handleRead() for FD " << getSocketFD() << std::endl;
    char buffer[BUFF_SIZE];
    memset(buffer, 0, BUFF_SIZE); // Ensure buffer is null-terminated for string operations
    ssize_t bytes_read = recv(getSocketFD(), buffer, BUFF_SIZE - 1, 0); // Use recv for sockets

    std::cout << "DEBUG: handleRead() on FD " << getSocketFD() << ", bytes_read: " << bytes_read << std::endl;

    if (bytes_read > 0) {
        _parser.appendData(buffer, bytes_read); // Pass data to parser
        std::cout << "DEBUG: Appended " << bytes_read << " bytes to parser for FD " << getSocketFD() << std::endl;
    } else if (bytes_read < 0) { // bytes_read < 0, treat as error (cannot use errno directly, but can assume transient for non-blocking or fatal)
        std::cerr << "Error reading from socket FD: " << getSocketFD() << ". Marking for CLOSING." << std::endl;
        setState(CLOSING);
        std::cout << "DEBUG: Exiting handleRead() for FD " << getSocketFD() << std::endl;
        return; // Exit early on error
    }

    // Always attempt to parse after receiving data or if connection closed
    _parser.parse(); // Call parse() here

    if (_parser.isComplete()) {
        _request = _parser.getRequest();
        std::cout << "DEBUG: Request parsing complete for FD " << getSocketFD() << ". URI: " << _request.uri << std::endl;
        _processRequest();
    } else if (_parser.hasError()) {
        std::cerr << "ERROR: Request parsing error for FD: " << getSocketFD() << ". Closing connection." << std::endl;
        HttpRequestHandler handler;
        _response = handler._generateErrorResponse(400, this->getServerBlock(), NULL); // Bad Request
        setState(WRITING);
    } else {
        std::cout << "DEBUG: Request parsing still in progress for FD " << getSocketFD() << ". Waiting for more data." << std::endl;
        // If bytes_read was 0 and parser is still incomplete, then it's a malformed request.
        if (bytes_read == 0) {
            std::cerr << "ERROR: Client closed connection, but request is incomplete. Malformed request. Closing connection." << std::endl;
            HttpRequestHandler handler;
            _response = handler._generateErrorResponse(400, this->getServerBlock(), NULL); // Bad Request
            setState(WRITING); // Send a 400 response before closing
        }
    }
    std::cout << "DEBUG: Exiting handleRead() for FD " << getSocketFD() << std::endl;
}

// Handles writing data to the client socket.
void Connection::handleWrite() {
    std::cout << "DEBUG: Entering handleWrite() for FD " << getSocketFD() << std::endl;
    if (_bytesSentFromRawResponse == 0) {
        _rawResponseToSend = _response.toString();
        std::cout << "DEBUG: handleWrite: Generated raw response of size " << _rawResponseToSend.length() << " for FD: " << getSocketFD() << std::endl;
        // std::cout << "--- RESPONSE START ---\n" << _rawResponseToSend << "\n--- RESPONSE END ---" << std::endl;
    }

    size_t remaining_to_send = _rawResponseToSend.length() - _bytesSentFromRawResponse;
    if (remaining_to_send == 0) {
        std::cout << "DEBUG: handleWrite: No more data to send for FD: " << getSocketFD() << ". Transitioning to CLOSING." << std::endl;
        _resetForNextRequest(); // Prepare for next request if keep-alive, else close.
        return; // Early exit, state transition handled by reset.
    }

    // Use send for sockets
    ssize_t bytes_sent = send(getSocketFD(), _rawResponseToSend.c_str() + _bytesSentFromRawResponse, remaining_to_send, 0);

    std::cout << "DEBUG: handleWrite() on FD " << getSocketFD() << ", bytes_sent: " << bytes_sent << ", total response size: " << _rawResponseToSend.length() << std::endl;

    if (bytes_sent < 0) { // treat as error (cannot use errno directly)
        std::cerr << "Error writing to socket FD: " << getSocketFD() << ". Closing connection." << std::endl;
        setState(CLOSING);
    } else {
        _bytesSentFromRawResponse += bytes_sent;
        if (static_cast<size_t>(bytes_sent) == remaining_to_send || _bytesSentFromRawResponse >= _rawResponseToSend.length()) {
            std::cout << "Response sent completely on FD: " << getSocketFD() << std::endl;
            _resetForNextRequest(); // Response fully sent, prepare for next request
        } else {
            std::cout << "Partial write on FD: " << getSocketFD() << ". Sent " << bytes_sent << " of " << remaining_to_send << " remaining. Total sent: " << _bytesSentFromRawResponse << "/" << _rawResponseToSend.length() << std::endl;
            // _server->updateFdEvents(getSocketFD(), POLLOUT); // State change already handles this
        }
    }
    std::cout << "DEBUG: Exiting handleWrite() for FD " << getSocketFD() << std::endl;
}

// Processes the parsed HTTP request.
void Connection::_processRequest() {
    std::cout << "DEBUG: Entering _processRequest() for FD " << getSocketFD() << std::endl;
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
    std::cout << "DEBUG: Connection::_processRequest: Processing request for server: " << currentServerConfig->serverNames[0] << " on port: " << currentServerConfig->port << std::endl;

    MatchedConfig matchedConfig;
    // We need to use const_cast if the MatchedConfig structure requires non-const ServerConfig*
    // but the source is const. It's better to make MatchedConfig also use const pointers.
    matchedConfig.server_config = currentServerConfig;

    matchedConfig.location_config = RequestDispatcher::findMatchingLocation(_request, *currentServerConfig);

    const LocationConfig* location = matchedConfig.location_config;

    if (location) {
        std::cout << "DEBUG: Matched location: " << location->path << std::endl;
    } else {
        std::cout << "DEBUG: No specific location matched. Using server defaults." << std::endl;
    }

    if (location && !location->cgiExecutables.empty()) {
        size_t dot_pos = _request.path.rfind('.');
        if (dot_pos != std::string::npos) {
            std::string file_extension = _request.path.substr(dot_pos);
            // Check if the exact extension is configured for CGI in this location
            if (location->cgiExecutables.count(file_extension)) {
                std::cout << "DEBUG: Request is for a CGI script: " << _request.path << std::endl;
                _isCgiRequest = true;
                setState(HANDLING_CGI); // Transition to a CGI specific state
                executeCGI();
            } else {
                // Not a matching CGI extension, handle as non-CGI
                std::cout << "DEBUG: Request is not a CGI script (extension mismatch)." << std::endl;
                _isCgiRequest = false;
                HttpRequestHandler handler;
                _response = handler.handleRequest(_request, matchedConfig);
                setState(WRITING);
                // _server->updateFdEvents(getSocketFD(), POLLOUT); // State change already handles this
                std::cout << "Request processed (non-CGI, but CGI configured location, no matching extension), transitioning to WRITING on FD: " << getSocketFD() << std::endl;
            }
        } else {
            // No file extension, handle as non-CGI
            std::cout << "DEBUG: Request is not a CGI script (no extension)." << std::endl;
            _isCgiRequest = false;
            HttpRequestHandler handler;
            _response = handler.handleRequest(_request, matchedConfig);
            setState(WRITING);
            // _server->updateFdEvents(getSocketFD(), POLLOUT); // State change already handles this
            std::cout << "Request processed (non-CGI, no extension), transitioning to WRITING on FD: " << getSocketFD() << std::endl;
        }
    } else {
        // No CGI configured for this location or no location matched, handle as non-CGI
        std::cout << "DEBUG: Request is not a CGI script (no CGI config for location)." << std::endl;
        _isCgiRequest = false;
        HttpRequestHandler handler;
        _response = handler.handleRequest(_request, matchedConfig);
        setState(WRITING);

        // _server->updateFdEvents(getSocketFD(), POLLOUT); // State change already handles this
        std::cout << "Request processed (non-CGI), transitioning to WRITING on FD: " << getSocketFD() << std::endl;
    }

    // Resetting for the next request is handled by _resetForNextRequest()
    // after the response is sent. Don't reset here or it will clear the response.
    // _requestBuffer.clear();
    // _parser.reset();
    // _bytesSentFromRawResponse = 0;
    // _rawResponseToSend.clear();
    std::cout << "DEBUG: Exiting _processRequest() for FD " << getSocketFD() << std::endl;
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
        // _server->updateFdEvents(getSocketFD(), POLLOUT); // State change already handles this
        // _bytesSentFromRawResponse = 0; // Will be set in handleWrite
        // _rawResponseToSend = _response.toString(); // Will be generated in handleWrite
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
        // _server->updateFdEvents(getSocketFD(), POLLOUT); // State change already handles this
        delete _cgiHandler; // Clean up the failed handler
        _cgiHandler = NULL;
        // _bytesSentFromRawResponse = 0; // Will be set in handleWrite
        // _rawResponseToSend = _response.toString(); // Will be generated in handleWrite
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
        std::cout << "DEBUG: CGI process started for FD: " << getSocketFD() << std::endl;

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
            // _server->updateFdEvents(getSocketFD(), POLLOUT); // State change already handles this
            delete _cgiHandler; _cgiHandler = NULL;
            // _bytesSentFromRawResponse = 0; // Will be set in handleWrite
            // _rawResponseToSend = _response.toString(); // Will be generated in handleWrite
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
                // Fix: Corrected `set_location_config` to `location_config`
                _response = handler._generateErrorResponse(500, matchedConfig.server_config, matchedConfig.location_config);
                setState(WRITING);
                // _server->updateFdEvents(getSocketFD(), POLLOUT); // State change already handles this
                delete _cgiHandler; _cgiHandler = NULL;
                // _bytesSentFromRawResponse = 0; // Will be set in handleWrite
                // _rawResponseToSend = _response.toString(); // Will be generated in handleWrite
                return;
            }
        }
    }
}

// Finalizes CGI handling and prepares the response.
void Connection::finalizeCGI() {
    std::cout << "DEBUG: Connection::finalizeCGI() called for FD: " << getSocketFD() << std::endl;

    if (_cgiHandler) {
        // Removed: pid_t cgiPid = _cgiHandler->getCGIPid(); // This line is no longer needed

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

        // Removed: The explicit waitpid block has been moved to CGIHandler::cleanup()
        /*
        if (cgiPid != -1) {
            int status;
            pid_t result = waitpid(cgiPid, &status, WNOHANG);
            if (result == 0) {
                std::cout << "DEBUG: Finalizing CGI: PID " << cgiPid << " still running, sending SIGTERM." << std::endl;
                kill(cgiPid, SIGTERM);
                waitpid(cgiPid, &status, 0);
            } else if (result == -1) {
                 // std::cerr << "ERROR: Finalizing CGI: waitpid failed for PID " << cgiPid << ". (Perhaps already reaped or no such child)." << std::endl;
            }
            if (WIFEXITED(status)) {
                 std::cout << "DEBUG: CGI PID " << cgiPid << " exited with status: " << WEXITSTATUS(status) << std::endl;
            } else if (WIFSIGNALED(status)) {
                 std::cout << "DEBUG: CGI PID " << cgiPid << " terminated by signal: " << WTERMSIG(status) << std::endl;
            }
        }
        */

        // Transition back to writing state and prepare response for sending
        setState(WRITING);
        _bytesSentFromRawResponse = 0; // Reset byte counter for client response
        // _rawResponseToSend will be generated in handleWrite()

        // Re-enable polling for client socket (POLLOUT)
        // This is handled by setState(WRITING) which calls _server->updateFdEvents
        std::cout << "DEBUG: CGI finalized for FD: " << getSocketFD() << ", transitioning to WRITING." << std::endl;
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
    std::cout << "DEBUG: Connection FD " << getSocketFD() << " reset for next request. State: READING." << std::endl;
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
    std::cout << "DEBUG: Connection FD " << getSocketFD() << ": State change from " << _state << " to " << state << std::endl;
    _state = state;

    short events = 0;
    if (state == READING) {
        events = POLLIN;
    } else if (state == WRITING) {
        events = POLLOUT;
    } else if (state == CLOSING || state == HANDLING_CGI) {
        // For CLOSING, no events needed, it's about to be reaped.
        // For HANDLING_CGI, client socket should not be polled (CGI pipes are).
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
