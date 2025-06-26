#ifndef DIVERS_HPP
# define DIVERS_HPP

// HTTP Status Codes
# define OK 200 // Standard HTTP status code for successful requests.

// Networking Constants
# define BACKLOG 25 // Maximum length of the queue of pending connections for a listening socket.

// Standard Library Includes
# include <typeinfo> // For runtime type information (e.g., typeid).
# include <poll.h> // For the poll() system call and related structures (e.g., pollfd).
# include <iostream> // For input/output operations (e.g., std::cout, std::cerr).


#endif