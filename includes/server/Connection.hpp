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


class Server;

// Represents a single client connection to the server.
class Connection : public Socket {
public:
	enum ConnectionState {
		READING,         // Reading client request.
		HANDLING_CGI,    // Waiting for CGI response.
		WRITING,         // Writing response to client.
		CLOSING          // Connection needs to be closed and reaped.
	};

	Connection(Server* server);
	~Connection();

	void	handleRead();
	void	handleWrite();
	void	executeCGI();
	void	finalizeCGI();

	int	getCgiReadFd() const;
	int	getCgiWriteFd() const;

	ConnectionState	getState() const;
	void			setState(ConnectionState state);
	bool			isCGI() const;

	CGIHandler*	getCgiHandler() const;
	bool		hasActiveCGI() const;

private:
	HttpRequest			_request;		// The parsed HTTP request.
	HttpResponse		_response;		// The HTTP response to be sent.
	HttpRequestParser	_parser;		// Parser for incoming request data.
	std::vector<char>	_requestBuffer;	// Buffer for raw incoming request data.
	ConnectionState		_state;			// Current state of the connection.
	Server*				_server;		// Pointer to the parent server (for callbacks like updateFdEvents).
	CGIHandler*			_cgiHandler;	// Pointer to CGI handler if this is a CGI request.
	bool				_isCgiRequest;	// Flag to indicate if the current request is for CGI.

	std::string			_rawResponseToSend;			// The complete raw HTTP response string.
	size_t				_bytesSentFromRawResponse;	// Number of bytes sent from _rawResponseToSend.

	void	_processRequest();
	void	_resetForNextRequest();
};

#endif