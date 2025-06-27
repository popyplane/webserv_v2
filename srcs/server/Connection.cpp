#include "../../includes/server/Connection.hpp"
#include "../../includes/server/Server.hpp"
#include "../../includes/webserv.hpp" // Brings in all necessary headers and constants
#include "../../includes/http/RequestDispatcher.hpp"
#include "../../includes/http/HttpRequestHandler.hpp"

// No longer needed explicitly if webserv.hpp pulls them
// #include <iostream>
// #include <unistd.h>
// #include <vector>
// #include <sys/wait.h>

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

        int cgiReadFd = _cgiHandler->getReadFd();
        int cgiWriteFd = _cgiHandler->getWriteFd();

        if (cgiReadFd != -1 && cgiReadFd != -2) {
            _server->unregisterCgiFd(cgiReadFd);
            close(cgiReadFd);
            std::cerr << "WARNING: Connection destructor closed CGI Read FD: " << cgiReadFd << std::endl;
        }
        if (cgiWriteFd != -1 && cgiWriteFd != -2) {
            _server->unregisterCgiFd(cgiWriteFd);
            close(cgiWriteFd);
            std::cerr << "WARNING: Connection destructor closed CGI Write FD: " << cgiWriteFd << std::endl;
        }
        delete _cgiHandler;
        _cgiHandler = NULL;
    }
    std::cout << "SOCKET " << getSocketFD() << " CLOSED" << std::endl;
    // The client socket FD is closed by Server::_reapClosedConnections()
    // or when this Connection object is destroyed if not explicitly closed before.
    // For consistency, Server::_reapClosedConnections explicitly calls close(fd).
}

// Handles reading data from the client socket.
void Connection::handleRead() {
    char buffer[BUFF_SIZE];
    ssize_t bytes_read = read(getSocketFD(), buffer, BUFF_SIZE - 1);

    if (bytes_read > 0) {
        _requestBuffer.insert(_requestBuffer.end(), buffer, buffer + bytes_read);
        _parser.appendData(buffer, bytes_read);

        _parser.parse();

        if (_parser.isComplete()) {
            _request = _parser.getRequest();
            _processRequest();
        } else if (_parser.hasError()) {
            std::cerr << "Request parsing error. Closing connection." << std::endl;
            setState(CLOSING);
        }
    } else if (bytes_read == 0) {
        std::cout << "Client closed connection on FD: " << getSocketFD() << std::endl;
        setState(CLOSING);
    } else { // bytes_read == -1, treat as error (cannot use errno)
        std::cerr << "Error reading from socket FD: " << getSocketFD() << ". Closing connection." << std::endl;
        setState(CLOSING);
    }
}

// Handles writing data to the client socket.
void Connection::handleWrite() {
    if (_bytesSentFromRawResponse == 0) {
        _rawResponseToSend = _response.toString();
        std::cout << "DEBUG: handleWrite: Generated raw response of size " << _rawResponseToSend.length() << " for FD: " << getSocketFD() << std::endl;
    }

    size_t remaining_to_send = _rawResponseToSend.length() - _bytesSentFromRawResponse;
    if (remaining_to_send == 0) {
        std::cout << "DEBUG: handleWrite: No more data to send for FD: " << getSocketFD() << ". Transitioning to CLOSING." << std::endl;
        setState(CLOSING);
        return;
    }

    ssize_t bytes_sent = write(getSocketFD(), _rawResponseToSend.c_str() + _bytesSentFromRawResponse, remaining_to_send);

    if (bytes_sent < 0) { // treat as error (cannot use errno)
        std::cerr << "Error writing to socket FD: " << getSocketFD() << ". Closing connection." << std::endl;
        setState(CLOSING);
    } else {
        _bytesSentFromRawResponse += bytes_sent;
        if (static_cast<size_t>(bytes_sent) == remaining_to_send) {
            std::cout << "Response sent completely on FD: " << getSocketFD() << std::endl;
            setState(CLOSING);
        } else {
            std::cout << "Partial write on FD: " << getSocketFD() << ". Sent " << bytes_sent << " of " << remaining_to_send << " remaining. Total sent: " << _bytesSentFromRawResponse << "/" << _rawResponseToSend.length() << std::endl;
            _server->updateFdEvents(getSocketFD(), POLLOUT);
        }
    }
}

// Processes the parsed HTTP request.
void Connection::_processRequest() {
    ServerConfig* currentServerConfig = this->getServerBlock();
    if (!currentServerConfig) {
        std::cerr << "ERROR: Connection::_processRequest: currentServerConfig is NULL for FD: " << getSocketFD() << ". Cannot process request." << std::endl;
        HttpRequestHandler handler;
        _response = handler._generateErrorResponse(500, NULL, NULL);
        setState(WRITING);
        _server->updateFdEvents(getSocketFD(), POLLOUT);
        _bytesSentFromRawResponse = 0;
        _rawResponseToSend = _response.toString();
        return;
    }
    std::cout << "DEBUG: Connection::_processRequest: Processing request for server: " << currentServerConfig->serverNames[0] << " on port: " << currentServerConfig->port << std::endl;

    MatchedConfig matchedConfig;
    matchedConfig.server_config = currentServerConfig;

    matchedConfig.location_config = RequestDispatcher::findMatchingLocation(_request, *currentServerConfig);

    const LocationConfig* location = matchedConfig.location_config;

    if (location && !location->cgiExecutables.empty()) {
        size_t dot_pos = _request.path.rfind('.');
        if (dot_pos != std::string::npos) {
            std::string file_extension = _request.path.substr(dot_pos);
            if (location->cgiExecutables.count(file_extension)) {
                _isCgiRequest = true;
                setState(HANDLING_CGI);
                executeCGI();
            } else {
                _isCgiRequest = false;
                HttpRequestHandler handler;
                _response = handler.handleRequest(_request, matchedConfig);
                setState(WRITING);
                _server->updateFdEvents(getSocketFD(), POLLOUT);
                std::cout << "Request processed (non-CGI, but CGI configured location), transitioning to WRITING on FD: " << getSocketFD() << std::endl;
            }
        } else {
            _isCgiRequest = false;
            HttpRequestHandler handler;
            _response = handler.handleRequest(_request, matchedConfig);
            setState(WRITING);
            _server->updateFdEvents(getSocketFD(), POLLOUT);
            std::cout << "Request processed (non-CGI, no extension), transitioning to WRITING on FD: " << getSocketFD() << std::endl;
        }
    } else {
        _isCgiRequest = false;
        HttpRequestHandler handler;
        _response = handler.handleRequest(_request, matchedConfig);
        setState(WRITING);

        _server->updateFdEvents(getSocketFD(), POLLOUT);
        std::cout << "Request processed (non-CGI), transitioning to WRITING on FD: " << getSocketFD() << std::endl;
    }

    _requestBuffer.clear();
    _parser.reset();
    _bytesSentFromRawResponse = 0;
    _rawResponseToSend.clear();
}

// Initiates the CGI process for the current request.
void Connection::executeCGI() {
    ServerConfig* currentServerConfig = this->getServerBlock();
    if (!currentServerConfig) {
        std::cerr << "ERROR: Connection::executeCGI: currentServerConfig is NULL for FD: " << getSocketFD() << ". Cannot execute CGI." << std::endl;
        HttpRequestHandler handler;
        _response = handler._generateErrorResponse(500, NULL, NULL);
        setState(WRITING);
        _server->updateFdEvents(getSocketFD(), POLLOUT);
        _bytesSentFromRawResponse = 0;
        _rawResponseToSend = _response.toString();
        return;
    }

    MatchedConfig matchedConfig;
    matchedConfig.server_config = currentServerConfig;
    matchedConfig.location_config = RequestDispatcher::findMatchingLocation(_request, *currentServerConfig);

    _cgiHandler = new CGIHandler(_request, matchedConfig.server_config, matchedConfig.location_config);

    if (_cgiHandler->getState() == CGIState::CGI_PROCESS_ERROR) {
        std::cerr << "ERROR: CGIHandler failed to initialize for FD: " << getSocketFD() << std::endl;
        HttpRequestHandler handler;
        _response = handler._generateErrorResponse(500, matchedConfig.server_config, matchedConfig.location_config);
        setState(WRITING);
        _server->updateFdEvents(getSocketFD(), POLLOUT);
        delete _cgiHandler;
        _cgiHandler = NULL;
        _bytesSentFromRawResponse = 0;
        _rawResponseToSend = _response.toString();
        return;
    }

    if (!_cgiHandler->start()) {
        std::cerr << "ERROR: CGI process failed to start (fork/pipe error) for FD: " << getSocketFD() << std::endl;
        HttpRequestHandler handler;
        _response = handler._generateErrorResponse(500, matchedConfig.server_config, matchedConfig.location_config);
        setState(WRITING);
        _server->updateFdEvents(getSocketFD(), POLLOUT);
        delete _cgiHandler;
        _cgiHandler = NULL;
        _bytesSentFromRawResponse = 0;
        _rawResponseToSend = _response.toString();
    } else {
        std::cout << "DEBUG: CGI process started for FD: " << getSocketFD() << std::endl;

        _server->updateFdEvents(getSocketFD(), 0);

        int cgiReadFd = _cgiHandler->getReadFd();
        if (cgiReadFd != -1) {
            _server->registerCgiFd(cgiReadFd, this, POLLIN);
        } else {
            std::cerr << "ERROR: CGI Read FD is invalid (start() succeeded but getReadFd() returned -1). Generating 500." << std::endl;
            _cgiHandler->setState(CGIState::CGI_PROCESS_ERROR);
            HttpRequestHandler handler;
            _response = handler._generateErrorResponse(500, matchedConfig.server_config, matchedConfig.location_config);
            setState(WRITING);
            _server->updateFdEvents(getSocketFD(), POLLOUT);
            delete _cgiHandler; _cgiHandler = NULL;
            _bytesSentFromRawResponse = 0;
            _rawResponseToSend = _response.toString();
            return;
        }

        if (_cgiHandler->getState() == CGIState::WRITING_INPUT) {
            int cgiWriteFd = _cgiHandler->getWriteFd();
            if (cgiWriteFd != -1) {
                _server->registerCgiFd(cgiWriteFd, this, POLLOUT);
            } else {
                std::cerr << "ERROR: CGI Write FD is invalid (start() succeeded but getWriteFd() returned -1 for POST). Generating 500." << std::endl;
                _cgiHandler->setState(CGIState::CGI_PROCESS_ERROR);
                HttpRequestHandler handler;
                _response = handler._generateErrorResponse(500, matchedConfig.server_config, matchedConfig.set_location_config);
                setState(WRITING);
                _server->updateFdEvents(getSocketFD(), POLLOUT);
                delete _cgiHandler; _cgiHandler = NULL;
                _bytesSentFromRawResponse = 0;
                _rawResponseToSend = _response.toString();
                return;
            }
        }
    }
}

// Finalizes CGI handling and prepares the response.
void Connection::finalizeCGI() {
    if (_cgiHandler) {
        int cgiReadFdToUnregister = _cgiHandler->getReadFd();
        int cgiWriteFdToUnregister = _cgiHandler->getWriteFd();
        pid_t cgiPid = _cgiHandler->getCGIPid();

        _response = _cgiHandler->getHttpResponse();

        delete _cgiHandler;
        _cgiHandler = NULL;

        if (cgiPid != -1) {
            std::cout << "DEBUG: Finalizing CGI: Waiting for PID " << cgiPid << std::endl;
            int status;
            pid_t result = waitpid(cgiPid, &status, WNOHANG);
            if (result == 0) {
                std::cout << "DEBUG: Finalizing CGI: PID " << cgiPid << " still running, sending SIGTERM." << std::endl;
                kill(cgiPid, SIGTERM);
                waitpid(cgiPid, &status, 0);
            } else if (result == -1) { // waitpid itself failed
                std::cerr << "ERROR: Finalizing CGI: waitpid failed for PID " << cgiPid << ". (Perhaps already reaped)." << std::endl;
            }
        }

        if (cgiReadFdToUnregister != -1 && cgiReadFdToUnregister != -2) {
            _server->unregisterCgiFd(cgiReadFdToUnregister);
            close(cgiReadFdToUnregister);
            std::cout << "DEBUG: Finalizing CGI: Unregistered and closed CGI Read FD: " << cgiReadFdToUnregister << std::endl;
        } else {
            std::cout << "DEBUG: Finalizing CGI: CGI Read FD " << cgiReadFdToUnregister << " was already conceptually closed or invalid. Skipping unregister/close." << std::endl;
        }

        if (cgiWriteFdToUnregister != -1 && cgiWriteFdToUnregister != -2) {
            _server->unregisterCgiFd(cgiWriteFdToUnregister);
            close(cgiWriteFdToUnregister);
            std::cout << "DEBUG: Finalizing CGI: Unregistered and closed CGI Write FD: " << cgiWriteFdToUnregister << std::endl;
        } else {
            std::cout << "DEBUG: Finalizing CGI: CGI Write FD " << cgiWriteFdToUnregister << " was already conceptually closed or invalid. Skipping unregister/close." << std::endl;
        }

        if (_response.getStatusCode() == 0) {
            _response.setStatus(200);
        }

        setState(WRITING);
        _bytesSentFromRawResponse = 0;
        _rawResponseToSend = _response.toString();

        _server->updateFdEvents(getSocketFD(), POLLOUT);
        std::cout << "DEBUG: CGI finalized for FD: " << getSocketFD() << ", transitioning to WRITING." << std::endl;
    } else {
        std::cerr << "ERROR: finalizeCGI called but _cgiHandler is NULL. This should not happen." << std::endl;
        setState(CLOSING);
    }
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
}

// Checks if the current request is a CGI request.
bool Connection::isCGI() const {
    return _isCgiRequest;
}

// Returns a pointer to the CGI handler.
CGIHandler* Connection::getCgiHandler() const {
    return _cgiHandler;
}
