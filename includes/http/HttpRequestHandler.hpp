#ifndef HTTP_REQUEST_HANDLER_HPP
# define HTTP_REQUEST_HANDLER_HPP

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "RequestDispatcher.hpp"
#include "../utils/StringUtils.hpp"

#include <string>
#include <vector>
#include <map>
#include <sys/stat.h>
#include <fstream>
#include <dirent.h>
#include <unistd.h>
#include <limits>

// Handles HTTP requests and generates responses.
class HttpRequestHandler {
public:
    // Constructor: Initializes a new HttpRequestHandler.
    HttpRequestHandler();
    // Destructor: Cleans up HttpRequestHandler resources.
    ~HttpRequestHandler();

    // Handles an incoming HTTP request and generates a corresponding response.
    HttpResponse handleRequest(const HttpRequest& request, const MatchedConfig& matchedConfig);
    // Checks if the request should be handled by a CGI script.
    bool isCgiRequest(const MatchedConfig& matchedConfig) const;

private:
    // Generates an HTTP error response, optionally using custom error pages.
    HttpResponse _generateErrorResponse(int statusCode,
                                       const ServerConfig* serverConfig,
                                       const LocationConfig* locationConfig);
    // Resolves the full file system path for a given URI.
    std::string _resolvePath(const std::string& uriPath,
                            const ServerConfig* serverConfig,
                            const LocationConfig* locationConfig) const;
    // Handles a GET request to serve static content or directory listings.
    HttpResponse _handleGet(const HttpRequest& request,
                           const ServerConfig* serverConfig,
                           const LocationConfig* locationConfig);
    // Handles a POST request, primarily for file uploads.
    HttpResponse _handlePost(const HttpRequest& request,
                            const ServerConfig* serverConfig,
                            const LocationConfig* locationConfig);
    // Handles a DELETE request to remove a resource.
    HttpResponse _handleDelete(const HttpRequest& request,
                              const ServerConfig* serverConfig,
                              const LocationConfig* locationConfig);
    // Generates an HTML page listing directory contents for autoindex.
    std::string _generateAutoindexPage(const std::string& directoryPath, const std::string& uriPath) const;
    // Determines the effective root directory for a request.
    std::string _getEffectiveRoot(const ServerConfig* server, const LocationConfig* location) const;
    // Determines the effective error pages map for a request.
    const std::map<int, std::string>& _getEffectiveErrorPages(const ServerConfig* server, const LocationConfig* location) const;
    // Determines the effective upload store path for a request.
    std::string _getEffectiveUploadStore(const ServerConfig* server, const LocationConfig* location) const;
    // Determines the effective client maximum body size for a request.
    long _getEffectiveClientMaxBodySize(const ServerConfig* server, const LocationConfig* location) const;
    // Determines the MIME type based on the file extension.
    std::string _getMimeType(const std::string& filePath) const;
    // Checks if a given path points to a regular file.
    bool _isRegularFile(const std::string& path) const;
    // Checks if a given path points to a directory.
    bool _isDirectory(const std::string& path) const;
    // Checks if a file or directory exists at the given path.
    bool _fileExists(const std::string& path) const;
    // Checks if a file or directory has read permissions.
    bool _canRead(const std::string& path) const;
    // Checks if a directory has write permissions.
    bool _canWrite(const std::string& path) const;
};

#endif
