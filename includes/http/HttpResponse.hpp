/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpResponse.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: baptistevieilhescaze <baptistevieilhesc    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/25 09:50:26 by baptistevie       #+#    #+#             */
/*   Updated: 2025/06/25 09:55:47 by baptistevie      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTP_RESPONSE_HPP
# define HTTP_RESPONSE_HPP

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <ctime>

// Helper function to get HTTP status message for a given code.
std::string getHttpStatusMessage(int statusCode);

// Helper function to get MIME type based on file extension.
std::string getMimeType(const std::string& filePath);

// Represents an HTTP response to be sent back to a client.
class HttpResponse {
public:
    // Constructor: Initializes with default protocol version and common headers.
    HttpResponse();

    // Destructor: Cleans up HttpResponse resources.
    ~HttpResponse();

    // Sets the HTTP status of the response.
    void setStatus(int code);

    // Adds or updates a header in the response.
    void addHeader(const std::string& name, const std::string& value);

    // Sets the response body from a string and updates Content-Length.
    void setBody(const std::string& content);

    // Sets the response body from a vector of characters (for binary data) and updates Content-Length.
    void setBody(const std::vector<char>& content);

    // Generates the complete raw HTTP response string, ready to be sent over a socket.
    std::string toString() const;

    // Getters for Response Components.
    int getStatusCode() const { return _statusCode; }
    const std::string& getStatusMessage() const { return _statusMessage; }
    const std::string& getProtocolVersion() const { return _protocolVersion; }
    const std::map<std::string, std::string>& getHeaders() const { return _headers; }
    const std::vector<char>& getBody() const { return _body; }

private:
    std::string _protocolVersion; // e.g., "HTTP/1.1".
    int         _statusCode; // e.g., 200, 404.
    std::string _statusMessage; // e.g., "OK", "Not Found".
    std::map<std::string, std::string> _headers; // Header names are typically canonical.
    std::vector<char> _body; // Use std::vector<char> for the body to handle binary data safely.

    // Helper to generate current GMT date/time for the "Date" header.
    std::string getCurrentGmTime() const;

    // Default headers that should always be present unless explicitly overridden.
    void setDefaultHeaders();
};

#endif
