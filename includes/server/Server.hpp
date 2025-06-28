
#ifndef SERVER_HPP
# define SERVER_HPP

# include "../config/ServerStructures.hpp"
# include "Socket.hpp"
# include "Connection.hpp"
# include "divers.hpp"

# include <vector>
# include <map>
# include <stdexcept>
# include <poll.h>

class Connection;

class Server {
private:
	std::vector<ServerConfig>	_serverConfigs;
	std::map<int, Socket*>		_listenSockets;
	std::vector<struct pollfd>	_pfds;
	std::map<int, Connection*>	_connections;
	std::map<int, Connection*>	_cgiFdsToConnection;

	bool	_running;
	int		_timeout_ms;

	bool	_setupListeners();
	void	_acceptNewConnection(int listen_fd);
	void	_handleClientEvent(int client_fd, short revents);
	void	_handleCgiEvent(int cgi_fd, short revents);
	void	_reapClosedConnections();

public:
	Server(const std::vector<ServerConfig>& configs);
	~Server();

	void run();

	const std::vector<ServerConfig>&	getConfigs() const;
	void	updateFdEvents(int fd, short events);
	void	_addFdToPoll(int fd, short events);
	void	_removeFdFromPoll(int fd);

	void	registerCgiFd(int fd, Connection* conn, short events);
	void	unregisterCgiFd(int fd);
};

#endif
