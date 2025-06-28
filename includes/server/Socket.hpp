#ifndef SOCKET_HPP
# define SOCKET_HPP

# include "divers.hpp"
# include "../config/ServerStructures.hpp"
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <netdb.h>
# include <unistd.h>

class Socket {
	private:
		int						_sockfd;		// The socket file descriptor.
		socklen_t				_sin_size;		// Size of the socket address structure.
		struct sockaddr_storage	_addr;			// Generic socket address structure.
		std::string				_port;			// The port the socket is bound to.
		const ServerConfig*		_server_block;	// Pointer to the associated server configuration.

	public:
		Socket();
		virtual ~Socket();
		Socket(const Socket& cpy);
		Socket& operator=(const Socket& src);

		void	createSocket(int ai_family, int ai_socktype, int ai_protocol);
		void	bindSocket(struct sockaddr* ai_addr, socklen_t ai_addrlen);
		void	listenOnSocket(void);
		int		acceptConnection(int listenSock);
		void	printConnection(void);
		bool	initListenSocket(const char* port);
		void	closeSocket(void);

		int					getSocketFD(void);
		int					getPort(void);
		const ServerConfig*	getServerBlock(void);

		void	setSocketFD(int fd);
		void	setPortFD(std::string port);
		void	setServerBlock(const ServerConfig* sb);
};

#endif