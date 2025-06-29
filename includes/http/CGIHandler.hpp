/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CGIHandler.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bvieilhe <bvieilhe@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/25 17:40:21 by baptistevie       #+#    #+#             */
/*   Updated: 2025/06/28 17:54:10 by bvieilhe         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CGIHANDLER_HPP
# define CGIHANDLER_HPP

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <algorithm>
#include <cstdio>

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "../config/ServerStructures.hpp"
#include "HttpExceptions.hpp"

class Server;

namespace CGIState {
	enum Type {
		NOT_STARTED,
		FORK_FAILED,
		WRITING_INPUT,
		READING_OUTPUT,
		COMPLETE,
		TIMEOUT,
		CGI_PROCESS_ERROR
	};
}

class CGIHandler {
public:
	CGIHandler(const HttpRequest& request,
			const ServerConfig* serverConfig,
			const LocationConfig* locationConfig,
			Server* serverPtr);

	~CGIHandler();

	CGIHandler(const CGIHandler& other); // Updated declaration
	CGIHandler& operator=(const CGIHandler& other);

	bool				start();
	void				handleRead();
	void				handleWrite();
	void				pollCGIProcess();
	int					getReadFd() const;
	int					getWriteFd() const;
	CGIState::Type		getState() const;
	void				setState(CGIState::Type newState);
	bool				isFinished() const;
	const HttpResponse&	getHttpResponse() const;
	pid_t				getCGIPid() const;
	void				setTimeout();
	void				setStartTime();
	bool				checkTimeout() const;

	void cleanup();

private:
	const HttpRequest&		_request;
	const ServerConfig*		_serverConfig;
	const LocationConfig*	_locationConfig;
	Server*					_serverPtr;

	std::string			_cgi_script_path;
	std::string			_cgi_executable_path;
	pid_t				_cgi_pid;
	int					_fd_stdin[2];
	int					_fd_stdout[2];
	std::vector<char>	_cgi_response_buffer;
	HttpResponse		_final_http_response;
	CGIState::Type		_state;
	bool				_cgi_headers_parsed;
	int					_cgi_exit_status;
	time_t				_cgi_start_time;
	bool				_cgi_stdout_eof_received;

	const std::vector<char>*	_request_body_ptr;
	size_t						_request_body_sent_bytes;

	bool	_setNonBlocking(int fd);
	char**	_createCGIEnvironment() const;
	char**	_createCGIArguments() const;
	void	_freeCGICharArrays(char** arr) const;
	void	_closePipes();
	void	_parseCGIOutput();
	bool	_initializeCGIPaths();
};

#endif