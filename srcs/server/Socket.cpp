#include "../../includes/server/Socket.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>

// Constructor: Initializes a new Socket object.
Socket::Socket() {
}

// Destructor: Cleans up Socket resources.
Socket::~Socket() {
}

// Copy Constructor: Copies the socket file descriptor and size.
Socket::Socket(const Socket	&cpy) {
	this->_sockfd = cpy._sockfd;
	this->_sin_size = cpy._sin_size;
}

// Assignment Operator: Assigns socket file descriptor and size from another Socket.
Socket& Socket::operator=(const Socket	&src) {
	this->_sockfd = src._sockfd;
	this->_sin_size = src._sin_size;
	return (*this);
}

// Helper function: Gets the appropriate IP address structure (IPv4 or IPv6).
static void	*get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa)->sin_addr);
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Creates a new socket and sets options for reuse.
void	Socket::createSocket(int ai_family, int ai_socktype, int ai_protocol) {
	int yes = 1;

	if ((_sockfd = socket(ai_family, ai_socktype, ai_protocol)) < 0)
		throw std::runtime_error("error with socket");
	// Allow reuse of local addresses.
	if (setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0)
		throw std::runtime_error("error with socket opt");
}

// Binds the socket to a specified IP address and port.
void	Socket::bindSocket(struct sockaddr* ai_addr, socklen_t ai_addrlen) {
	if (bind(_sockfd, ai_addr, ai_addrlen) < 0)
		throw std::runtime_error("error with socket bind");
}

// Sets the socket to listen for incoming connections.
void	Socket::listenOnSocket(void) {
	if (listen(_sockfd, BACKLOG) < 0)
		throw std::runtime_error("error with listen socket");
	std::cout << "listen socket : " << _sockfd << std::endl;
}

// Accepts an incoming connection on the listening socket.
void	Socket::acceptConnection(int listenSock) {
	if ((_sockfd = accept(listenSock, (struct sockaddr*)&_addr, &_sin_size)) < 0)
		throw std::runtime_error("error with accept socket");
}

// Prints information about the accepted connection to console.
void	Socket::printConnection(void) {
	char s[INET6_ADDRSTRLEN];

	inet_ntop(_addr.ss_family, get_in_addr((struct sockaddr *)&_addr), s, sizeof(s));
	std::cout << "server received connection from: " << s << std::endl;
}

// Initializes a listening socket by creating, binding, and listening.
void	Socket::initListenSocket(const char* port) {
	struct addrinfo	base;
	struct addrinfo	*ai;
	struct addrinfo *p;

	memset(&base, 0, sizeof(base));
	base.ai_family = AF_INET;
    base.ai_socktype = SOCK_STREAM;
    base.ai_flags = AI_PASSIVE;
	// Get address information for the given port.
	if (getaddrinfo(NULL, port, &base, &ai) != 0)
		throw std::runtime_error("error with init socket");
	// Loop through all results and try to create and bind a socket.
	for (p = ai; p != NULL; p = p->ai_next)
	{
		try {
			createSocket(p->ai_family, p->ai_socktype, p->ai_protocol);
		} catch (std::exception& e) {
			std::cerr << e.what() << std::endl;
			continue ;
		}
		try {
			bindSocket(p->ai_addr, p->ai_addrlen);
		} catch (std::exception& e) {
            close(_sockfd);
			std::cerr << e.what() << std::endl;
			continue ;
		}
		break ;
	}
	freeaddrinfo(ai);
	// If no socket was successfully bound, throw an error.
	if (p == NULL)
		throw std::runtime_error("error with socket bind");
	listenOnSocket();
}

// Closes the socket file descriptor.
void    Socket::closeSocket(void) {
    int n;

    n = close(this->getSocketFD());
    if (n < 0)
        throw std::runtime_error("Error with socket close");
    std::cout << "SOCKET " << this->getSocketFD() << " CLOSED" << std::endl;
}

// Returns the socket file descriptor.
int		Socket::getSocketFD(void) {
	return (this->_sockfd);
}

// Returns a pointer to the associated ServerConfig block.
ServerConfig*    Socket::getServerBlock(void) {
    return (_server_block);
}

// Sets the socket file descriptor.
void    Socket::setSocketFD(int fd) {
    if (fd < 0)
        throw std::runtime_error("Error fd incorrect");
    this->_sockfd = fd;
}

// Sets the port string for the socket.
void    Socket::setPortFD(std::string port) {
    this->_port = port;
}

// Sets the associated ServerConfig block.
void    Socket::setServerBlock(ServerConfig* sb) {
    this->_server_block = sb;
}