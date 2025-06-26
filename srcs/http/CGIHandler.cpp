/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CGIHandler.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: baptistevieilhescaze <baptistevieilhesc    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/25 17:47:11 by baptistevie       #+#    #+#             */
/*   Updated: 2025/06/25 23:25:10 by baptistevie      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../../includes/http/CGIHandler.hpp"
#include "../../includes/http/HttpRequest.hpp"
#include "../../includes/config/ServerStructures.hpp"
#include "../../includes/utils/StringUtils.hpp"

#include <iostream>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <cstdio>
#include <signal.h>
#include <fcntl.h>
#include <cstring>
#include <unistd.h>

// Constructor: Initializes CGIHandler with request and configuration details.
CGIHandler::CGIHandler(const HttpRequest& request,
                       const ServerConfig* serverConfig,
                       const LocationConfig* locationConfig)
    : _request(request),
      _serverConfig(serverConfig),
      _locationConfig(locationConfig),
      _cgi_pid(-1),
      _request_body_sent_bytes(0),
      _request_body_ptr(&request.body),
      _state(CGIState::NOT_STARTED),
      _cgi_headers_parsed(false),
      _cgi_exit_status(-1)
{
    // Initialize pipe FDs to -1 to indicate they are not open.
    _fd_stdin[0] = -1;
    _fd_stdin[1] = -1;
    _fd_stdout[0] = -1;
    _fd_stdout[1] = -1;

    // Determine CGI script path and executable path from config.
    if (_locationConfig && !_locationConfig->root.empty() && !_locationConfig->cgiExecutables.empty()) {
        std::string locationRoot = _locationConfig->root;
        // Ensure locationRoot does NOT end with a slash for proper concatenation.
        if (locationRoot.length() > 1 && locationRoot[locationRoot.length() - 1] == '/') {
            locationRoot = locationRoot.substr(0, locationRoot.length() - 1);
        }

        // Extract the file extension from the request path.
        size_t dot_pos = _request.path.rfind('.');
        if (dot_pos == std::string::npos) {
            std::cerr << "ERROR: CGIHandler: No file extension found in URI for CGI: " << _request.path << std::endl;
            _state = CGIState::CGI_PROCESS_ERROR;
            return;
        }
        std::string file_extension = _request.path.substr(dot_pos);
        
        // Find the CGI executable mapped to this extension.
        std::map<std::string, std::string>::const_iterator cgi_it = _locationConfig->cgiExecutables.find(file_extension);
        if (cgi_it == _locationConfig->cgiExecutables.end()) {
            std::cerr << "ERROR: CGIHandler: No CGI executable configured for extension: " << file_extension << std::endl;
            _state = CGIState::CGI_PROCESS_ERROR;
            return;
        }
        _cgi_executable_path = cgi_it->second;

        // Combines location's root with the full URI path portion that maps to the script.
        std::string requestPathNormalized = _request.path;
        if (requestPathNormalized.empty() || requestPathNormalized[0] != '/') {
             requestPathNormalized = "/" + requestPathNormalized;
        }
        _cgi_script_path = locationRoot + requestPathNormalized;

    } else {
        std::cerr << "ERROR: CGIHandler: Incomplete location config for CGI setup." << std::endl;
        _state = CGIState::CGI_PROCESS_ERROR;
    }

    // Set body pointer to NULL if request has no body (e.g., GET).
    if (_request.body.empty()) {
        _request_body_ptr = NULL;
    }
}

// Destructor: Cleans up any open file descriptors and child processes.
CGIHandler::~CGIHandler() {
    _closePipes(); // Ensure pipes are closed.

    // If CGI process was forked and not yet waited for, wait for it.
    if (_cgi_pid != -1) {
        int status;
        pid_t result = waitpid(_cgi_pid, &status, WNOHANG);
        if (result == 0) {
            std::cerr << "WARNING: CGI child process " << _cgi_pid << " still running in destructor, sending SIGTERM." << std::endl;
            kill(_cgi_pid, SIGTERM);
            waitpid(_cgi_pid, &status, 0);
        }
    }
}

// Copy Constructor: Disallows copying to prevent issues with file descriptors and PIDs.
CGIHandler::CGIHandler(const CGIHandler& other)
    : _request(other._request),
      _serverConfig(other._serverConfig),
      _locationConfig(other._locationConfig),
      _cgi_pid(-1),
      _request_body_sent_bytes(0),
      _request_body_ptr(other._request_body_ptr),
      _state(CGIState::NOT_STARTED),
      _cgi_headers_parsed(false),
      _cgi_exit_status(-1),
      _cgi_script_path(other._cgi_script_path),
      _cgi_executable_path(other._cgi_executable_path)
{
    _fd_stdin[0] = -1; _fd_stdin[1] = -1;
    _fd_stdout[0] = -1; _fd_stdout[1] = -1;
}

// Assignment Operator: Disallows assignment to prevent issues with file descriptors and PIDs.
CGIHandler& CGIHandler::operator=(const CGIHandler& other) {
    if (this != &other) {
        _closePipes();
        if (_cgi_pid != -1) {
            kill(_cgi_pid, SIGTERM);
            waitpid(_cgi_pid, NULL, 0);
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
        _state = CGIState::NOT_STARTED;
        _cgi_headers_parsed = false;
        _cgi_exit_status = -1;
    }
    return *this;
}

// Sets a file descriptor to non-blocking mode.
bool CGIHandler::_setNonBlocking(int fd) {
    if (fd < 0) return false;
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        std::cerr << "ERROR: fcntl F_GETFL failed for FD " << fd << ": " << strerror(errno) << std::endl;
        return false;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        std::cerr << "ERROR: fcntl F_SETFL O_NONBLOCK failed for FD " << fd << ": " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

// Creates environment variables for CGI execution.
char** CGIHandler::_createCGIEnvironment() const {
    std::vector<std::string> env_vars_vec;

    // Mandatory CGI variables.
    env_vars_vec.push_back("REQUEST_METHOD=" + _request.method);
    env_vars_vec.push_back("SERVER_PROTOCOL=" + _request.protocolVersion);
    env_vars_vec.push_back("REDIRECT_STATUS=200");

    // SERVER_NAME and SERVER_PORT from matched server config.
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

    // SCRIPT_FILENAME: Full file system path to the script.
    env_vars_vec.push_back("SCRIPT_FILENAME=" + _cgi_script_path); 

    // SCRIPT_NAME: The URI path to the script itself.
    env_vars_vec.push_back("SCRIPT_NAME=" + _request.path); 

    // PATH_INFO: Additional path information from the URI beyond the script name.
    std::string path_info;
    size_t script_name_len = _request.path.length();
    size_t uri_path_len = _request.uri.find('?');
    if (uri_path_len == std::string::npos) {
        uri_path_len = _request.uri.length();
    }

    if (uri_path_len > script_name_len) {
        path_info = _request.uri.substr(script_name_len, uri_path_len - script_name_len);
    }
    env_vars_vec.push_back("PATH_INFO=" + path_info); 

    // REQUEST_URI: The full original request URI (including query string).
    env_vars_vec.push_back("REQUEST_URI=" + _request.uri); 

    // QUERY_STRING for GET requests.
    size_t query_pos = _request.uri.find('?');
    if (query_pos != std::string::npos) {
        env_vars_vec.push_back("QUERY_STRING=" + _request.uri.substr(query_pos + 1));
    } else {
        env_vars_vec.push_back("QUERY_STRING=");
    }

    // CONTENT_TYPE and CONTENT_LENGTH for POST requests.
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
            env_vars_vec.push_back("CONTENT_LENGTH=0");
        }
    } else {
        env_vars_vec.push_back("CONTENT_TYPE=");
        env_vars_vec.push_back("CONTENT_LENGTH=");
    }

    // Add DOCUMENT_ROOT as it's often required by PHP CGI.
    if (_locationConfig && !_locationConfig->root.empty()) {
        std::string doc_root = _locationConfig->root;
        if (doc_root.length() > 1 && doc_root[doc_root.length() - 1] == '/') {
            doc_root = doc_root.substr(0, doc_root.length() - 1);
        }
        env_vars_vec.push_back("DOCUMENT_ROOT=" + doc_root);
    } else {
        env_vars_vec.push_back("DOCUMENT_ROOT=/");
    }

    // Other HTTP headers (prefixed with HTTP_ and converted to uppercase with _ instead of -).
    for (std::map<std::string, std::string>::const_iterator it = _request.headers.begin(); it != _request.headers.end(); ++it) {
        std::string header_name = it->first;
        if (StringUtils::ciCompare(header_name, "content-type") || StringUtils::ciCompare(header_name, "content-length")) {
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
    
    // REMOTE_ADDR and REMOTE_PORT placeholders. Actual client IP/Port integration requires HttpRequest to store this info.
    env_vars_vec.push_back("REMOTE_ADDR=127.0.0.1");
    env_vars_vec.push_back("REMOTE_PORT=8080");

    // Convert std::vector<std::string> to char**.
    char** envp = new char*[env_vars_vec.size() + 1];
    for (size_t i = 0; i < env_vars_vec.size(); ++i) {
        envp[i] = new char[env_vars_vec[i].length() + 1];
        std::strcpy(envp[i], env_vars_vec[i].c_str());
    }
    envp[env_vars_vec.size()] = NULL;
    return envp;
}

// Creates argument list for execve.
char** CGIHandler::_createCGIArguments() const {
    char** argv = new char*[3];
    argv[0] = new char[_cgi_executable_path.length() + 1];
    std::strcpy(argv[0], _cgi_executable_path.c_str());
    
    argv[1] = new char[_cgi_script_path.length() + 1];
    std::strcpy(argv[1], _cgi_script_path.c_str());
    
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
    if (_state != CGIState::NOT_STARTED) {
        std::cerr << "ERROR: CGI process already started or in an invalid state." << std::endl;
        return false;
    }

    // Check if paths are valid before starting.
    if (_cgi_script_path.empty() || _cgi_executable_path.empty()) {
        std::cerr << "ERROR: CGIHandler: Script or executable path not properly initialized." << std::endl;
        _state = CGIState::CGI_PROCESS_ERROR;
        return false;
    }

    // 1. Create pipes for stdin and stdout communication.
    if (pipe(_fd_stdin) == -1) {
        std::cerr << "ERROR: Failed to create stdin pipe: " << strerror(errno) << std::endl;
        _state = CGIState::FORK_FAILED;
        return false;
    }
    if (pipe(_fd_stdout) == -1) {
        std::cerr << "ERROR: Failed to create stdout pipe: " << strerror(errno) << std::endl;
        _closePipes();
        _state = CGIState::FORK_FAILED;
        return false;
    }

    // 2. Set parent's ends of pipes to non-blocking.
    if (!_setNonBlocking(_fd_stdin[1]) || !_setNonBlocking(_fd_stdout[0])) {
        _closePipes();
        _state = CGIState::FORK_FAILED;
        return false;
    }

    // 3. Fork process.
    _cgi_pid = fork();
    if (_cgi_pid == -1) {
        std::cerr << "ERROR: Failed to fork CGI process: " << strerror(errno) << std::endl;
        _closePipes();
        _state = CGIState::FORK_FAILED;
        return false;
    }

    if (_cgi_pid == 0) { // Child process.
        // Close parent's ends of pipes in child.
        close(_fd_stdin[1]);
        close(_fd_stdout[0]);

        // Redirect stdin/stdout to pipe ends.
        if (dup2(_fd_stdin[0], STDIN_FILENO) == -1) {
            std::cerr << "ERROR: dup2 STDIN_FILENO failed in CGI child: " << strerror(errno) << std::endl;
            _exit(EXIT_FAILURE);
        }
        if (dup2(_fd_stdout[1], STDOUT_FILENO) == -1) {
            std::cerr << "ERROR: dup2 STDOUT_FILENO failed in CGI child: " << strerror(errno) << std::endl;
            _exit(EXIT_FAILURE);
        }

        // Close original pipe FDs after dup2.
        close(_fd_stdin[0]);
        close(_fd_stdout[1]);

        // Change working directory to DOCUMENT_ROOT or script directory.
        std::string cgi_working_dir;
        if (_locationConfig && !_locationConfig->root.empty()) {
            cgi_working_dir = _locationConfig->root;
        } else {
            size_t last_slash_pos = _cgi_script_path.rfind('/');
            if (last_slash_pos != std::string::npos) {
                cgi_working_dir = _cgi_script_path.substr(0, last_slash_pos);
            } else {
                cgi_working_dir = ".";
            }
        }

        if (chdir(cgi_working_dir.c_str()) == -1) {
            std::cerr << "ERROR: chdir failed in CGI child to " << cgi_working_dir << ": " << strerror(errno) << std::endl;
            _exit(EXIT_FAILURE);
        }

        // Prepare environment and arguments.
        char** envp = _createCGIEnvironment();
        char** argv = _createCGIArguments();

        // Execute CGI program.
        execve(_cgi_executable_path.c_str(), argv, envp);

        // If execve returns, it must have failed.
        std::cerr << "ERROR: execve failed for CGI: " << _cgi_executable_path << " - " << strerror(errno) << std::endl;
        _freeCGICharArrays(envp);
        _freeCGICharArrays(argv);
        _exit(EXIT_FAILURE);
    } else { // Parent process.
        // Close child's ends of pipes in parent.
        close(_fd_stdin[0]);
        close(_fd_stdout[1]);

        // Set initial state based on request method.
        if (_request.method == "POST" && _request_body_ptr && !_request_body_ptr->empty()) {
            _state = CGIState::WRITING_INPUT;
        } else {
            _state = CGIState::READING_OUTPUT;
        }
    }
    return true;
}

// Returns the read file descriptor for the CGI's stdout pipe.
int CGIHandler::getReadFd() const {
    return _fd_stdout[0];
}

// Returns the write file descriptor for the CGI's stdin pipe.
int CGIHandler::getWriteFd() const {
    return _fd_stdin[1];
}

// Handles incoming data from the CGI's stdout pipe.
void CGIHandler::handleRead() {
    if (_state != CGIState::READING_OUTPUT && _state != CGIState::WRITING_INPUT) {
        std::cerr << "WARNING: handleRead called in unexpected state: " << _state << std::endl;
        return;
    }

    char buffer[4096];
    ssize_t bytes_read = read(_fd_stdout[0], buffer, sizeof(buffer));

    if (bytes_read > 0) {
        _cgi_response_buffer.insert(_cgi_response_buffer.end(), buffer, buffer + bytes_read);
    } else if (bytes_read == 0) {
        close(_fd_stdout[0]);
        _fd_stdout[0] = -1;
        
        // After receiving all output, parse it.
        _parseCGIOutput();
        // Transition to COMPLETE only if all input was sent and output fully parsed.
        if (_state != CGIState::WRITING_INPUT) {
            if (_fd_stdin[1] == -1) {
                _state = CGIState::COMPLETE;
            }
        }
    } else if (bytes_read == -1) {
        // Per subject: "Checking the value of errno is strictly forbidden after performing a read or write operation."
        // So, any -1 from read is treated as a fatal error.
        std::cerr << "ERROR: Reading from CGI stdout pipe failed." << std::endl;
        _state = CGIState::CGI_PROCESS_ERROR;
        _closePipes();
    }
}

// Handles sending data to the CGI's stdin pipe (for POST requests).
void CGIHandler::handleWrite() {
    if (_state != CGIState::WRITING_INPUT) {
        std::cerr << "WARNING: handleWrite called in unexpected state: " << _state << std::endl;
        return;
    }

    if (!_request_body_ptr || _request_body_ptr->empty()) {
        close(_fd_stdin[1]);
        _fd_stdin[1] = -1;
        _state = CGIState::READING_OUTPUT;
        return;
    }

    size_t remaining_bytes = _request_body_ptr->size() - _request_body_sent_bytes;
    if (remaining_bytes == 0) {
        close(_fd_stdin[1]);
        _fd_stdin[1] = -1;
        _state = CGIState::READING_OUTPUT;
        return;
    }

    ssize_t bytes_written = write(_fd_stdin[1],
                                  &(*_request_body_ptr)[_request_body_sent_bytes],
                                  remaining_bytes);

    if (bytes_written > 0) {
        _request_body_sent_bytes += bytes_written;
        if (_request_body_sent_bytes == _request_body_ptr->size()) {
            close(_fd_stdin[1]);
            _fd_stdin[1] = -1;
            _state = CGIState::READING_OUTPUT;
        }
    } else if (bytes_written == -1) {
        // Per subject: "Checking the value of errno is strictly forbidden after performing a read or write operation."
        // So, any -1 from write is treated as a fatal error.
        std::cerr << "ERROR: Writing to CGI stdin pipe failed." << std::endl;
        _state = CGIState::CGI_PROCESS_ERROR;
        _closePipes();
    }
}

// Checks the status of the CGI child process (non-blocking waitpid).
void CGIHandler::pollCGIProcess() {
    if (_cgi_pid != -1 && !isFinished()) {
        int status;
        pid_t result = waitpid(_cgi_pid, &status, WNOHANG);

        if (result == _cgi_pid) {
            if (WIFEXITED(status)) {
                _cgi_exit_status = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                _cgi_exit_status = WTERMSIG(status);
                _state = CGIState::CGI_PROCESS_ERROR;
            } else {
                _state = CGIState::CGI_PROCESS_ERROR;
            }

            _closePipes();

            if (!_cgi_headers_parsed && !_cgi_response_buffer.empty()) {
                _parseCGIOutput();
            } else if (!_cgi_headers_parsed && _cgi_response_buffer.empty() && _state != CGIState::CGI_PROCESS_ERROR) {
                _final_http_response.setStatus(500);
                _final_http_response.addHeader("Content-Type", "text/html");
                _final_http_response.setBody("<html><body><h1>500 Internal Server Error</h1><p>CGI process exited without output.</p></body></html>");
                _state = CGIState::CGI_PROCESS_ERROR;
            }

            if (_state != CGIState::CGI_PROCESS_ERROR && _state != CGIState::TIMEOUT) {
                _state = CGIState::COMPLETE;
            }

        } else if (result == -1) {
            std::cerr << "ERROR: waitpid failed for CGI process " << _cgi_pid << ": " << strerror(errno) << std::endl;
            _state = CGIState::CGI_PROCESS_ERROR;
            _closePipes();
        }
    }
}

// Returns the current state of the CGI execution.
CGIState::Type CGIHandler::getState() const {
    return _state;
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

// Sets a flag to indicate if a timeout has occurred.
void CGIHandler::setTimeout() {
    if (isFinished()) return;

    std::cerr << "WARNING: CGI process " << _cgi_pid << " timed out." << std::endl;
    _state = CGIState::TIMEOUT;
    if (_cgi_pid != -1) {
        kill(_cgi_pid, SIGTERM);
    }
    _closePipes();

    // Generate a 504 Gateway Timeout response.
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

    bool crlf_crlf = true;
    if (header_end_pos == std::string::npos) {
        header_end_pos = raw_output.find("\n\n");
        crlf_crlf = false;
    }

    std::string cgi_headers_str;
    std::string cgi_body_str;

    if (header_end_pos != std::string::npos) {
        cgi_headers_str = raw_output.substr(0, header_end_pos);
        cgi_body_str = raw_output.substr(header_end_pos + (crlf_crlf ? 4 : 2));
    } else {
        std::cerr << "WARNING: No double CRLF/LF found in CGI output, treating entire output as body (CGI output was: " << raw_output.substr(0, std::min((size_t)200, raw_output.length())) << "..." << std::endl;
        cgi_body_str = raw_output;
    }

    _final_http_response.setBody(cgi_body_str);

    std::istringstream header_stream(cgi_headers_str);
    std::string line;
    int cgi_status_code = 200;
    bool content_type_provided_by_cgi = false;

    while (std::getline(header_stream, line) && !line.empty()) {
        std::string trimmed_line = line;
        StringUtils::trim(trimmed_line);

        size_t colon_pos = trimmed_line.find(':');
        if (colon_pos == std::string::npos) {
            std::cerr << "WARNING: Malformed CGI header line (no colon): " << trimmed_line << std::endl;
            continue;
        }

        std::string name = trimmed_line.substr(0, colon_pos);
        StringUtils::trim(name);

        std::string value = trimmed_line.substr(colon_pos + 1);
        StringUtils::trim(value);

        if (StringUtils::ciCompare(name, "Status")) {
            std::istringstream status_stream(value);
            status_stream >> cgi_status_code;
            if (status_stream.fail()) {
                std::cerr << "WARNING: Failed to parse CGI Status code from '" << value << "'. Defaulting to 200." << std::endl;
                cgi_status_code = 200;
            }
            _final_http_response.setStatus(cgi_status_code);
        } else if (StringUtils::ciCompare(name, "Content-Type")) {
            _final_http_response.addHeader("Content-Type", value);
            content_type_provided_by_cgi = true;
        } else {
            _final_http_response.addHeader(name, value);
        }
    }
    
    if (_final_http_response.getHeaders().find("Content-Length") == _final_http_response.getHeaders().end()) {
        _final_http_response.addHeader("Content-Length", StringUtils::longToString(_final_http_response.getBody().size()));
    }

    if (!content_type_provided_by_cgi) {
        _final_http_response.addHeader("Content-Type", "application/octet-stream");
        std::cerr << "WARNING: CGI did not provide Content-Type, defaulting to application/octet-stream." << std::endl;
    }

    _cgi_headers_parsed = true;
    if (_state != CGIState::COMPLETE) {
        _state = CGIState::PROCESSING_OUTPUT;
    }
}