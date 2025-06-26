/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequestParser.cpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: baptistevieilhescaze <baptistevieilhesc    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/23 15:27:03 by baptistevie       #+#    #+#             */
/*   Updated: 2025/06/24 13:19:07 by baptistevie      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../../includes/http/HttpRequestParser.hpp"
#include "../../includes/utils/StringUtils.hpp"

#include <iostream>
#include <sstream>
#include <cctype>

// Converts an HttpMethod enum to its string representation.
std::string httpMethodToString(HttpMethod method) {
    switch (method) {
        case HTTP_GET: return "GET";
        case HTTP_POST: return "POST";
        case HTTP_DELETE: return "DELETE";
        case HTTP_UNKNOWN: return "UNKNOWN";
        default: return "INVALID_ENUM_VALUE";
    }
}

// Default constructor: Initializes the parser and request state.
HttpRequestParser::HttpRequestParser() : _request() {
    _request.currentState = HttpRequest::RECV_REQUEST_LINE;
}

// Destructor: Cleans up any allocated resources.
HttpRequestParser::~HttpRequestParser() {
}

// Appends new raw data to the internal buffer for parsing.
void HttpRequestParser::appendData(const char* data, size_t len) {
    if (data && len > 0) {
        _buffer.insert(_buffer.end(), data, data + len);
    }
}

// Finds the first occurrence of a pattern string within the internal char vector buffer.
size_t HttpRequestParser::findInVector(const std::string& pattern) {
    if (_buffer.empty() || pattern.empty() || pattern.length() > _buffer.size()) {
        return std::string::npos;
    }
    for (size_t i = 0; i <= _buffer.size() - pattern.length(); ++i) {
        bool match = true;
        for (size_t j = 0; j < pattern.length(); ++j) {
            if (_buffer[i+j] != pattern[j]) {
                match = false;
                break;
            }
        }
        if (match) {
            return i;
        }
    }
    return std::string::npos;
}

// Removes a specified number of bytes from the beginning of the internal buffer.
void HttpRequestParser::consumeBuffer(size_t count) {
    if (count > _buffer.size()) {
        _buffer.clear();
    } else {
        _buffer.erase(_buffer.begin(), _buffer.begin() + count);
    }
}

// Sets the parser to an error state and logs a message.
void HttpRequestParser::setError(const std::string& msg) {
    _request.currentState = HttpRequest::ERROR;
    std::cerr << "HTTP Parsing Error: " << msg << std::endl;
}

// Parses the request line (method, URI, protocol version).
void HttpRequestParser::parseRequestLine() {
    // Find the end of the request line (CRLF).
    size_t crlf_pos = findInVector(CRLF);

    if (crlf_pos == std::string::npos) { 
        return; // Not enough data yet.
    }

    // Extract the request line.
    std::string requestLine( _buffer.begin(), _buffer.begin() + crlf_pos );
    
    // Parse method.
    size_t first_space = requestLine.find(' ');
    if (first_space == std::string::npos) {
        setError("Malformed request line: Missing method or URI.");
        return;
    }
    _request.method = requestLine.substr(0, first_space);

    // Parse URI.
    size_t second_space = requestLine.find(' ', first_space + 1);
    if (second_space == std::string::npos) {
        setError("Malformed request line: Missing URI or protocol version.");
        return;
    }
    _request.uri = requestLine.substr(first_space + 1, second_space - (first_space + 1));

    // Parse protocol version.
    _request.protocolVersion = requestLine.substr(second_space + 1);

    // Basic validation of request line components.
    if (_request.method.empty() || _request.uri.empty() || _request.protocolVersion.empty()) {
        setError("Malformed request line: Empty component.");
        return;
    }
    if (_request.protocolVersion != "HTTP/1.1") {
        setError("Unsupported protocol version. Only HTTP/1.1 is supported.");
        return;
    }

    // Consume the parsed request line from the buffer.
    consumeBuffer(crlf_pos + CRLF.length());

    // Decompose URI into path and query parameters.
    decomposeURI(); 

    _request.currentState = HttpRequest::RECV_HEADERS;
}

// Parses HTTP headers.
void HttpRequestParser::parseHeaders() {
    // Handle empty headers (double CRLF immediately after request line).
    if (_buffer.size() == CRLF.length() && _buffer[0] == '\r' && _buffer[1] == '\n') {
        consumeBuffer(CRLF.length());
        _request.currentState = HttpRequest::COMPLETE;
        return;
    }
    
    // Find the end of the headers block (double CRLF).
    size_t double_crlf_pos = findInVector(DOUBLE_CRLF);

    if (double_crlf_pos == std::string::npos) {
        return; // Not enough data yet.
    }

    // Extract and parse individual headers.
    std::string allHeaders( _buffer.begin(), _buffer.begin() + double_crlf_pos );
    std::istringstream iss(allHeaders);
    std::string line;

    while (std::getline(iss, line, '\r')) {
        if (!line.empty() && line[0] == '\n') {
            line = line.substr(1);
        }
        if (line.empty()) continue;

        size_t colon_pos = line.find(':');
        if (colon_pos == std::string::npos) {
            setError("Malformed header line: Missing colon.");
            return;
        }

        std::string name = line.substr(0, colon_pos);
        std::string value = line.substr(colon_pos + 1);

        StringUtils::trim(name);
        StringUtils::trim(value);
        
        // Store header with canonicalized name (lowercase).
        std::string canonicalName = name;
        for (size_t i = 0; i < canonicalName.length(); ++i) {
            canonicalName[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(canonicalName[i])));
        }
        _request.headers[canonicalName] = value;
    }

    // Process Content-Length header to determine expected body size.
    std::string contentLengthStr = _request.getHeader("content-length");
    if (!contentLengthStr.empty()) {
        try {
            _request.expectedBodyLength = StringUtils::stringToLong(contentLengthStr);
        } catch (const std::exception& e) {
            setError("Invalid Content-Length header: " + std::string(e.what()));
            return;
        }
    } else {
        if (_request.method == "POST") {
            setError("Content-Length header missing for POST request.");
            return;
        }
    }
    
    // Consume parsed headers and the double CRLF from the buffer.
    consumeBuffer(double_crlf_pos + DOUBLE_CRLF.length());

    // Transition to body parsing or complete state.
    if (_request.method == "POST" && _request.expectedBodyLength > 0) {
        _request.currentState = HttpRequest::RECV_BODY;
    } else {
        _request.currentState = HttpRequest::COMPLETE;
    }

    // Check for extraneous data if request is complete without a body.
    if (_request.currentState == HttpRequest::COMPLETE && !_buffer.empty()) {
        setError("Extraneous data after end of headers for request with no body.");
        return;
    }
}

// Parses the HTTP request body.
void HttpRequestParser::parseBody() {
    // Check if the entire body is available in the buffer.
    if (_buffer.size() < _request.expectedBodyLength) {
        return; // Not enough data yet.
    }

    // Copy the body data from the buffer to the HttpRequest object.
    _request.body.insert(_request.body.end(), _buffer.begin(), _buffer.begin() + _request.expectedBodyLength);
    
    // Consume the parsed body data from the buffer.
    consumeBuffer(_request.expectedBodyLength);

    // Update parsing state to complete.
    _request.currentState = HttpRequest::COMPLETE;

    // Check for extraneous data after the body.
    if (!_buffer.empty()) {
        setError("Extraneous data after end of body.");
        return;
    }
}

// Decomposes the URI into path and query parameters.
void HttpRequestParser::decomposeURI() {
    // Check for presence of query string.
    size_t query_pos = _request.uri.find('?');
    if (query_pos != std::string::npos) {
        // Extract path and query string.
        _request.path = _request.uri.substr(0, query_pos);
        std::string queryString = _request.uri.substr(query_pos + 1);
        
        // Parse query parameters (key=value pairs).
        std::istringstream qss(queryString);
        std::string pair;
        while (std::getline(qss, pair, '&')) {
            size_t eq_pos = pair.find('=');
            if (eq_pos != std::string::npos) {
                std::string key = pair.substr(0, eq_pos);
                std::string value = pair.substr(eq_pos + 1);
                _request.queryParams[key] = value;
            } else {
                _request.queryParams[pair] = ""; // Key only.
            }
        }
    } else {
        _request.path = _request.uri; // Entire URI is the path.
    }
}

// Main parsing function: Drives the state machine to parse the HTTP request.
void HttpRequestParser::parse() {
    size_t prev_buffer_size;
    HttpRequest::ParsingState prev_state;

    // Continue parsing as long as the request is not complete or in an error state,
    // and progress is being made in each iteration.
    while (_request.currentState != HttpRequest::COMPLETE && _request.currentState != HttpRequest::ERROR) {
        prev_buffer_size = _buffer.size();
        prev_state = _request.currentState;

        switch (_request.currentState) {
            case HttpRequest::RECV_REQUEST_LINE:
                parseRequestLine();
                break;
            case HttpRequest::RECV_HEADERS:
                parseHeaders();
                break;
            case HttpRequest::RECV_BODY:
                parseBody();
                break;
            case HttpRequest::COMPLETE:
            case HttpRequest::ERROR:
                return;
        }

        // Break if no progress was made (waiting for more data).
        if (_buffer.size() == prev_buffer_size && _request.currentState == prev_state) {
            break;
        }
    }
}

// Checks if the HTTP request parsing is complete.
bool HttpRequestParser::isComplete() const {
    return _request.currentState == HttpRequest::COMPLETE;
}

// Checks if an error occurred during HTTP request parsing.
bool HttpRequestParser::hasError() const {
    return _request.currentState == HttpRequest::ERROR;
}

// Returns a reference to the parsed HttpRequest object.
HttpRequest& HttpRequestParser::getRequest() {
    return _request;
}

// Returns a constant reference to the parsed HttpRequest object.
const HttpRequest& HttpRequestParser::getRequest() const {
    return _request;
}

// Resets the parser to its initial state for a new request.
void HttpRequestParser::reset() {
    _request = HttpRequest();
    _buffer.clear();
}
