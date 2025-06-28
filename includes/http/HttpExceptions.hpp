#ifndef HTTPEXCEPTIONS_HPP
# define HTTPEXCEPTIONS_HPP

#include <stdexcept>
#include <string>

class HttpException : public std::runtime_error {
private:
    int _statusCode;
public:
    HttpException(const std::string& message, int statusCode)
        : std::runtime_error(message), _statusCode(statusCode) {}

    int getStatusCode() const { return _statusCode; }
};

class Http400Exception : public HttpException {
public:
    Http400Exception(const std::string& message = "Bad Request") : HttpException(message, 400) {}
};

class Http403Exception : public HttpException {
public:
    Http403Exception(const std::string& message = "Forbidden") : HttpException(message, 403) {}
};

class Http404Exception : public HttpException {
public:
    Http404Exception(const std::string& message = "Not Found") : HttpException(message, 404) {}
};

class Http405Exception : public HttpException {
public:
    Http405Exception(const std::string& message = "Method Not Allowed") : HttpException(message, 405) {}
};

class Http411Exception : public HttpException {
public:
    Http411Exception(const std::string& message = "Length Required") : HttpException(message, 411) {}
};

class Http413Exception : public HttpException {
public:
    Http413Exception(const std::string& message = "Payload Too Large") : HttpException(message, 413) {}
};

class Http500Exception : public HttpException {
public:
    Http500Exception(const std::string& message = "Internal Server Error") : HttpException(message, 500) {}
};

class Http501Exception : public HttpException {
public:
    Http501Exception(const std::string& message = "Not Implemented") : HttpException(message, 501) {}
};

#endif
