#include "../../includes/server/Connection.hpp"
#include "../../includes/server/Server.hpp"
#include <iostream>
#include <unistd.h> // For read/write

// Constructor: Initializes a new connection.
Connection::Connection(Server* server) 
    : _state(READING), _server(server), _cgiHandler(NULL), _isCgiRequest(false) {
    _parser.reset();
}

// Destructor: Cleans up the CGI handler if it exists.
Connection::~Connection() {
    if (_cgiHandler) {
        delete _cgiHandler;
    }
}

// Handles reading data from the client socket.
void Connection::handleRead() {
    char buffer[BUFF_SIZE];
    // Read data from the socket.
    ssize_t bytes_read = read(getSocketFD(), buffer, BUFF_SIZE - 1);

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        _requestBuffer.insert(_requestBuffer.end(), buffer, buffer + bytes_read);
        _parser.appendData(buffer, bytes_read);
        _parser.parse();

        // If the request is complete, process it.
        if (_parser.isComplete()) {
            _request = _parser.getRequest();
            _processRequest();
        }
    } else {
        // If read returns 0 or -1, the connection is closed or an error occurred.
        setState(CLOSING);
    }
}

// Handles writing data to the client socket.
void Connection::handleWrite() {
    std::string raw_response = _response.toString();
    // Write data to the socket.
    ssize_t bytes_sent = write(getSocketFD(), raw_response.c_str(), raw_response.length());

    if (bytes_sent < 0) {
        // Error during write.
        setState(CLOSING);
    } else if (static_cast<size_t>(bytes_sent) == raw_response.length()) {
        // Entire response sent, close connection.
        setState(CLOSING);
    } else {
        // Partial send, remaining data will be handled in the next poll cycle.
    }
}

// Processes the parsed HTTP request.
void Connection::_processRequest() {
    GlobalConfig globalConfig;
    globalConfig.servers = _server->getConfigs();
    RequestDispatcher dispatcher(globalConfig);
    MatchedConfig matchedConfig = dispatcher.dispatch(_request, "", 0);
    
    const LocationConfig* location = matchedConfig.location_config;
    // Check if the request should be handled by a CGI script.
    if (location && !location->cgiExecutables.empty()) {
        _isCgiRequest = true;
        setState(HANDLING_CGI);
    } else {
        HttpRequestHandler handler;
        _response = handler.handleRequest(_request, matchedConfig);
        setState(WRITING);
    }
}

// Initiates the CGI process for the current request.
void Connection::executeCGI() {
    GlobalConfig globalConfig;
    globalConfig.servers = _server->getConfigs();
    RequestDispatcher dispatcher(globalConfig);
    MatchedConfig matchedConfig = dispatcher.dispatch(_request, "", 0);
    _cgiHandler = new CGIHandler(_request, matchedConfig.server_config, matchedConfig.location_config);
    
    // If CGI fails to start, generate an error response.
    if (!_cgiHandler->start()) {
        HttpRequestHandler handler;
        _response = handler.handleRequest(_request, matchedConfig);
        setState(WRITING);
        delete _cgiHandler;
        _cgiHandler = NULL;
    }
}

// Finalizes CGI handling and prepares the response.
void Connection::finalizeCGI() {
    if (_cgiHandler) {
        _response = _cgiHandler->getHttpResponse();
        delete _cgiHandler;
        _cgiHandler = NULL;
        setState(WRITING);
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
