#ifndef CONNECTION_HPP
# define CONNECTION_HPP

# include "../http/HttpRequestParser.hpp"
# include "../http/HttpRequest.hpp"
# include "../http/HttpResponse.hpp"
# include "../http/HttpRequestHandler.hpp"
# include "../http/RequestDispatcher.hpp"
# include "../http/CGIHandler.hpp"
# include "divers.hpp"
# include "Socket.hpp"
# include "../config/ServerStructures.hpp"

// Forward declaration to avoid circular dependency.
class Server; // Needs to be here because Connection holds a Server*

// Represents a single client connection, handling request parsing, response generation, and CGI interaction.
class Connection : public Socket {
public:
    // Enum to track the state of the connection during request processing.
    enum ConnectionState {
        READING, // Currently reading data from the client.
        WRITING, // Currently writing data to the client.
        HANDLING_CGI, // Waiting for or interacting with a CGI process.
        CLOSING // Connection is being closed.
    };

private:
    HttpRequestParser   _parser; // Parses incoming HTTP request data.
    HttpRequest         _request; // Stores the parsed HTTP request.
    HttpResponse        _response; // Stores the HTTP response to be sent.
    ConnectionState     _state; // Current state of the connection.
    Server* _server; // Pointer to the parent server for configuration access.

    CGIHandler* _cgiHandler; // Manages the CGI process if applicable.
    bool                _isCgiRequest; // Flag indicating if the current request is for CGI.

    std::vector<char>   _requestBuffer; // Buffer for incoming raw request data.

public:
    // Constructor: Initializes a new connection with a reference to the server.
    Connection(Server* server);
    // Destructor: Cleans up the CGI handler if it exists.
    virtual ~Connection();

    // Handles reading data from the client socket.
    void handleRead();
    // Handles writing data to the client socket.
    void handleWrite();

    // Returns the current state of the connection.
    ConnectionState getState() const;
    // Sets the state of the connection.
    void setState(ConnectionState state);

    // Checks if the current request is a CGI request.
    bool isCGI() const;
    // Initiates the CGI process for the current request.
    void executeCGI();
    // Returns a pointer to the CGI handler.
    CGIHandler* getCgiHandler() const;
    // Finalizes CGI handling and prepares the response.
    void finalizeCGI();

private:
    // Processes the parsed HTTP request, determining if it's static or CGI.
    void _processRequest();
};

#endif