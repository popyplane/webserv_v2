/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CGIHandler.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bvieilhe <bvieilhe@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/25 17:40:21 by baptistevie       #+#    #+#             */
/*   Updated: 2025/06/27 04:54:29 by bvieilhe         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CGIHANDLER_HPP
# define CGIHANDLER_HPP

#include <string>
#include <vector>
#include <map>
#include <sstream> // For std::istringstream
#include <algorithm> // For std::transform
#include <cstdio> // For NULL (char**)

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "../config/ServerStructures.hpp" // For ServerConfig, LocationConfig

// Namespace for CGI state types.
namespace CGIState {
    enum Type {
        NOT_STARTED,        // Initial state
        FORK_FAILED,        // Forking the CGI process failed
        WRITING_INPUT,      // Writing request body to CGI stdin
        READING_OUTPUT,     // Reading CGI stdout
        COMPLETE,           // CGI execution finished successfully and output parsed
        TIMEOUT,            // CGI process timed out
        CGI_PROCESS_ERROR   // General error during CGI execution (e.g., execve failed, pipe error)
    };
}

// Handles the execution and communication with CGI scripts.
class CGIHandler {
public:
    // Constructor: Initializes a new CGI handler.
    CGIHandler(const HttpRequest& request,
               const ServerConfig* serverConfig,
               const LocationConfig* locationConfig);

    // Destructor: Cleans up resources.
    ~CGIHandler();

    // Copy constructor and assignment operator (deleted to prevent accidental copying of FDs/PIDs)
    CGIHandler(const CGIHandler& other);
    CGIHandler& operator=(const CGIHandler& other);

    // Initiates the CGI process (forks, sets up pipes, execve).
    bool start();

    // Handles incoming data from the CGI's stdout pipe.
    void handleRead();

    // Handles sending data to the CGI's stdin pipe.
    void handleWrite();

    // Checks the status of the CGI child process (non-blocking).
    void pollCGIProcess();

    // Returns the read file descriptor for the CGI's stdout pipe.
    int getReadFd() const;

    // Returns the write file descriptor for the CGI's stdin pipe.
    int getWriteFd() const;

    // Returns the current state of the CGI execution.
    CGIState::Type getState() const;

    // Sets the state of the CGI execution.
    void setState(CGIState::Type newState);

    // Checks if the CGI execution has completed and its response is ready.
    bool isFinished() const;

    // Returns the HTTP response generated from the CGI output.
    const HttpResponse& getHttpResponse() const;

    // Returns the PID of the forked CGI process.
    pid_t getCGIPid() const;

    // Sets a flag to indicate if a timeout has occurred.
    void setTimeout();

private:
    const HttpRequest& _request;            // Reference to the HTTP request.
    const ServerConfig* _serverConfig;      // Pointer to the server configuration.
    const LocationConfig* _locationConfig;  // Pointer to the matched location configuration.

    std::string         _cgi_script_path;   // Absolute path to the CGI script.
    std::string         _cgi_executable_path; // Absolute path to the CGI interpreter (e.g., /usr/bin/python3).
    pid_t               _cgi_pid;           // PID of the forked CGI process.
    int                 _fd_stdin[2];       // Pipe for parent -> child stdin.
    int                 _fd_stdout[2];      // Pipe for child stdout -> parent.
    std::vector<char>   _cgi_response_buffer; // Buffer for raw CGI output.
    HttpResponse        _final_http_response; // The final HTTP response to be sent to client.
    CGIState::Type      _state;             // Current state of the CGI execution.
    bool                _cgi_headers_parsed; // Flag to indicate if CGI headers have been parsed.
    int                 _cgi_exit_status;   // Exit status of the CGI process.

    // CORRECTED TYPE: Now a pointer to const std::vector<char>
    const std::vector<char>* _request_body_ptr;   // Pointer to the request body if it exists.
    size_t             _request_body_sent_bytes; // Number of bytes sent from request body.


    // Private helper methods
    bool    _setNonBlocking(int fd);            // Sets a file descriptor to non-blocking mode.
    char** _createCGIEnvironment() const;      // Creates environment variables for CGI.
    char** _createCGIArguments() const;        // Creates arguments for execve.
    void    _freeCGICharArrays(char** arr) const; // Frees char** arrays.
    void    _closePipes();                      // Closes all internal pipe FDs. (Now only closes, does not set to -1)
    void    _parseCGIOutput();                  // Parses raw CGI output into HTTP response.
};

#endif
