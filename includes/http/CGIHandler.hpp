/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CGIHandler.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: baptistevieilhescaze <baptistevieilhesc    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/25 17:40:21 by baptistevie       #+#    #+#             */
/*   Updated: 2025/06/25 17:45:03 by baptistevie      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include <string>
#include <vector>
#include <map>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <errno.h>
#include <cstring>

// Forward declarations to avoid circular dependencies.
class HttpRequest;
struct ServerConfig;
struct LocationConfig;

#include "HttpResponse.hpp"

// Enum to define the internal state of the CGI process within the handler.
namespace CGIState {
    enum Type {
        NOT_STARTED,        // CGI process not yet forked.
        FORK_FAILED,        // Fork operation failed.
        WRITING_INPUT,      // Currently writing request body to CGI's stdin pipe.
        READING_OUTPUT,     // Currently reading CGI's stdout pipe.
        PROCESSING_OUTPUT,  // All output read, now parsing headers/body.
        COMPLETE,           // CGI process finished, output parsed, ready to build final HTTP response.
        TIMEOUT,            // CGI process timed out.
        CGI_EXEC_FAILED,    // execve failed in child process.
        CGI_PROCESS_ERROR   // Generic error during CGI process I/O or termination.
    };
}

// Handles the execution and communication with a CGI script.
class CGIHandler {
public:
    // Constructor: Takes the request and matched configuration.
    CGIHandler(const HttpRequest& request,
               const ServerConfig* serverConfig,
               const LocationConfig* locationConfig);

    // Destructor: Cleans up any open file descriptors and child processes.
    ~CGIHandler();

    // Initiates the CGI process (fork, pipe, execve).
    bool start();

    // Returns the read file descriptor for the CGI's stdout pipe.
    int getReadFd() const;

    // Returns the write file descriptor for the CGI's stdin pipe.
    int getWriteFd() const;

    // Handles incoming data from the CGI's stdout pipe.
    void handleRead();

    // Handles sending data to the CGI's stdin pipe (for POST requests).
    void handleWrite();

    // Checks the status of the CGI child process (non-blocking waitpid).
    void pollCGIProcess();

    // Returns the current state of the CGI execution.
    CGIState::Type getState() const;

    // Checks if the CGI execution has completed and its response is ready.
    bool isFinished() const;

    // Returns the HTTP response generated from the CGI output.
    const HttpResponse& getHttpResponse() const;

    // Returns the PID of the forked CGI process.
    pid_t getCGIPid() const;

    // Sets a flag to indicate if a timeout has occurred.
    void setTimeout();

private:
    // Disallow copy constructor and assignment operator for safety.
    CGIHandler(const CGIHandler& other);
    CGIHandler& operator=(const CGIHandler& other);

    // Sets a file descriptor to non-blocking mode.
    bool _setNonBlocking(int fd);

    // Creates environment variables for CGI execution.
    char** _createCGIEnvironment() const;

    // Creates argument list for execve.
    char** _createCGIArguments() const;

    // Frees char** arrays (for envp and argv).
    void _freeCGICharArrays(char** arr) const;

    // Parses the CGI's raw output into headers and body.
    void _parseCGIOutput();

    // Cleans up CGI related file descriptors.
    void _closePipes();

    const HttpRequest& _request;            // Reference to the original HTTP request.
    const ServerConfig* _serverConfig;      // Pointer to the matched server config.
    const LocationConfig* _locationConfig;  // Pointer to the matched location config.

    pid_t           _cgi_pid;               // PID of the forked CGI child process.
    int             _fd_stdin[2];           // Pipe for server->CGI stdin.
    int             _fd_stdout[2];          // Pipe for CGI->server stdout.

    std::vector<char> _cgi_response_buffer; // Buffer to accumulate raw CGI output.
    size_t          _request_body_sent_bytes; // Tracks how much POST body has been sent.
    const std::vector<char>* _request_body_ptr; // Pointer to the request's body data.

    CGIState::Type  _state;                 // Current state of the CGI execution.
    HttpResponse    _final_http_response;   // The HTTP response built from CGI output.
    bool            _cgi_headers_parsed;    // Flag if CGI's HTTP headers have been parsed.
    int             _cgi_exit_status;       // Exit status of the CGI child process.

    std::string     _cgi_script_path;       // Full file system path to the CGI script.
    std::string     _cgi_executable_path;   // Full file system path to the CGI interpreter.
};

#endif // CGIHANDLER_HPP
