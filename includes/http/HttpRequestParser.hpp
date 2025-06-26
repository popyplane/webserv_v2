/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequestParser.hpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: baptistevieilhescaze <baptistevieilhesc    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/23 15:26:50 by baptistevie       #+#    #+#             */
/*   Updated: 2025/06/23 16:08:48 by baptistevie      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTP_REQUEST_PARSER_HPP
# define HTTP_REQUEST_PARSER_HPP

#include "HttpRequest.hpp"
#include <vector>
#include <string>

// Define CRLF for consistency.
const std::string CRLF = "\r\n";
const std::string DOUBLE_CRLF = "\r\n\r\n";

// Parses raw HTTP request data into an HttpRequest object.
class HttpRequestParser {
private:
    HttpRequest         _request; // The HttpRequest object being parsed.
    std::vector<char>   _buffer; // Buffer to accumulate incoming raw data.

    // Parses the request line (method, URI, protocol version).
    void parseRequestLine();
    // Parses HTTP headers.
    void parseHeaders();
    // Parses the HTTP request body.
    void parseBody();
    // Decomposes the URI into path and query parameters.
    void decomposeURI();

    // Helper to find a substring (pattern) within a std::vector<char>.
    size_t findInVector(const std::string& pattern);
    // Helper to remove parsed data from the beginning of the buffer.
    void consumeBuffer(size_t count);
    // Helper to set the error state and potentially log a message.
    void setError(const std::string& msg);

public:
    // Constructor: Initializes the parser.
    HttpRequestParser();
    // Destructor: Cleans up resources.
    ~HttpRequestParser();

    // Appends new raw data to the internal buffer.
    void appendData(const char* data, size_t len);

    // Attempts to parse the request based on the current state and buffered data.
    void parse();

    // Checks if parsing is complete.
    bool isComplete() const;
    // Checks if parsing encountered an error.
    bool hasError() const;

    // Returns a reference to the parsed HttpRequest object.
    HttpRequest& getRequest();
    // Returns a constant reference to the parsed HttpRequest object.
    const HttpRequest& getRequest() const;

    // Resets the parser for a new request.
    void reset();
};

#endif
