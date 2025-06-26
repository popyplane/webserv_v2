#ifndef SOCKET_HPP
# define SOCKET_HPP

# include "divers.hpp"
# include "../config/ServerStructures.hpp"
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <netdb.h>
# include <unistd.h>

// Represents a network socket for server communication.
class Socket {
	private:
		int						_sockfd; // The socket file descriptor.
		socklen_t				_sin_size; // Size of the socket address structure.
		struct sockaddr_storage	_addr; // Generic socket address structure.
        std::string             _port; // The port the socket is bound to.
        ServerConfig*            _server_block; // Pointer to the associated server configuration.

	public:
		// Constructor: Initializes a new Socket object.
		Socket();
		// Destructor: Cleans up Socket resources.
		virtual ~Socket();
		// Copy Constructor: Copies the socket file descriptor and size.
		Socket(const Socket& cpy);
		// Assignment Operator: Assigns socket file descriptor and size from another Socket.
		Socket& operator=(const Socket& src);

		// Creates a new socket.
		void	createSocket(int ai_family, int ai_socktype, int ai_protocol);
		// Binds the socket to a specified IP address and port.
		void	bindSocket(struct sockaddr* ai_addr, socklen_t ai_addrlen);
		// Sets the socket to listen for incoming connections.
		void	listenOnSocket(void);
		// Accepts an incoming connection on the listening socket.
		void	acceptConnection(int listenSock);
		// Prints information about the accepted connection to console.
		void	printConnection(void);
		// Initializes a listening socket by creating, binding, and listening.
		void	initListenSocket(const char* port);
        // Closes the socket file descriptor.
        void    closeSocket(void);

		// Returns the socket file descriptor.
		int		        getSocketFD(void);
        // Returns the port number as an integer.
        int             getPort(void);
        // Returns a pointer to the associated ServerConfig block.
        ServerConfig*    getServerBlock(void);

        // Sets the socket file descriptor.
        void    setSocketFD(int fd);
        // Sets the port string for the socket.
        void    setPortFD(std::string port);
        // Sets the associated ServerConfig block.
        void    setServerBlock(ServerConfig* sb);
};

#endif