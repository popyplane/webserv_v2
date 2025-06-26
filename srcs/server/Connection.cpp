#include "../../includes/server/Connection.hpp"
#include "../../includes/server/Server.hpp" // Needs full definition of Server
#include "../../includes/webserv.hpp" // For BUFF_SIZE
#include <iostream>
#include <unistd.h> // For read/write
#include <vector> // Ensure vector is included for _requestBuffer

// Constructor: Initializes a new connection.
Connection::Connection(Server* server) 
    : _state(READING), _server(server), _cgiHandler(NULL), _isCgiRequest(false) {
    _parser.reset();
}

// Destructor: Cleans up the CGI handler if it exists.
Connection::~Connection() {
    if (_cgiHandler) {
        delete _cgiHandler;
        _cgiHandler = NULL; // Good practice to nullify after delete
    }
}

// Handles reading data from the client socket.
void Connection::handleRead() {
    char buffer[BUFF_SIZE]; // BUFF_SIZE is now defined in divers.hpp
    // Read data from the socket.
    ssize_t bytes_read = read(getSocketFD(), buffer, BUFF_SIZE - 1);

    if (bytes_read > 0) {
        _requestBuffer.insert(_requestBuffer.end(), buffer, buffer + bytes_read);
        _parser.appendData(buffer, bytes_read); // Pass buffer and its length

        _parser.parse(); // Assuming this handles partial reads correctly

        // If the request is complete, process it.
        if (_parser.isComplete()) {
            _request = _parser.getRequest(); // Get the parsed request
            _processRequest(); // Process the request and potentially change state to WRITING
        } else if (_parser.hasError()) { // Check for parsing errors
            std::cerr << "Request parsing error. Closing connection." << std::endl;
            setState(CLOSING);
        }
    } else if (bytes_read == 0) {
        // Client closed the connection gracefully
        std::cout << "Client closed connection on FD: " << getSocketFD() << std::endl; // Debug print
        setState(CLOSING);
    } else { // bytes_read < 0
        // Error during read (e.g., connection reset, broken pipe)
        std::cerr << "Error reading from socket FD: " << getSocketFD() << std::endl; // Debug print
        setState(CLOSING);
    }
}

// Handles writing data to the client socket.
void Connection::handleWrite() {
    std::string raw_response = _response.toString();
    
    ssize_t bytes_sent = write(getSocketFD(), raw_response.c_str(), raw_response.length());

    if (bytes_sent < 0) {
        // Error during write.
        std::cerr << "Error writing to socket FD: " << getSocketFD() << std::endl; // Debug print
        setState(CLOSING);
    } else if (static_cast<size_t>(bytes_sent) == raw_response.length()) {
        // Entire response sent.
        std::cout << "Response sent completely on FD: " << getSocketFD() << std::endl; // Debug print
        setState(CLOSING); 
    } else {
        std::cerr << "Partial write on FD: " << getSocketFD() << ". Sent " << bytes_sent << " of " << raw_response.length() << std::endl; // Debug print
    }
}

// Processes the parsed HTTP request.
void Connection::_processRequest() {
    GlobalConfig globalConfig;
    globalConfig.servers = _server->getConfigs(); 
    RequestDispatcher dispatcher(globalConfig);
    
    MatchedConfig matchedConfig = dispatcher.dispatch(_request, "", 0); 
    
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
        std::cout << "Request processed (non-CGI), transitioning to WRITING on FD: " << getSocketFD() << std::endl; // Debug print
    }
    _requestBuffer.clear(); 
    _parser.reset(); 
}

// Initiates the CGI process for the current request.
void Connection::executeCGI() {
    GlobalConfig globalConfig;
    globalConfig.servers = _server->getConfigs();
    RequestDispatcher dispatcher(globalConfig);
    MatchedConfig matchedConfig = dispatcher.dispatch(_request, "", 0);
    
    _cgiHandler = new CGIHandler(_request, matchedConfig.server_config, matchedConfig.location_config);
    
    if (!_cgiHandler->start()) {
        std::cerr << "CGI failed to start for FD: " << getSocketFD() << std::endl; // Debug print
        HttpRequestHandler handler;
        // FIX: Call the now public _generateErrorResponse method
        _response = handler._generateErrorResponse(500, matchedConfig.server_config, matchedConfig.location_config);
        setState(WRITING); 
        _server->updateFdEvents(getSocketFD(), POLLOUT);
        delete _cgiHandler; 
        _cgiHandler = NULL;
    } else {
        std::cout << "CGI process started for FD: " << getSocketFD() << std::endl; // Debug print
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
        std::cout << "CGI finalized, transitioning to WRITING on FD: " << getSocketFD() << std::endl; // Debug print
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