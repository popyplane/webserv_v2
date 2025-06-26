#ifndef WEBSERV_HPP
# define WEBSERV_HPP

// Standard C++ Library Includes
# include <iostream> // For input/output operations (e.g., std::cout, std::cerr).
# include <vector> // For dynamic arrays (e.g., std::vector).
# include <map> // For associative arrays (e.g., std::map).
# include <string> // For string manipulation (e.g., std::string).
# include <stdexcept> // For standard exception classes (e.g., std::runtime_error).
# include <cstring> // For C-style string manipulation (e.g., strerror).

// System-Specific Includes for Networking and File Operations
# include <sys/socket.h> // For socket programming (e.g., socket, bind, listen, accept).
# include <netinet/in.h> // For Internet Protocol family addresses (e.g., sockaddr_in).
# include <arpa/inet.h> // For IP address manipulation (e.g., inet_ntop).
# include <unistd.h> // For POSIX operating system API (e.g., read, write, close, fork).
# include <fcntl.h> // For file control options (e.g., fcntl, O_NONBLOCK).
# include <poll.h> // For multiplexing I/O (e.g., poll, pollfd).
# include <errno.h> // For error number definitions (e.g., errno).

// Constants
# define MAXEVENTS 1000 // Maximum number of events to handle in poll().
# define BUFF_SIZE 8192 // Size of the buffer for reading/writing data.

// Project-Specific Class Includes
# include "config/ServerStructures.hpp" // Defines structures for server and location configurations.
# include "server/Server.hpp" // Declares the Server class, main entry point for the web server.
# include "server/divers.hpp" // Miscellaneous definitions or utility functions.
# include "http/HttpRequest.hpp" // Defines the HttpRequest class for parsed HTTP requests.
# include "http/HttpResponse.hpp" // Defines the HttpResponse class for HTTP responses.
# include "server/Uri.hpp" // Defines the Uri class for URI parsing.
# include "server/Socket.hpp" // Defines the Socket class for network socket operations.
# include "server/Connection.hpp" // Defines the Connection class for managing client connections.

#endif