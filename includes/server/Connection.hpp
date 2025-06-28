// includes/server/Connection.hpp (Updated)
#ifndef CONNECTION_HPP
# define CONNECTION_HPP

#include <vector>
#include <string>

#include "Socket.hpp"
#include "../http/HttpRequest.hpp"
#include "../http/HttpResponse.hpp"
#include "../http/HttpRequestParser.hpp"
#include "../http/CGIHandler.hpp"
#include "../config/ServerStructures.hpp" // For ServerConfig


// Forward declaration to break circular dependency
class Server;

// Represents a single client connection to the server.
class Connection : public Socket {
public:
    // Enum to represent the current state of the connection.
    enum ConnectionState {
        READING,         // Reading client request.
        HANDLING_CGI,    // Waiting for CGI response.
        WRITING,         // Writing response to client.
        CLOSING          // Connection needs to be closed and reaped.
    };

    // Constructor: Initializes a new connection.
    Connection(Server* server);

    // Destructor: Cleans up connection resources.
    ~Connection();

    // Handles incoming data from the client socket.
    void handleRead();

    // Handles sending outgoing data to the client socket.
    void handleWrite();

    // Initiates the CGI process for the current request.
    void executeCGI();

    // Finalizes CGI handling and prepares the HTTP response.
    void finalizeCGI();

    // Getters for CGI pipes (for Server to add to pollfds).
    int getCgiReadFd() const;
    int getCgiWriteFd() const;

    // Returns the current state of the connection.
    ConnectionState getState() const;

    // Sets the state of the connection.
    void setState(ConnectionState state);

    // Checks if the current request is a CGI request.
    bool isCGI() const;

    // Returns a pointer to the CGI handler.
    CGIHandler* getCgiHandler() const;

private:
    HttpRequest         _request;           // The parsed HTTP request.
    HttpResponse        _response;          // The HTTP response to be sent.
    HttpRequestParser   _parser;            // Parser for incoming request data.
    std::vector<char>   _requestBuffer;     // Buffer for raw incoming request data.
    ConnectionState     _state;             // Current state of the connection.
    Server* _server;            // Pointer to the parent server (for callbacks like updateFdEvents).
    CGIHandler* _cgiHandler;        // Pointer to CGI handler if this is a CGI request.
    bool                _isCgiRequest;      // Flag to indicate if the current request is for CGI.

    // New members to manage partial response sending
    std::string         _rawResponseToSend;         // The complete raw HTTP response string.
    size_t              _bytesSentFromRawResponse;  // Number of bytes sent from _rawResponseToSend.

    // Private helper methods
    void _processRequest();
    void _resetForNextRequest(); // <--- ADD THIS DECLARATION
};

#endif