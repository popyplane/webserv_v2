/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CGIHandler.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: baptistevieilhescaze <baptistevieilhesc    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/25 17:47:11 by baptistevie       #+#    #+#             */
/*   Updated: 2025/06/29 04:11:27 by baptistevie      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <signal.h>
#include "../../includes/webserv.hpp" // Brings in all necessary headers and constants

// Constructor: Initializes CGIHandler with request and configuration details.
CGIHandler::CGIHandler(const HttpRequest& request,
					   const ServerConfig* serverConfig,
					   const LocationConfig* locationConfig,
					   Server* serverPtr)
	: _request(request),
	  _serverConfig(serverConfig),
	  _locationConfig(locationConfig),
	  _serverPtr(serverPtr),
	  _cgi_script_path(),
	  _cgi_executable_path(),
	  _cgi_pid(-1),
	  _fd_stdin(), // Default construction
	  _fd_stdout(), // Default construction
	  _cgi_response_buffer(),
	  _final_http_response(),
	  _state(CGIState::NOT_STARTED),
	  _cgi_headers_parsed(false),
	  _cgi_exit_status(-1),
	  _cgi_start_time(0),
	  _cgi_stdout_eof_received(false),
	  _request_body_ptr(&request.body),
	  _request_body_sent_bytes(0)
{
	_fd_stdin[0] = -1;
	_fd_stdin[1] = -1;
	_fd_stdout[0] = -1;
	_fd_stdout[1] = -1;

	// Call the new helper method to initialize paths
	if (!_initializeCGIPaths()) {
		// If initialization fails, the state is already set to CGI_PROCESS_ERROR
		// and constructor can just return.
		return;
	}

	// Set _request_body_ptr to NULL if the request body is empty
	if (request.body.empty()) {
		_request_body_ptr = NULL;
	}
}

// Destructor: Cleans up any child processes. Pipe FDs are closed by Connection/Server.
CGIHandler::~CGIHandler() {
	if (_cgi_pid != -1) {
		int status;
		pid_t result = waitpid(_cgi_pid, &status, WNOHANG);
		if (result == 0) {
			std::cerr << "WARNING: CGI child process " << _cgi_pid << " still running in CGIHandler destructor, sending SIGTERM." << std::endl;
			kill(_cgi_pid, SIGTERM);
			waitpid(_cgi_pid, &status, 0);
		}
		_cgi_pid = -1;
	}
}

// Copy Constructor: Disallows copying to prevent issues with file descriptors and PIDs.
CGIHandler::CGIHandler(const CGIHandler& other)
    : _request(other._request),
      _serverConfig(other._serverConfig),
      _locationConfig(other._locationConfig),
      _serverPtr(other._serverPtr),
      _cgi_script_path(other._cgi_script_path),
      _cgi_executable_path(other._cgi_executable_path),
      _cgi_pid(-1),
      _fd_stdin(),
      _fd_stdout(),
      _cgi_response_buffer(),
      _final_http_response(),
      _state(CGIState::NOT_STARTED),
      _cgi_headers_parsed(false),
      _cgi_exit_status(-1),
      _cgi_start_time(0),
      _cgi_stdout_eof_received(false),
      _request_body_ptr(other._request_body_ptr),
      _request_body_sent_bytes(0)
{
    _fd_stdin[0] = -1; _fd_stdin[1] = -1;
    _fd_stdout[0] = -1; _fd_stdout[1] = -1;
}

// Assignment Operator: Disallows assignment to prevent issues with file descriptors and PIDs.
CGIHandler& CGIHandler::operator=(const CGIHandler& other) {
	if (this != &other) {
		if (_cgi_pid != -1) {
			kill(_cgi_pid, SIGTERM);
			waitpid(_cgi_pid, NULL, 0);
			_cgi_pid = -1;
		}

		_serverConfig = other._serverConfig;
		_locationConfig = other._locationConfig;
		_request_body_ptr = other._request_body_ptr;
		_cgi_script_path = other._cgi_script_path;
		_cgi_executable_path = other._cgi_executable_path;

		_cgi_pid = -1;
		_fd_stdin[0] = -1; _fd_stdin[1] = -1;
		_fd_stdout[0] = -1; _fd_stdout[1] = -1;
		_request_body_sent_bytes = 0;
		_cgi_response_buffer.clear();
		_final_http_response = HttpResponse();
		_state = CGIState::NOT_STARTED;
		_cgi_headers_parsed = false;
		_cgi_exit_status = -1;
		_cgi_start_time = 0;
	}
	return *this;
}

// Private helper to determine CGI script and executable paths.
bool CGIHandler::_initializeCGIPaths() {
	if (!_locationConfig || _locationConfig->root.empty() || _locationConfig->cgiExecutables.empty()) {
		std::cerr << "ERROR: CGIHandler: Incomplete location config for CGI setup (root or cgiExecutables empty)." << std::endl;
		_state = CGIState::CGI_PROCESS_ERROR;
		return false;
	}

	std::string document_root_relative = _locationConfig->root;

	char abs_path_buffer[PATH_MAX];
	if (realpath(document_root_relative.c_str(), abs_path_buffer) == NULL) {
		std::cerr << "ERROR: CGIHandler constructor: Failed to get absolute path for document root: " << document_root_relative << ". Setting CGI_PROCESS_ERROR state." << std::endl;
		_state = CGIState::CGI_PROCESS_ERROR;
		return false;
	}
	std::string absoluteDocumentRoot = abs_path_buffer;

	// Ensure no trailing slash for consistent path concatenation
	if (absoluteDocumentRoot.length() > 1 && absoluteDocumentRoot[absoluteDocumentRoot.length() - 1] == '/') {
		absoluteDocumentRoot = absoluteDocumentRoot.substr(0, absoluteDocumentRoot.length() - 1);
	}

	size_t dot_pos = _request.path.rfind('.');
	if (dot_pos == std::string::npos) {
		std::cerr << "ERROR: CGIHandler: No file extension found in URI for CGI: " << _request.path << std::endl;
		_state = CGIState::CGI_PROCESS_ERROR;
		return false;
	}
	std::string file_extension = _request.path.substr(dot_pos);

	std::map<std::string, std::string>::const_iterator cgi_it = _locationConfig->cgiExecutables.find(file_extension);
	if (cgi_it == _locationConfig->cgiExecutables.end()) {
		std::cerr << "ERROR: CGIHandler: No CGI executable configured for extension: " << file_extension << " in location: " << _locationConfig->path << std::endl;
		_state = CGIState::CGI_PROCESS_ERROR;
		return false;
	}
	_cgi_executable_path = cgi_it->second;

	std::string normalizedRequestPath = _request.path;
	// Ensure request path starts with a slash if it doesn't already
	if (!normalizedRequestPath.empty() && normalizedRequestPath[0] != '/') {
		normalizedRequestPath = "/" + normalizedRequestPath;
	}
	_cgi_script_path = absoluteDocumentRoot + normalizedRequestPath;

	return true;
}

// Sets a file descriptor to non-blocking mode.
bool CGIHandler::_setNonBlocking(int fd) {
	if (fd < 0) return false;
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) {
		std::cerr << "ERROR: fcntl F_GETFL failed for FD " << fd << ". Setting CGI_PROCESS_ERROR state." << std::endl;
		_state = CGIState::CGI_PROCESS_ERROR;
		return false;
	}
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		std::cerr << "ERROR: fcntl F_SETFL O_NONBLOCK failed for FD " << fd << ". Setting CGI_PROCESS_ERROR state." << std::endl;
		_state = CGIState::CGI_PROCESS_ERROR;
		return false;
	}
	return true;
}

// Creates environment variables for CGI execution.
char** CGIHandler::_createCGIEnvironment() const {
	std::vector<std::string> env_vars_vec;

	env_vars_vec.push_back("REQUEST_METHOD=" + _request.method);
	env_vars_vec.push_back("SERVER_PROTOCOL=" + _request.protocolVersion);
	env_vars_vec.push_back("REDIRECT_STATUS=200");

	if (_serverConfig) {
		if (!_serverConfig->serverNames.empty()) {
			env_vars_vec.push_back("SERVER_NAME=" + _serverConfig->serverNames[0]);
		} else {
			env_vars_vec.push_back("SERVER_NAME=localhost");
		}
		env_vars_vec.push_back("SERVER_PORT=" + StringUtils::longToString(_serverConfig->port));
	} else {
		env_vars_vec.push_back("SERVER_NAME=unknown");
		env_vars_vec.push_back("SERVER_PORT=80");
	}

	env_vars_vec.push_back("SCRIPT_FILENAME=" + _cgi_script_path);

	std::string script_name = _request.path;
	if (script_name.empty() || script_name[0] != '/') {
		script_name = "/" + script_name;
	}
	env_vars_vec.push_back("SCRIPT_NAME=" + script_name);

	std::string path_info;
	size_t script_path_len_in_uri = script_name.length();
	size_t request_uri_path_only_len = _request.uri.find('?');
	if (request_uri_path_only_len == std::string::npos) {
		request_uri_path_only_len = _request.uri.length();
	}

	if (request_uri_path_only_len > script_path_len_in_uri) {
		path_info = _request.uri.substr(script_path_len_in_uri, request_uri_path_only_len - script_path_len_in_uri);
	}
	env_vars_vec.push_back("PATH_INFO=" + path_info);

	env_vars_vec.push_back("REQUEST_URI=" + _request.uri);

	size_t query_pos = _request.uri.find('?');
	if (query_pos != std::string::npos) {
		env_vars_vec.push_back("QUERY_STRING=" + _request.uri.substr(query_pos + 1));
	} else {
		env_vars_vec.push_back("QUERY_STRING=");
	}

	if (_request.method == "POST") {
		std::map<std::string, std::string>::const_iterator it_type = _request.headers.find("content-type");
		if (it_type != _request.headers.end()) {
			env_vars_vec.push_back("CONTENT_TYPE=" + it_type->second);
		} else {
			env_vars_vec.push_back("CONTENT_TYPE=");
		}

		std::map<std::string, std::string>::const_iterator it_len = _request.headers.find("content-length");
		if (it_len != _request.headers.end()) {
			env_vars_vec.push_back("CONTENT_LENGTH=" + it_len->second);
		} else {
			if (_request_body_ptr) {
				 env_vars_vec.push_back("CONTENT_LENGTH=" + StringUtils::longToString(_request_body_ptr->size()));
			} else {
				 env_vars_vec.push_back("CONTENT_LENGTH=0");
			}
		}
	} else {
		env_vars_vec.push_back("CONTENT_TYPE=");
		env_vars_vec.push_back("CONTENT_LENGTH=");
	}

	std::string document_root_env;
	std::string document_root_relative_for_env;
	if (_locationConfig && !_locationConfig->root.empty()) {
		document_root_relative_for_env = _locationConfig->root;
	} else if (_serverConfig && !_serverConfig->root.empty()) {
		document_root_relative_for_env = _serverConfig->root;
	} else {
		document_root_relative_for_env = "./";
	}

	char abs_doc_root_buffer[PATH_MAX];
	if (realpath(document_root_relative_for_env.c_str(), abs_doc_root_buffer) == NULL) {
		std::cerr << "WARNING: Failed to get absolute path for DOCUMENT_ROOT: " << document_root_relative_for_env << ". Using relative path." << std::endl;
		document_root_env = document_root_relative_for_env;
	} else {
		document_root_env = abs_doc_root_buffer;
	}
	if (document_root_env.length() > 1 && document_root_env[document_root_env.length() - 1] == '/') {
		document_root_env = document_root_env.substr(0, document_root_env.length() - 1);
	}
	env_vars_vec.push_back("DOCUMENT_ROOT=" + document_root_env);

	for (std::map<std::string, std::string>::const_iterator it = _request.headers.begin(); it != _request.headers.end(); ++it) {
		std::string header_name = it->first;
		if (StringUtils::ciCompare(header_name, "content-type") || StringUtils::ciCompare(header_name, "content-length") || StringUtils::ciCompare(header_name, "host")) {
			continue;
		}
		std::transform(header_name.begin(), header_name.end(), header_name.begin(), static_cast<int(*)(int)>(std::toupper));
		for (size_t i = 0; i < header_name.length(); ++i) {
			if (header_name[i] == '-') {
				header_name[i] = '_';
			}
		}
		env_vars_vec.push_back("HTTP_" + header_name + "=" + it->second);
	}

	env_vars_vec.push_back("REMOTE_ADDR=127.0.0.1");
	env_vars_vec.push_back("REMOTE_PORT=8080");

	char** envp = new char*[env_vars_vec.size() + 1];
	for (size_t i = 0; i < env_vars_vec.size(); ++i) {
		envp[i] = new char[env_vars_vec[i].length() + 1];
		strcpy(envp[i], env_vars_vec[i].c_str());
	}
	envp[env_vars_vec.size()] = NULL;
	return envp;
}

// Creates argument list for execve.
char** CGIHandler::_createCGIArguments() const {
	char** argv = new char*[3];
	argv[0] = new char[_cgi_executable_path.length() + 1];
	strcpy(argv[0], _cgi_executable_path.c_str());

	argv[1] = new char[_cgi_script_path.length() + 1];
	strcpy(argv[1], _cgi_script_path.c_str());

	argv[2] = NULL;
	return argv;
}

// Frees char** arrays (for envp and argv).
void CGIHandler::_freeCGICharArrays(char** arr) const {
	if (arr) {
		for (int i = 0; arr[i] != NULL; ++i) {
			delete[] arr[i];
		}
		delete[] arr;
	}
}

// Cleans up CGI related file descriptors.
void CGIHandler::_closePipes() {
	if (_fd_stdin[0] != -1) {
		close(_fd_stdin[0]);
		_fd_stdin[0] = -1;
	}
	if (_fd_stdin[1] != -1) {
		close(_fd_stdin[1]);
		_fd_stdin[1] = -1;
	}
	if (_fd_stdout[0] != -1) {
		close(_fd_stdout[0]);
		_fd_stdout[0] = -1;
	}
	if (_fd_stdout[1] != -1) {
		close(_fd_stdout[1]);
		_fd_stdout[1] = -1;
	}
}

// Initiates the CGI process (fork, pipe, execve).
bool CGIHandler::start() {
	std::cout << "DEBUG: CGIHandler::start() called." << std::endl;
	std::cout << "DEBUG: Initial FDs: stdin[0]=\t" << _fd_stdin[0] << ", stdin[1]=\t" << _fd_stdin[1]
		<< ", stdout[0]=\t" << _fd_stdout[0] << ", stdout[1]=\t" << _fd_stdout[1] << std::endl;

	if (_state != CGIState::NOT_STARTED) {
		std::cerr << "ERROR: CGI process already started or in an invalid state (" << _state << ")." << std::endl;
		return false;
	}

	if (_cgi_script_path.empty() || _cgi_executable_path.empty()) {
		std::cerr << "ERROR: CGIHandler: Script or executable path not properly initialized (empty)." << std::endl;
		_state = CGIState::CGI_PROCESS_ERROR;
		return false;
	}

	// Initialize FDs to -1 before pipe calls
	_fd_stdin[0] = -1; _fd_stdin[1] = -1;
	_fd_stdout[0] = -1; _fd_stdout[1] = -1;

	std::cout << "DEBUG: Before pipe() calls: stdin[0]=\t" << _fd_stdin[0] << ", stdin[1]=\t" << _fd_stdin[1]
		<< ", stdout[0]=\t" << _fd_stdout[0] << ", stdout[1]=\t" << _fd_stdout[1] << std::endl;

	if (pipe(_fd_stdin) == -1) {
		std::cerr << "ERROR: Failed to create stdin pipe." << std::endl;
		_state = CGIState::FORK_FAILED;
		return false;
	}
	std::cout << "DEBUG: After stdin pipe(): stdin[0]=\t" << _fd_stdin[0] << ", stdin[1]=\t" << _fd_stdin[1]
		<< ", stdout[0]=\t" << _fd_stdout[0] << ", stdout[1]=\t" << _fd_stdout[1] << std::endl;

	if (pipe(_fd_stdout) == -1) {
		std::cerr << "ERROR: Failed to create stdout pipe." << std::endl;
		_closePipes(); // Close stdin pipes if stdout pipe creation fails
		_state = CGIState::FORK_FAILED;
		return false;
	}
	std::cout << "DEBUG: After stdout pipe(): stdin[0]=\t" << _fd_stdin[0] << ", stdin[1]=\t" << _fd_stdin[1]
		<< ", stdout[0]=\t" << _fd_stdout[0] << ", stdout[1]=\t" << _fd_stdout[1] << std::endl;

	// Set FD_CLOEXEC on all pipe ends in the parent
	if (fcntl(_fd_stdin[0], F_SETFD, FD_CLOEXEC) == -1 ||
		fcntl(_fd_stdin[1], F_SETFD, FD_CLOEXEC) == -1 ||
		fcntl(_fd_stdout[0], F_SETFD, FD_CLOEXEC) == -1 ||
		fcntl(_fd_stdout[1], F_SETFD, FD_CLOEXEC) == -1) {
		std::cerr << "ERROR: Failed to set FD_CLOEXEC on CGI pipes." << std::endl;
		_closePipes();
		_state = CGIState::FORK_FAILED;
		return false;
	}
	std::cout << "DEBUG: After fcntl(FD_CLOEXEC): stdin[0]=\t" << _fd_stdin[0] << ", stdin[1]=\t" << _fd_stdin[1]
		<< ", stdout[0]=\t" << _fd_stdout[0] << ", stdout[1]=\t" << _fd_stdout[1] << std::endl;

	if (!_setNonBlocking(_fd_stdin[1]) || !_setNonBlocking(_fd_stdout[0])) {
		_closePipes(); // Close all pipes if setting non-blocking fails
		return false;
	}
	std::cout << "DEBUG: After setNonBlocking(): stdin[0]=\t" << _fd_stdin[0] << ", stdin[1]=\t" << _fd_stdin[1]
		<< ", stdout[0]=\t" << _fd_stdout[0] << ", stdout[1]=\t" << _fd_stdout[1] << std::endl;

	_cgi_pid = fork();
	if (_cgi_pid == -1) {
		std::cerr << "ERROR: Failed to fork CGI process." << std::endl;
		_closePipes(); // Close all pipes if fork fails
		_state = CGIState::FORK_FAILED;
		return false;
	}

	if (_cgi_pid == 0) { // Child process.
		std::cout << "DEBUG: CGI child: Entered child process." << std::endl;
		std::cout << "DEBUG: CGI child: FDs before closing parent ends: stdin[0]=\t" << _fd_stdin[0] << ", stdin[1]=\t" << _fd_stdin[1]
		<< ", stdout[0]=\t" << _fd_stdout[0] << ", stdout[1]=\t" << _fd_stdout[1] << std::endl;

		close(_fd_stdin[1]); // Close parent's write end of stdin pipe
		_fd_stdin[1] = -1; // Mark as closed
		close(_fd_stdout[0]); // Close parent's read end of stdout pipe
		_fd_stdout[0] = -1; // Mark as closed
		std::cout << "DEBUG: CGI child: FDs after closing parent ends: stdin[0]=\t" << _fd_stdin[0] << ", stdin[1]=\t" << _fd_stdin[1]
		<< ", stdout[0]=\t" << _fd_stdout[0] << ", stdout[1]=\t" << _fd_stdout[1] << std::endl;

		if (dup2(_fd_stdin[0], STDIN_FILENO) == -1) {
			std::cerr << "ERROR: dup2 STDIN_FILENO failed in CGI child. " << strerror(errno) << ". Exiting." << std::endl;
			_exit(EXIT_FAILURE);
		}
		if (dup2(_fd_stdout[1], STDOUT_FILENO) == -1) {
			std::cerr << "ERROR: dup2 STDOUT_FILENO failed in CGI child. " << strerror(errno) << ". Exiting." << std::endl;
			_exit(EXIT_FAILURE);
		}
		std::cout << "DEBUG: CGI child: FDs after dup2: stdin[0]=\t" << _fd_stdin[0] << ", stdin[1]=\t" << _fd_stdin[1]
		<< ", stdout[0]=\t" << _fd_stdout[0] << ", stdout[1]=\t" << _fd_stdout[1] << std::endl;

		close(_fd_stdin[0]); // Close child's read end of stdin pipe
		_fd_stdin[0] = -1; // Mark as closed
		close(_fd_stdout[1]); // Close child's write end of stdout pipe
		_fd_stdout[1] = -1; // Mark as closed
		std::cout << "DEBUG: CGI child: FDs after closing child ends: stdin[0]=\t" << _fd_stdin[0] << ", stdin[1]=\t" << _fd_stdin[1]
		<< ", stdout[0]=\t" << _fd_stdout[0] << ", stdout[1]=\t" << _fd_stdout[1] << std::endl;

		std::string cgi_working_dir_relative;
		if (_locationConfig && !_locationConfig->root.empty()) {
			cgi_working_dir_relative = _locationConfig->root;
		} else if (_serverConfig && !_serverConfig->root.empty()) {
			cgi_working_dir_relative = _serverConfig->root;
		} else {
			cgi_working_dir_relative = "./";
		}

		char abs_chdir_path[PATH_MAX];
		if (realpath(cgi_working_dir_relative.c_str(), abs_chdir_path) == NULL) {
			std::cerr << "ERROR: CGI child: Failed to get absolute path for chdir target '" << cgi_working_dir_relative << "'. " << strerror(errno) << ". Exiting." << std::endl;
			_exit(EXIT_FAILURE);
		}
		std::string cgi_working_dir_absolute = abs_chdir_path;

		if (chdir(cgi_working_dir_absolute.c_str()) == -1) {
			std::cerr << "ERROR: chdir failed in CGI child to " << cgi_working_dir_absolute << ". " << strerror(errno) << ". Exiting." << std::endl;
			_exit(EXIT_FAILURE);
		}

		// Check if executable exists and is executable
		if (access(_cgi_executable_path.c_str(), X_OK) == -1) {
			std::cerr << "ERROR: CGI child: Executable not found or not executable: " << _cgi_executable_path << ". " << strerror(errno) << ". Exiting." << std::endl;
			_exit(EXIT_FAILURE);
		}

		// Check if script exists and is readable
		if (access(_cgi_script_path.c_str(), F_OK | R_OK) == -1) {
			std::cerr << "ERROR: CGI child: Script not found or not readable: " << _cgi_script_path << ". " << strerror(errno) << ". Exiting." << std::endl;
			_exit(EXIT_FAILURE);
		}

		char** envp = _createCGIEnvironment();
		char** argv = _createCGIArguments();

		execve(_cgi_executable_path.c_str(), argv, envp);

		// If execve fails, this code will be executed
		std::cerr << "ERROR: execve failed for CGI: " << _cgi_executable_path << ". " << strerror(errno) << ". Exiting." << std::endl;
		_freeCGICharArrays(envp);
		_freeCGICharArrays(argv);
		_exit(EXIT_FAILURE);
	} else { // Parent process.
		std::cout << "DEBUG: CGI parent: FDs before closing child ends: stdin[0]=\t" << _fd_stdin[0] << ", stdin[1]=\t" << _fd_stdin[1]
		<< ", stdout[0]=\t" << _fd_stdout[0] << ", stdout[1]=\t" << _fd_stdout[1] << std::endl;
		close(_fd_stdin[0]); // Close child's read end in parent
		_fd_stdin[0] = -1; // Mark as closed
		close(_fd_stdout[1]); // Close child's write end in parent
		_fd_stdout[1] = -1; // Mark as closed

		// If it's a GET request, we don't need to write to CGI stdin, so close the parent's write end.
		if (_request.method != "POST" || !_request_body_ptr || _request_body_ptr->empty()) {
			if (_fd_stdin[1] != -1) {
				close(_fd_stdin[1]);
				_fd_stdin[1] = -1;
				std::cout << "DEBUG: CGI parent: Closed _fd_stdin[1] for non-POST request." << std::endl;
			}
			_state = CGIState::READING_OUTPUT;
		} else {
			_state = CGIState::WRITING_INPUT;
		}
		setStartTime(); // Record start time in parent
		std::cout << "DEBUG: CGI parent: FDs after closing child ends: stdin[0]=\t" << _fd_stdin[0] << ", stdin[1]=\t" << _fd_stdin[1]
		<< ", stdout[0]=\t" << _fd_stdout[0] << ", stdout[1]=\t" << _fd_stdout[1] << std::endl;
	}
	return true;
}

// Returns the read file descriptor for the CGI's stdout pipe (parent's read end).
int CGIHandler::getReadFd() const {
	return _fd_stdout[0];
}

// Returns the write file descriptor for the CGI's stdin pipe (parent's write end).
int CGIHandler::getWriteFd() const {
	return _fd_stdin[1];
}

// Handles incoming data from the CGI's stdout pipe.
void CGIHandler::handleRead() {
	if (_state != CGIState::READING_OUTPUT && _state != CGIState::WRITING_INPUT) {
		std::cerr << "WARNING: CGIHandler::handleRead called in unexpected state: " << _state << std::endl;
		return;
	}

	if (_fd_stdout[0] == -1 || _fd_stdout[0] == -2) {
		return;
	}

	char buffer[BUFF_SIZE];
	ssize_t bytes_read = read(_fd_stdout[0], buffer, sizeof(buffer));

	

	if (bytes_read > 0) {
		_cgi_response_buffer.insert(_cgi_response_buffer.end(), buffer, buffer + bytes_read);
	} else if (bytes_read == 0) { // EOF received
		_cgi_stdout_eof_received = true;
	} else { // bytes_read == -1
		std::cerr << "ERROR: CGIHandler::handleRead: Error reading from CGI stdout pipe (FD: " << _fd_stdout[0] << "). Throwing 500." << std::endl;
		throw Http500Exception("Error reading from CGI stdout pipe.");
	}
}

// Handles sending data to the CGI's stdin pipe (for POST requests).
void CGIHandler::handleWrite() {
	if (_state != CGIState::WRITING_INPUT) {
		std::cerr << "WARNING: CGIHandler::handleWrite called in unexpected state: " << _state << std::endl;
		return;
	}

	if (_fd_stdin[1] == -1 || _fd_stdin[1] == -2) {
		_state = CGIState::READING_OUTPUT;
		return;
	}

	if (!_request_body_ptr || _request_body_ptr->empty()) {
		if (_fd_stdin[1] != -1) {
			_fd_stdin[1] = -2;
		}
		_state = CGIState::READING_OUTPUT;
		return;
	}

	size_t remaining_bytes = _request_body_ptr->size() - _request_body_sent_bytes;
	if (remaining_bytes == 0) {
		if (_fd_stdin[1] != -1) {
			_fd_stdin[1] = -2;
		}
		_state = CGIState::READING_OUTPUT;
		return;
	}

	ssize_t bytes_written = write(_fd_stdin[1],
								  &(*_request_body_ptr)[_request_body_sent_bytes],
								  remaining_bytes);

	if (bytes_written > 0) {
		_request_body_sent_bytes += bytes_written;

		if (_request_body_sent_bytes == _request_body_ptr->size()) {
			if (_fd_stdin[1] != -1) {
				_fd_stdin[1] = -2;
			}
			_state = CGIState::READING_OUTPUT;
		}
	} else { // bytes_written <= 0
		if (bytes_written == 0) {
			// This case means write() returned 0 bytes.
			// For non-blocking, this implies the pipe is full or not ready.
			// It's not an error that should terminate the CGI process.
			// Do nothing, just return. The poll loop will re-poll for POLLOUT.
		} else { // bytes_written == -1
			std::cerr << "ERROR: CGIHandler::handleWrite: Fatal error writing to CGI stdin pipe (FD: " << _fd_stdin[1] << "). Throwing 500." << std::endl;
			throw Http500Exception("Fatal error writing to CGI stdin pipe.");
		}
	}
}

// Checks the status of the CGI child process (non-blocking waitpid).
void CGIHandler::pollCGIProcess() {
    if (_cgi_pid != -1 && !isFinished()) {
        int status = 0; // Initialize status to 0
        pid_t result = waitpid(_cgi_pid, &status, WNOHANG);

        if (result == _cgi_pid) {
            std::cout << "DEBUG: CGIHandler::pollCGIProcess: Child process " << _cgi_pid << " has exited." << std::endl;
            if (WIFEXITED(status)) {
                _cgi_exit_status = WEXITSTATUS(status);
                std::cout << "DEBUG: CGI process exited with status: " << _cgi_exit_status << std::endl;
            } else if (WIFSIGNALED(status)) {
                _cgi_exit_status = WTERMSIG(status);
                std::cerr << "ERROR: CGI process terminated by signal: " << _cgi_exit_status << std::endl;
                _state = CGIState::CGI_PROCESS_ERROR;
            } else {
                // Process ended abnormally (e.g., stopped, continued, core dump)
                std::cerr << "ERROR: CGI process ended abnormally (not exited/signaled). Status: " << status << std::endl;
                _cgi_exit_status = -2; // Custom value to indicate abnormal termination
                _state = CGIState::CGI_PROCESS_ERROR;
            }

            // If CGI process exited, ensure all output is read before parsing
            while (!_cgi_stdout_eof_received) {
                std::cout << "DEBUG: CGI process exited, but EOF not yet received on stdout. Attempting final read to drain pipe." << std::endl;
                handleRead(); // This will set _cgi_stdout_eof_received to true on EOF
                if (_cgi_stdout_eof_received) {
                    std::cout << "DEBUG: Final read successful, EOF received." << std::endl;
                    break;
                }
            }

            if (!_cgi_headers_parsed) {
                std::cerr << "DEBUG: CGI child: Calling _parseCGIOutput()." << std::endl;
                _parseCGIOutput();
            }

            if (_state != CGIState::CGI_PROCESS_ERROR && _state != CGIState::TIMEOUT && _state != CGIState::COMPLETE) {
                _state = CGIState::COMPLETE;
            }
        } else if (result == -1) { // waitpid itself failed
            std::cerr << "ERROR: waitpid failed for CGI process " << _cgi_pid << "." << std::endl;
            _state = CGIState::CGI_PROCESS_ERROR;
        }
    }
}

// Returns the current state of the CGI execution.
CGIState::Type CGIHandler::getState() const {
	return _state;
}

// Sets the state of the CGI execution.
void CGIHandler::setState(CGIState::Type newState) {
	_state = newState;
}

// Checks if the CGI execution has completed and its response is ready.
bool CGIHandler::isFinished() const {
	return _state == CGIState::COMPLETE || _state == CGIState::TIMEOUT ||
		   _state == CGIState::CGI_PROCESS_ERROR || _state == CGIState::FORK_FAILED;
}

// Returns the HTTP response generated from the CGI output.
const HttpResponse& CGIHandler::getHttpResponse() const {
	return _final_http_response;
}

// Returns the PID of the forked CGI process.
pid_t CGIHandler::getCGIPid() const {
	return _cgi_pid;
}

// Records the start time of the CGI process.
void CGIHandler::setStartTime() {
	_cgi_start_time = time(NULL);
}

// Checks if the CGI process has exceeded its timeout.
bool CGIHandler::checkTimeout() const {
	if (_state == CGIState::COMPLETE || _state == CGIState::TIMEOUT || _state == CGIState::CGI_PROCESS_ERROR) {
		return false; // Already finished or in an error state
	}
	if (_cgi_start_time == 0) {
		return false; // Not started yet
	}
	return (time(NULL) - _cgi_start_time) > CGI_TIMEOUT_SECONDS;
}

// Sets a flag to indicate if a timeout has occurred.
void CGIHandler::setTimeout() {
	if (isFinished()) return;

	std::cerr << "WARNING: CGI process " << _cgi_pid << " timed out." << std::endl;
	_state = CGIState::TIMEOUT;
	if (_cgi_pid != -1) {
		kill(_cgi_pid, SIGTERM);
		waitpid(_cgi_pid, NULL, WNOHANG);
	}

	_final_http_response.setStatus(504);
	_final_http_response.addHeader("Content-Type", "text/html");
	_final_http_response.setBody("<html><body><h1>504 Gateway Timeout</h1><p>The CGI script did not respond in time.</p></body></html>");
}

// Parses the CGI's raw output into headers and body.
void CGIHandler::_parseCGIOutput() {
	if (_cgi_headers_parsed) {
		return;
	}

	std::string raw_output(_cgi_response_buffer.begin(), _cgi_response_buffer.end());

	size_t header_end_pos = raw_output.find("\r\n\r\n");
	bool crlf_crlf_used = true;
	if (header_end_pos == std::string::npos) {
		header_end_pos = raw_output.find("\n\n");
		crlf_crlf_used = false;
	}

	if (header_end_pos == std::string::npos) {
		std::cerr << "ERROR: CGI output did not contain valid HTTP header termination (no double CRLF/LF found). Assuming full output is body or malformed." << std::endl;
		_final_http_response.setStatus(500);
		_final_http_response.addHeader("Content-Type", "text/plain");
		_final_http_response.setBody("Internal Server Error: Malformed CGI output (no header termination).\nRaw output:\n" + raw_output);
		_cgi_headers_parsed = true;
		_state = CGIState::CGI_PROCESS_ERROR;
		return;
	}

	std::string headers_str = raw_output.substr(0, header_end_pos);
	std::string body_str = raw_output.substr(header_end_pos + (crlf_crlf_used ? 4 : 2));

	std::istringstream iss_headers(headers_str);
	std::string line;
	int status_code = 200;
	bool content_type_set = false;

	while (std::getline(iss_headers, line) && !line.empty()) {
		// Corrected: Create a temporary variable, trim in place, then use it.
		StringUtils::trim(line); // trim line itself
		if (line.empty()) continue;

		size_t colon_pos = line.find(':');
		if (colon_pos != std::string::npos) {
			std::string name_temp = line.substr(0, colon_pos);
			StringUtils::trim(name_temp); // Trim in place
			std::string value_temp = line.substr(colon_pos + 1);
			StringUtils::trim(value_temp); // Trim in place

			if (StringUtils::ciCompare(name_temp, "Status")) { // Use trimmed name
				std::istringstream status_stream(value_temp); // Use trimmed value
				status_stream >> status_code;
				if (status_stream.fail() || status_code < 100 || status_code >= 600) {
					std::cerr << "WARNING: Invalid Status header from CGI: '" << value_temp << "'. Using default 200." << std::endl;
					status_code = 200;
				}
			} else if (StringUtils::ciCompare(name_temp, "Content-Type")) { // Use trimmed name
				_final_http_response.addHeader("Content-Type", value_temp); // Use trimmed value
				content_type_set = true;
			}
			else {
				_final_http_response.addHeader(name_temp, value_temp); // Use trimmed name and value
			}
		} else {
			std::cerr << "WARNING: Malformed CGI header line: '" << line << "'" << std::endl;
		}
	}

	_final_http_response.setStatus(status_code);
	_final_http_response.setBody(body_str);

	if (!content_type_set) {
		_final_http_response.addHeader("Content-Type", "application/octet-stream");
		std::cerr << "WARNING: CGI did not provide Content-Type header. Defaulting to application/octet-stream." << std::endl;
	}
	_final_http_response.addHeader("Content-Length", StringUtils::longToString(body_str.length()));

	_cgi_headers_parsed = true;
	_state = CGIState::COMPLETE;
}

void CGIHandler::cleanup() {
    std::cout << "DEBUG: CGIHandler::cleanup() called." << std::endl;
    std::cout << "DEBUG: FDs at cleanup start: stdin[0]=\t" << _fd_stdin[0] << ", stdin[1]=\t" << _fd_stdin[1]
		<< ", stdout[0]=\t" << _fd_stdout[0] << ", stdout[1]=\t" << _fd_stdout[1] << std::endl;

    // Unregister and close parent's ends of the pipes
    if (_fd_stdin[1] != -1) { // Parent's write end to CGI stdin
        std::cout << "DEBUG: Cleanup: Unregistering/closing _fd_stdin[1]: " << _fd_stdin[1] << std::endl;
        if (_serverPtr) {
            _serverPtr->unregisterCgiFd(_fd_stdin[1]); // This closes the FD
        } else {
            close(_fd_stdin[1]); // Fallback if _serverPtr is null (shouldn't happen)
        }
        _fd_stdin[1] = -1;
    }
    if (_fd_stdout[0] != -1) { // Parent's read end from CGI stdout
        std::cout << "DEBUG: Cleanup: Unregistering/closing _fd_stdout[0]: " << _fd_stdout[0] << std::endl;
        if (_serverPtr) {
            _serverPtr->unregisterCgiFd(_fd_stdout[0]); // This closes the FD
        } else {
            close(_fd_stdout[0]); // Fallback
        }
        _fd_stdout[0] = -1;
    }

    // The child's ends (_fd_stdin[0] and _fd_stdout[1]) are closed in the parent
    // immediately after fork, and in the child process itself. So, no need to close them here again.
    // Just ensure they are marked as closed in case of error paths where they might not have been.
    _fd_stdin[0] = -1;
    _fd_stdout[1] = -1;

    std::cout << "DEBUG: FDs at cleanup end: stdin[0]=\t" << _fd_stdin[0] << ", stdin[1]=\t" << _fd_stdin[1]
		<< ", stdout[0]=\t" << _fd_stdout[0] << ", stdout[1]=\t" << _fd_stdout[1] << std::endl;

    // If CGI process is still running, attempt to terminate it
    if (_cgi_pid != -1) {
        int status;
        pid_t result = waitpid(_cgi_pid, &status, WNOHANG);
        if (result == 0) { // Child is still running
            std::cerr << "WARNING: CGI process (PID " << _cgi_pid << ") still active during cleanup. Sending SIGTERM." << std::endl;
            kill(_cgi_pid, SIGTERM);
            waitpid(_cgi_pid, &status, 0); // Wait for termination
        } else if (result == -1) {
             // std::cerr << "DEBUG: waitpid for PID " << _cgi_pid << " returned -1 in cleanup (possibly already reaped)." << std::endl;
        }
        _cgi_pid = -1; // Mark as cleaned up
    }
}