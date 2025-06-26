#include "../../includes/server/Connection.hpp"
#include "../../includes/server/Server.hpp" 
#include "../../includes/server/divers.hpp" 
#include "../../includes/http/RequestDispatcher.hpp"
#include "../../includes/webserv.hpp"
#include <iostream>
#include <unistd.h> 
#include <vector> 

// Constructor: Initializes a new connection.
Connection::Connection(Server* server) 
    : _state(READING), _server(server), _cgiHandler(NULL), _isCgiRequest(false) {
    _parser.reset();
}

// Destructor: Cleans up the CGI handler if it exists.
Connection::~Connection() {
    if (_cgiHandler) {
        delete _cgiHandler;
        _cgiHandler = NULL; 
    }
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
    } else { 
        std::cerr << "Error reading from socket FD: " << getSocketFD() << std::endl; 
        setState(CLOSING);
    }
}

// Handles writing data to the client socket.
void Connection::handleWrite() {
    std::string raw_response = _response.toString();
    
    ssize_t bytes_sent = write(getSocketFD(), raw_response.c_str(), raw_response.length());

    if (bytes_sent < 0) {
        std::cerr << "Error writing to socket FD: " << getSocketFD() << std::endl; 
        setState(CLOSING);
    } else if (static_cast<size_t>(bytes_sent) == raw_response.length()) {
        std::cout << "Response sent completely on FD: " << getSocketFD() << std::endl; 
        setState(CLOSING); 
    } else {
        std::cerr << "Partial write on FD: " << getSocketFD() << ". Sent " << bytes_sent << " of " << raw_response.length() << std::endl; 
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
        return;
    }
    std::cout << "DEBUG: Connection::_processRequest: Processing request for server: " << currentServerConfig->serverNames[0] << " on port: " << currentServerConfig->port << std::endl;

    MatchedConfig matchedConfig;
    matchedConfig.server_config = currentServerConfig; // Set the known server config

    // Call the static method to find the location within the known server config
    matchedConfig.location_config = RequestDispatcher::findMatchingLocation(_request, *currentServerConfig); 

    const LocationConfig* location = matchedConfig.location_config;
    
    if (location && !location->cgiExecutables.empty()) { 
        _isCgiRequest = true;
        setState(HANDLING_CGI); 
        executeCGI(); 
    } else {
        HttpRequestHandler handler; 
        _response = handler.handleRequest(_request, matchedConfig);
        setState(WRITING); 

        _server->updateFdEvents(getSocketFD(), POLLOUT);
        std::cout << "Request processed (non-CGI), transitioning to WRITING on FD: " << getSocketFD() << std::endl; 
    }
    _requestBuffer.clear(); 
    _parser.reset(); 
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
        return;
    }

    MatchedConfig matchedConfig;
    matchedConfig.server_config = currentServerConfig; // Set the known server config

    // Call the static method to find the location within the known server config
    matchedConfig.location_config = RequestDispatcher::findMatchingLocation(_request, *currentServerConfig); 

    _cgiHandler = new CGIHandler(_request, matchedConfig.server_config, matchedConfig.location_config);
    
    if (!_cgiHandler->start()) {
        std::cerr << "ERROR: CGI failed to start for FD: " << getSocketFD() << std::endl; 
        HttpRequestHandler handler;
        _response = handler._generateErrorResponse(500, matchedConfig.server_config, matchedConfig.location_config);
        setState(WRITING); 
        _server->updateFdEvents(getSocketFD(), POLLOUT);
        delete _cgiHandler; 
        _cgiHandler = NULL;
    } else {
        std::cout << "DEBUG: CGI process started for FD: " << getSocketFD() << std::endl; 
    }
}

// Finalizes CGI handling and prepares the response.
void Connection::finalizeCGI() {
    if (_cgiHandler) {
        _response = _cgiHandler->getHttpResponse(); 
        delete _cgiHandler;
        _cgiHandler = NULL;
        setState(WRITING); 

        _server->updateFdEvents(getSocketFD(), POLLOUT);
        std::cout << "CGI finalized, transitioning to WRITING on FD: " << getSocketFD() << std::endl; 
    }
}

// Returns the current state of the connection.
Connection::ConnectionState Connection::getState() const {
    return _state;
}

// Sets the state of the connection.
void Connection::setState(ConnectionState state) {
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