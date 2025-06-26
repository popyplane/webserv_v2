/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequest.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: baptistevieilhescaze <baptistevieilhesc    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/23 15:26:53 by baptistevie       #+#    #+#             */
/*   Updated: 2025/06/24 23:17:24 by baptistevie      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTPREQUEST_HPP
# define HTTPREQUEST_HPP

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <algorithm>
#include <sstream>

// Enum for HTTP Methods.
enum HttpMethod {
    HTTP_GET,
    HTTP_POST,
    HTTP_DELETE,
    HTTP_UNKNOWN
};

// Helper function to convert HttpMethod enum to string.
std::string httpMethodToString(HttpMethod method);

// Represents a parsed HTTP request.
class HttpRequest {
public:
    // Request Line Components.
    std::string method;
    std::string uri;
    std::string protocolVersion;

    // URI Decomposed Components.
    std::string path;
    std::map<std::string, std::string> queryParams;

    // Headers.
    std::map<std::string, std::string> headers;

    // Message Body.
    std::vector<char> body;
    size_t expectedBodyLength; // From Content-Length header.

    // Parsing State.
    enum ParsingState {
        RECV_REQUEST_LINE,
        RECV_HEADERS,
        RECV_BODY,
        COMPLETE,
        ERROR
    };
    ParsingState currentState;

    // Constructor: Initializes HttpRequest members.
    HttpRequest();

    // Helper Method to get header value (case-insensitive lookup).
    std::string getHeader(const std::string& name) const;

    // Helper Method for debugging: Prints request details.
    void print() const;
};

#endif