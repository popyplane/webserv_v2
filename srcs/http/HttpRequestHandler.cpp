/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequestHandler.cpp                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bvieilhe <bvieilhe@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/25 09:57:18 by baptistevie       #+#    #+#             */
/*   Updated: 2025/06/26 23:43:10 by bvieilhe         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../../includes/http/HttpRequestHandler.hpp"
#include "../../includes/utils/StringUtils.hpp"

#include <iostream>
#include <sstream>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <dirent.h>
#include <sys/time.h>
#include <limits>
#include <errno.h>
#include <string.h> // For strerror

// Constructor: Initializes a new HttpRequestHandler.
HttpRequestHandler::HttpRequestHandler() {}

// Destructor: Cleans up HttpRequestHandler resources.
HttpRequestHandler::~HttpRequestHandler() {}

// --- Private Utility Methods for File System & Config Access ---

bool HttpRequestHandler::_isRegularFile(const std::string& path) const {
    struct stat fileStat;
    // Check if path exists and is a regular file.
    return stat(path.c_str(), &fileStat) == 0 && S_ISREG(fileStat.st_mode);
}

// Checks if a given path points to a directory.
bool HttpRequestHandler::_isDirectory(const std::string& path) const {
    struct stat fileStat;
    // Check if path exists and is a directory.
    return stat(path.c_str(), &fileStat) == 0 && S_ISDIR(fileStat.st_mode);
}

// Checks if a file or directory exists at the given path.
bool HttpRequestHandler::_fileExists(const std::string& path) const {
    struct stat fileStat;
    // Check if path exists.
    return stat(path.c_str(), &fileStat) == 0;
}

// Checks if a file or directory has read permissions.
bool HttpRequestHandler::_canRead(const std::string& path) const {
    // Check for read access.
    return access(path.c_str(), R_OK) == 0;
}

// Checks if a directory has write permissions.
bool HttpRequestHandler::_canWrite(const std::string& path) const {
    // Check for write access.
    return access(path.c_str(), W_OK) == 0;
}

std::string HttpRequestHandler::_getEffectiveRoot(const ServerConfig* server, const LocationConfig* location) const {
    if (location && !location->root.empty()) {
        return location->root;
    }
    if (server && !server->root.empty()) {
        return server->root;
    }
    return "";
}

std::string HttpRequestHandler::_getEffectiveUploadStore(const ServerConfig* server, const LocationConfig* location) const {
    (void)server; // Suppress unused parameter warning
    if (location && !location->uploadStore.empty()) {
        return location->uploadStore;
    }
    return "";
}

long HttpRequestHandler::_getEffectiveClientMaxBodySize(const ServerConfig* server, const LocationConfig* location) const {
    if (location && location->clientMaxBodySize != 0) {
        return location->clientMaxBodySize;
    }
    if (server && server->clientMaxBodySize != 0) {
        return server->clientMaxBodySize;
    }
    return std::numeric_limits<long>::max(); // Default to a very large size if not specified
}

const std::map<int, std::string>& HttpRequestHandler::_getEffectiveErrorPages(const ServerConfig* server, const LocationConfig* location) const {
    if (location && !location->errorPages.empty()) {
        return location->errorPages;
    }
    if (server) {
        return server->errorPages;
    }
    static const std::map<int, std::string> emptyMap; // Return an empty map if no config
    return emptyMap;
}

std::string HttpRequestHandler::_getMimeType(const std::string& filePath) const {
    size_t dotPos = filePath.rfind('.');
    if (dotPos == std::string::npos) {
        return "application/octet-stream"; // Default for unknown or no extension
    }
    std::string ext = filePath.substr(dotPos);
    std::transform(ext.begin(), ext.end(), ext.begin(), static_cast<int(*)(int)>(std::tolower));

    if (ext == ".html" || ext == ".htm") return "text/html";
    if (ext == ".css") return "text/css";
    if (ext == ".js") return "application/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".txt") return "text/plain";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".png") return "image/png";
    if (ext == ".gif") return "image/gif";
    if (ext == ".ico") return "image/x-icon";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".pdf") return "application/pdf";
    if (ext == ".xml") return "application/xml";
    return "application/octet-stream";
}

// Debug prints added here
HttpResponse HttpRequestHandler::_generateErrorResponse(int statusCode,
                                                         const ServerConfig* serverConfig,
                                                         const LocationConfig* locationConfig) {
    HttpResponse response;
    response.setStatus(statusCode);
    response.addHeader("Content-Type", "text/html");

    std::cerr << "DEBUG: _generateErrorResponse: Generating error " << statusCode << std::endl;

    const std::map<int, std::string>& errorPages = _getEffectiveErrorPages(serverConfig, locationConfig);
    std::map<int, std::string>::const_iterator it = errorPages.find(statusCode);

    if (it != errorPages.end() && !it->second.empty()) {
        std::string effectiveRootForError = _getEffectiveRoot(serverConfig, NULL); // Use server root for error pages unless location specifies one
        std::string customErrorPagePath = effectiveRootForError;
        
        // Ensure only one slash between root and error page path
        if (!customErrorPagePath.empty() && customErrorPagePath[customErrorPagePath.length() - 1] == '/' && it->second[0] == '/') {
            customErrorPagePath += it->second.substr(1);
        } else if (!customErrorPagePath.empty() && customErrorPagePath[customErrorPagePath.length() - 1] != '/' && it->second[0] != '/') {
            customErrorPagePath += "/" + it->second;
        } else {
            customErrorPagePath += it->second;
        }

        std::cout << "DEBUG: _generateErrorResponse: Attempting to serve custom error page from: '" << customErrorPagePath << "'" << std::endl;

        if (_isRegularFile(customErrorPagePath) && _canRead(customErrorPagePath)) {
            std::ifstream file(customErrorPagePath.c_str(), std::ios::in | std::ios::binary);
            if (file.is_open()) {
                std::vector<char> fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                response.setBody(fileContent);
                response.addHeader("Content-Type", _getMimeType(customErrorPagePath));
                file.close();
                std::cout << "DEBUG: _generateErrorResponse: Custom error page '" << customErrorPagePath << "' opened and served successfully." << std::endl;
                return response;
            } else {
                std::cerr << "WARNING: _generateErrorResponse: Failed to open custom error page '" << customErrorPagePath << "', errno: " << strerror(errno) << ". Serving default message." << std::endl;
            }
        } else {
            std::cerr << "WARNING: _generateErrorResponse: Custom error page path '" << customErrorPagePath << "' is not a regular file or not readable. Serving default message." << std::endl;
        }
    } else {
        std::cout << "DEBUG: _generateErrorResponse: No custom error page configured for " << statusCode << " or path is empty. Serving default message." << std::endl;
    }

    // Fallback to default generic error page
    std::ostringstream oss;
    oss << "<html><head><title>Error " << statusCode << "</title></head><body>"
        << "<h1>" << statusCode << " " << response.getStatusMessage() << "</h1>"
        << "<p>The webserv server encountered an error.</p>"
        << "</body></html>";
    response.setBody(oss.str());
    return response;
}

// Debug prints added here
std::string HttpRequestHandler::_resolvePath(const std::string& uriPath,
                                             const ServerConfig* serverConfig,
                                             const LocationConfig* locationConfig) const {
    std::string effectiveRoot = _getEffectiveRoot(serverConfig, locationConfig);
    std::cout << "DEBUG: _resolvePath: URI: '" << uriPath << "', Effective Root: '" << effectiveRoot << "'" << std::endl;

    if (effectiveRoot.empty()) {
        std::cerr << "ERROR: _resolvePath: Effective root is empty." << std::endl;
        return "";
    }

    // Remove trailing slash from effectiveRoot if it exists and is not just "/"
    if (effectiveRoot.length() > 1 && StringUtils::endsWith(effectiveRoot, "/")) {
        effectiveRoot = effectiveRoot.substr(0, effectiveRoot.length() - 1);
        std::cout << "DEBUG: _resolvePath: Trimmed effectiveRoot to: '" << effectiveRoot << "'" << std::endl;
    }

    std::string relativeSuffix = uriPath;

    if (locationConfig) {
        std::cout << "DEBUG: _resolvePath: Using locationConfig with path: '" << locationConfig->path << "'" << std::endl;
        if (StringUtils::startsWith(uriPath, locationConfig->path)) {
            if (StringUtils::endsWith(locationConfig->path, "/")) {
                relativeSuffix = uriPath.substr(locationConfig->path.length());
                if (!relativeSuffix.empty() && relativeSuffix[0] != '/') {
                    relativeSuffix = "/" + relativeSuffix;
                } else if (relativeSuffix.empty() && uriPath == locationConfig->path) {
                    relativeSuffix = "/"; // If URI exactly matches location path ending with '/', treat as root of location
                }
            } else if (uriPath == locationConfig->path) {
                // If URI exactly matches location path without trailing slash (e.g., /html for location /html)
                // Append the last part of the location path to the root.
                // This logic might need refinement depending on desired exact behavior for exact matches.
                size_t lastSlash = locationConfig->path.rfind('/');
                if (lastSlash != std::string::npos) {
                    relativeSuffix = locationConfig->path.substr(lastSlash); // e.g., if /path/to, relativeSuffix becomes /to
                } else {
                    relativeSuffix = "/" + locationConfig->path; // e.g., if /foo, relativeSuffix becomes /foo
                }
            } else {
                relativeSuffix = uriPath.substr(locationConfig->path.length());
                if (!relativeSuffix.empty() && relativeSuffix[0] != '/') {
                    relativeSuffix = "/" + relativeSuffix;
                }
            }
        } else {
            // If URI does not start with locationConfig->path, but a location config is matched,
            // this might indicate a mismatch or a fallback. For simplicity, we make sure it starts with '/'
            if (!relativeSuffix.empty() && relativeSuffix[0] != '/') {
                relativeSuffix = "/" + relativeSuffix;
            }
        }
    } else { // No specific location config matched
        std::cout << "DEBUG: _resolvePath: No specific locationConfig matched." << std::endl;
        if (!relativeSuffix.empty() && relativeSuffix[0] != '/') {
            relativeSuffix = "/" + relativeSuffix;
        } else if (relativeSuffix.empty()) { // Handle requests to root like ""
            relativeSuffix = "/";
        }
    }
    
    std::string finalPath;
    // Handle root path variations:
    if (relativeSuffix == "/" && effectiveRoot.length() > 0 && effectiveRoot[effectiveRoot.length() - 1] != '/') {
        finalPath = effectiveRoot + "/";
    } else {
        finalPath = effectiveRoot + relativeSuffix;
    }

    std::cout << "DEBUG: _resolvePath: Final resolved path: '" << finalPath << "'" << std::endl;
    return finalPath;
}

HttpResponse HttpRequestHandler::_handleGet(const HttpRequest& request,
                                            const ServerConfig* serverConfig,
                                            const LocationConfig* locationConfig) {
    std::cout << "DEBUG: _handleGet: Processing GET request for path: '" << request.path << "'" << std::endl;

    if (!serverConfig) {
        std::cerr << "ERROR: _handleGet: serverConfig is NULL, returning 500." << std::endl;
        return _generateErrorResponse(500, NULL, NULL);
    }

    std::string fullPath = _resolvePath(request.path, serverConfig, locationConfig);
    std::cout << "DEBUG: _handleGet: Resolved fullPath from _resolvePath: '" << fullPath << "'" << std::endl;


    if (fullPath.empty()) {
        std::cerr << "ERROR: _handleGet: Resolved fullPath is empty after _resolvePath, returning 500." << std::endl;
        return _generateErrorResponse(500, serverConfig, locationConfig);
    }

    if (_isDirectory(fullPath)) {
        std::cout << "DEBUG: _handleGet: Path is a directory: '" << fullPath << "'" << std::endl;
        if (!_canRead(fullPath)) {
            std::cerr << "ERROR: _handleGet: Cannot read directory: '" << fullPath << "', returning 403. errno: " << strerror(errno) << std::endl;
            return _generateErrorResponse(403, serverConfig, locationConfig);
        }

        std::vector<std::string> indexFiles;
        if (locationConfig && !locationConfig->indexFiles.empty()) {
            indexFiles = locationConfig->indexFiles;
            std::cout << "DEBUG: _handleGet: Using location-specific index files." << std::endl;
        } else if (serverConfig && !serverConfig->indexFiles.empty()) {
            indexFiles = serverConfig->indexFiles;
            std::cout << "DEBUG: _handleGet: Using server-specific index files." << std::endl;
        } else {
            std::cout << "DEBUG: _handleGet: No index files configured." << std::endl;
        }

        for (size_t i = 0; i < indexFiles.size(); ++i) {
            std::string indexPath = fullPath;
            if (indexPath[indexPath.length() - 1] != '/') {
                indexPath += "/";
            }
            indexPath += indexFiles[i];
            std::cout << "DEBUG: _handleGet: Checking for index file: '" << indexPath << "'" << std::endl;
            
            if (_isRegularFile(indexPath) && _canRead(indexPath)) {
                std::ifstream file(indexPath.c_str(), std::ios::in | std::ios::binary);
                if (file.is_open()) {
                    std::cout << "DEBUG: _handleGet: Found and opened index file: '" << indexPath << "'" << std::endl;
                    HttpResponse response;
                    response.setStatus(200);
                    std::vector<char> fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                    response.setBody(fileContent);
                    response.addHeader("Content-Type", _getMimeType(indexPath));
                    file.close();
                    std::cout << "DEBUG: _handleGet: Successfully served index file: '" << indexPath << "'" << std::endl;
                    return response;
                } else {
                    std::cerr << "ERROR: _handleGet: Found index file '" << indexPath << "' but failed to open it, returning 500. errno: " << strerror(errno) << std::endl;
                    return _generateErrorResponse(500, serverConfig, locationConfig);
                }
            } else {
                std::cout << "DEBUG: _handleGet: Index file '" << indexPath << "' not a regular file or not readable." << std::endl;
            }
        }
        
        bool autoindexEnabled = false;
        if (locationConfig && locationConfig->autoindex) {
            autoindexEnabled = true;
            std::cout << "DEBUG: _handleGet: Autoindex enabled by location config." << std::endl;
        } else if (serverConfig && serverConfig->autoindex) {
            autoindexEnabled = true;
            std::cout << "DEBUG: _handleGet: Autoindex enabled by server config." << std::endl;
        } else {
            std::cout << "DEBUG: _handleGet: Autoindex is disabled." << std::endl;
        }


        if (autoindexEnabled) {
            std::cout << "DEBUG: _handleGet: Generating autoindex page for '" << fullPath << "'" << std::endl;
            HttpResponse response;
            response.setStatus(200);
            response.addHeader("Content-Type", "text/html");
            response.setBody(_generateAutoindexPage(fullPath, request.path));
            return response;
        } else {
            std::cerr << "ERROR: _handleGet: Directory '" << fullPath << "' has no index file and autoindex is off, returning 403." << std::endl;
            return _generateErrorResponse(403, serverConfig, locationConfig);
        }
    } else if (_isRegularFile(fullPath)) {
        std::cout << "DEBUG: _handleGet: Path is a regular file: '" << fullPath << "'" << std::endl;
        if (!_canRead(fullPath)) {
            std::cerr << "ERROR: _handleGet: Cannot read regular file: '" << fullPath << "', returning 403. errno: " << strerror(errno) << std::endl;
            return _generateErrorResponse(403, serverConfig, locationConfig);
        }
        
        std::ifstream file(fullPath.c_str(), std::ios::in | std::ios::binary);
        if (file.is_open()) {
            std::cout << "DEBUG: _handleGet: Opened regular file: '" << fullPath << "'" << std::endl;
            HttpResponse response;
            response.setStatus(200);
            std::vector<char> fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            response.setBody(fileContent);
            response.addHeader("Content-Type", _getMimeType(fullPath));
            file.close();
            std::cout << "DEBUG: _handleGet: Successfully served regular file: '" << fullPath << "'" << std::endl;
            return response;
        } else {
            std::cerr << "ERROR: _handleGet: Regular file '" << fullPath << "' exists but failed to open it, returning 500. errno: " << strerror(errno) << std::endl;
            return _generateErrorResponse(500, serverConfig, locationConfig);
        }
    } else {
        std::cerr << "ERROR: _handleGet: Path '" << fullPath << "' is neither directory nor regular file, returning 404." << std::endl;
        return _generateErrorResponse(404, serverConfig, locationConfig);
    }
}

HttpResponse HttpRequestHandler::_handlePost(const HttpRequest& request,
                                             const ServerConfig* serverConfig,
                                             const LocationConfig* locationConfig) {
    std::string uploadStore = _getEffectiveUploadStore(serverConfig, locationConfig);
    long maxBodySize = _getEffectiveClientMaxBodySize(serverConfig, locationConfig);

    if (uploadStore.empty()) {
        std::cerr << "ERROR: _handlePost: upload_store not configured, returning 500." << std::endl;
        return _generateErrorResponse(500, serverConfig, locationConfig);
    }
    
    if (!_fileExists(uploadStore)) {
        std::cout << "DEBUG: _handlePost: Upload store directory '" << uploadStore << "' does not exist, attempting to create." << std::endl;
        if (mkdir(uploadStore.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0) {
            std::cerr << "ERROR: _handlePost: Failed to create upload store directory '" << uploadStore << "', errno: " << strerror(errno) << ". Returning 500." << std::endl;
            return _generateErrorResponse(500, serverConfig, locationConfig);
        }
        std::cout << "DEBUG: _handlePost: Upload store directory '" << uploadStore << "' created successfully." << std::endl;
    } else if (!_isDirectory(uploadStore)) {
        std::cerr << "ERROR: _handlePost: Upload store path '" << uploadStore << "' exists but is not a directory, returning 500." << std::endl;
        return _generateErrorResponse(500, serverConfig, locationConfig);
    }

    if (!_canWrite(uploadStore)) {
        std::cerr << "ERROR: _handlePost: No write permissions for upload store directory '" << uploadStore << "', returning 403." << std::endl;
        return _generateErrorResponse(403, serverConfig, locationConfig);
    }

    std::string contentLengthStr = request.getHeader("content-length");
    long contentLength = 0;
    if (!contentLengthStr.empty()) {
        try {
            contentLength = StringUtils::stringToLong(contentLengthStr);
            std::cout << "DEBUG: _handlePost: Content-Length: " << contentLength << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "ERROR: _handlePost: Invalid Content-Length header: '" << contentLengthStr << "', returning 400." << std::endl;
            return _generateErrorResponse(400, serverConfig, locationConfig);
        }
    } else {
        if (!request.body.empty()) {
            std::cerr << "ERROR: _handlePost: Request body present but no Content-Length header, returning 411." << std::endl;
            return _generateErrorResponse(411, serverConfig, locationConfig);
        }
    }
    
    if (contentLength > maxBodySize) {
        std::cerr << "ERROR: _handlePost: Request body size (" << contentLength << ") exceeds maxBodySize (" << maxBodySize << "), returning 413." << std::endl;
        return _generateErrorResponse(413, serverConfig, locationConfig);
    }

    std::string originalFilename = "uploaded_file";
    std::string contentDisposition = request.getHeader("content-disposition");
    size_t filenamePos = contentDisposition.find("filename=");
    if (filenamePos != std::string::npos) {
        size_t start = contentDisposition.find('"', filenamePos);
        size_t end = std::string::npos;
        if (start != std::string::npos) {
            start++;
            end = contentDisposition.find('"', start);
        }
        if (start != std::string::npos && end != std::string::npos) {
            originalFilename = contentDisposition.substr(start, end - start);
        }
    }
    
    StringUtils::trim(originalFilename);
    size_t lastSlash = originalFilename.rfind('/');
    if (lastSlash != std::string::npos) {
        originalFilename = originalFilename.substr(lastSlash + 1);
    }
    lastSlash = originalFilename.rfind('\\');
    if (lastSlash != std::string::npos) {
        originalFilename = originalFilename.substr(lastSlash + 1);
    }
    if (originalFilename.find(".." ) != std::string::npos) {
        originalFilename = StringUtils::split(originalFilename, '.')[0]; // Simple sanitization for ".."
        if (originalFilename.empty()) originalFilename = "sanitized_file";
    }
    if (originalFilename.empty()) {
        originalFilename = "unnamed_file";
        std::cout << "DEBUG: _handlePost: Original filename empty, using default 'unnamed_file'." << std::endl;
    }
    std::cout << "DEBUG: _handlePost: Original filename extracted: '" << originalFilename << "'" << std::endl;


    struct timeval tv;
    gettimeofday(&tv, NULL);
    std::ostringstream uniqueNameStream;
    uniqueNameStream << tv.tv_sec << "_" << tv.tv_usec << "_" << originalFilename;
    std::string uniqueFilename = uniqueNameStream.str();
    std::cout << "DEBUG: _handlePost: Unique filename generated: '" << uniqueFilename << "'" << std::endl;

    std::string fullUploadPath = uploadStore;
    if (fullUploadPath[fullUploadPath.length() - 1] != '/') {
        fullUploadPath += "/";
    }
    fullUploadPath += uniqueFilename;
    std::cout << "DEBUG: _handlePost: Full upload path: '" << fullUploadPath << "'" << std::endl;


    std::ofstream outputFile(fullUploadPath.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!outputFile.is_open()) {
        std::cerr << "ERROR: _handlePost: Failed to open output file for writing: '" << fullUploadPath << "', errno: " << strerror(errno) << ". Returning 500." << std::endl;
        return _generateErrorResponse(500, serverConfig, locationConfig);
    }

    if (!request.body.empty()) {
        std::cout << "DEBUG: _handlePost: Writing " << request.body.size() << " bytes to file." << std::endl;
        outputFile.write(request.body.data(), request.body.size());
    } else {
        std::cout << "DEBUG: _handlePost: Request body is empty, not writing data." << std::endl;
    }
    outputFile.close();

    if (outputFile.fail()) {
        std::cerr << "ERROR: _handlePost: File stream failed after writing (e.g., disk full), returning 500. errno: " << strerror(errno) << std::endl;
        return _generateErrorResponse(500, serverConfig, locationConfig);
    }
    std::cout << "DEBUG: _handlePost: File writing completed successfully." << std::endl;


    HttpResponse response;
    response.setStatus(201); // 201 Created
    
    std::string locationHeaderUri = request.uri;
    if (!StringUtils::endsWith(locationHeaderUri, "/")) {
        locationHeaderUri += "/";
    }
    locationHeaderUri += originalFilename; // Use original filename for Location header, not unique one for simplicity

    response.addHeader("Location", locationHeaderUri);
    response.addHeader("Content-Type", "text/html");
    
    std::ostringstream responseBody;
    responseBody << "<html><body><h1>201 Created</h1><p>File uploaded successfully: <a href=\""
                 << locationHeaderUri << "\">" << originalFilename << "</a></p></body></html>";
    response.setBody(responseBody.str());
    std::cout << "DEBUG: _handlePost: Returning 201 Created response." << std::endl;
    return response;
}

HttpResponse HttpRequestHandler::_handleDelete(const HttpRequest& request,
                                               const ServerConfig* serverConfig,
                                               const LocationConfig* locationConfig) {
    std::string fullPath;
    std::cout << "DEBUG: _handleDelete: Processing DELETE request for path: '" << request.path << "'" << std::endl;


    if (locationConfig && !locationConfig->uploadStore.empty() && StringUtils::startsWith(request.path, locationConfig->path)) {
        std::string relativePath = request.path.substr(locationConfig->path.length());
        
        std::string baseUploadPath = locationConfig->uploadStore;
        if (!baseUploadPath.empty() && baseUploadPath[baseUploadPath.length() - 1] != '/') {
            baseUploadPath += "/";
        }

        if (!relativePath.empty() && relativePath[0] == '/') {
            relativePath = relativePath.substr(1);
        }
        fullPath = baseUploadPath + relativePath;
        std::cout << "DEBUG: _handleDelete: Resolved fullPath via uploadStore: '" << fullPath << "'" << std::endl;

    } else {
        fullPath = _resolvePath(request.path, serverConfig, locationConfig);
        std::cout << "DEBUG: _handleDelete: Resolved fullPath via _resolvePath: '" << fullPath << "'" << std::endl;
    }

    if (fullPath.empty()) {
        std::cerr << "ERROR: _handleDelete: Resolved fullPath is empty, returning 500." << std::endl;
        return _generateErrorResponse(500, serverConfig, locationConfig);
    }

    if (!_fileExists(fullPath)) {
        std::cerr << "ERROR: _handleDelete: File to delete '" << fullPath << "' does not exist, returning 404." << std::endl;
        return _generateErrorResponse(404, serverConfig, locationConfig);
    }

    if (!_isRegularFile(fullPath)) {
        std::cerr << "ERROR: _handleDelete: Path '" << fullPath << "' is not a regular file, cannot delete, returning 403." << std::endl;
        return _generateErrorResponse(403, serverConfig, locationConfig);
    }

    size_t lastSlashPos = fullPath.rfind('/');
    std::string parentDir;
    if (lastSlashPos != std::string::npos) {
        parentDir = fullPath.substr(0, lastSlashPos);
    } else {
        parentDir = "/"; // Root directory
    }
    std::cout << "DEBUG: _handleDelete: Parent directory of file: '" << parentDir << "'" << std::endl;


    if (!_canWrite(parentDir)) {
        std::cerr << "ERROR: _handleDelete: No write permissions on parent directory '" << parentDir << "', returning 403." << std::endl;
        return _generateErrorResponse(403, serverConfig, locationConfig);
    }
    
    // It's also good to check if the file itself has write permissions,
    // though parent directory write usually implies ability to unlink.
    if (!_canWrite(fullPath)) { // Check if the file itself is write-protected
        std::cerr << "ERROR: _handleDelete: No write permissions on file '" << fullPath << "', returning 403. errno: " << strerror(errno) << std::endl;
        return _generateErrorResponse(403, serverConfig, locationConfig);
    }

    if (std::remove(fullPath.c_str()) != 0) {
        std::cerr << "ERROR: _handleDelete: Failed to delete file '" << fullPath << "', errno: " << strerror(errno) << ". ";
        if (errno == EACCES || errno == EPERM) {
            std::cerr << "Returning 403." << std::endl;
            return _generateErrorResponse(403, serverConfig, locationConfig);
        } else if (errno == ENOENT) {
            std::cerr << "Returning 404 (file disappeared)." << std::endl;
            return _generateErrorResponse(404, serverConfig, locationConfig);
        }
        std::cerr << "Returning 500." << std::endl;
        return _generateErrorResponse(500, serverConfig, locationConfig);
    }

    std::cout << "DEBUG: _handleDelete: Successfully deleted file: '" << fullPath << "'" << std::endl;
    HttpResponse response;
    response.setStatus(204); // 204 No Content for successful DELETE
    return response;
}

std::string HttpRequestHandler::_generateAutoindexPage(const std::string& directoryPath, const std::string& uriPath) const {
    std::cout << "DEBUG: _generateAutoindexPage: Generating autoindex for directory: '" << directoryPath << "', URI: '" << uriPath << "'" << std::endl;
    std::ostringstream oss;
    oss << "<html><head><title>Index of " << uriPath << "</title>"
        << "<style>"
        << "body { font-family: sans-serif; background-color: #f0f0f0; margin: 2em; }"
        << "h1 { color: #333; }"
        << "ul { list-style-type: none; padding: 0; }"
        << "li { margin-bottom: 0.5em; }"
        << "a { text-decoration: none; color: #007bff; }"
        << "a:hover { text-decoration: underline; }"
        << ".parent-dir { font-weight: bold; color: #dc3545; }"
        << "</style>"
        << "</head><body>"
        << "<h1>Index of " << uriPath << "</h1><ul>";

    DIR *dir;
    struct dirent *ent;

    if ((dir = opendir(directoryPath.c_str())) != NULL) {
        std::cout << "DEBUG: _generateAutoindexPage: Directory opened successfully." << std::endl;
        // Add parent directory link unless at the true root "/"
        if (uriPath != "/") {
            size_t lastSlash = uriPath.rfind('/', uriPath.length() - 2); // Find the slash before the last one
            std::string parentUri = uriPath.substr(0, lastSlash + 1);
            oss << "<li><a href=\"" << parentUri << "\" class=\"parent-dir\">.. (Parent Directory)</a></li>";
        }

        while ((ent = readdir(dir)) != NULL) {
            std::string name = ent->d_name;
            if (name == "." || name == "..") {
                continue; // Skip current and parent directory entries as separate links
            }

            std::string fullEntryPath = directoryPath;
            if (fullEntryPath[fullEntryPath.length() - 1] != '/') {
                fullEntryPath += "/";
            }
            fullEntryPath += name;

            std::string entryUri = uriPath;
            if (entryUri[entryUri.length() - 1] != '/') {
                entryUri += "/";
            }
            entryUri += name;

            oss << "<li><a href=\"" << entryUri;
            if (_isDirectory(fullEntryPath)) {
                oss << "/"; // Append slash for directories in URI
            }
            oss << "\">" << name;
            if (_isDirectory(fullEntryPath)) {
                oss << "/"; // Append slash for directories in display
            }
            oss << "</a></li>";
        }
        closedir(dir);
        std::cout << "DEBUG: _generateAutoindexPage: Directory closed." << std::endl;
    } else {
        std::cerr << "ERROR: _generateAutoindexPage: Could not open directory '" << directoryPath << "', errno: " << strerror(errno) << "." << std::endl;
        oss << "<li>Error: Could not open directory.</li>";
    }

    oss << "</ul></body></html>";
    return oss.str();
}

HttpResponse HttpRequestHandler::handleRequest(const HttpRequest& request, const MatchedConfig& matchedConfig) {
    std::cout << "DEBUG: handleRequest: Starting to handle request for method: '" << request.method << "', path: '" << request.path << "'" << std::endl;

    const ServerConfig* serverConfig = matchedConfig.server_config;
    const LocationConfig* locationConfig = matchedConfig.location_config;

    if (!serverConfig) {
        std::cerr << "ERROR: handleRequest: No serverConfig provided, returning 500." << std::endl;
        return _generateErrorResponse(500, NULL, NULL);
    }

    // Handle return directives (redirects)
    if (locationConfig && locationConfig->returnCode != 0) {
        std::cout << "DEBUG: handleRequest: Found return directive (code: " << locationConfig->returnCode << ", URL: " << locationConfig->returnUrlOrText << "), returning redirect." << std::endl;
        HttpResponse response;
        response.setStatus(locationConfig->returnCode);
        response.addHeader("Location", locationConfig->returnUrlOrText);
        response.setBody("Redirecting to " + locationConfig->returnUrlOrText);
        return response;
    }

    // Determine allowed methods
    std::vector<HttpMethod> allowedMethods;
    if (locationConfig && !locationConfig->allowedMethods.empty()) {
        allowedMethods = locationConfig->allowedMethods;
        std::cout << "DEBUG: handleRequest: Using location-specific allowed methods." << std::endl;
    } else {
        // Default methods if not specified (common for a basic webserv)
        allowedMethods.push_back(HTTP_GET);
        allowedMethods.push_back(HTTP_POST);
        allowedMethods.push_back(HTTP_DELETE);
        std::cout << "DEBUG: handleRequest: Using default allowed methods (GET, POST, DELETE)." << std::endl;
    }

    bool methodAllowed = false;
    HttpMethod reqMethodEnum = HTTP_UNKNOWN;
    if (request.method == "GET") reqMethodEnum = HTTP_GET;
    else if (request.method == "POST") reqMethodEnum = HTTP_POST;
    else if (request.method == "DELETE") reqMethodEnum = HTTP_DELETE;

    for (size_t i = 0; i < allowedMethods.size(); ++i) {
        if (allowedMethods[i] == reqMethodEnum) {
            methodAllowed = true;
            break;
        }
    }

    if (!methodAllowed) {
        std::cerr << "ERROR: handleRequest: Method '" << request.method << "' not allowed for path '" << request.path << "', returning 405." << std::endl;
        HttpResponse response = _generateErrorResponse(405, serverConfig, locationConfig);
        std::string allowHeaderValue;
        for (size_t i = 0; i < allowedMethods.size(); ++i) {
            if (i > 0) allowHeaderValue += ", ";
            if (allowedMethods[i] == HTTP_GET) allowHeaderValue += "GET";
            else if (allowedMethods[i] == HTTP_POST) allowHeaderValue += "POST";
            else if (allowedMethods[i] == HTTP_DELETE) allowHeaderValue += "DELETE";
        }
        response.addHeader("Allow", allowHeaderValue);
        return response;
    }

    // Dispatch based on method
    if (request.method == "GET") {
        return _handleGet(request, serverConfig, locationConfig);
    } else if (request.method == "POST") {
        return _handlePost(request, serverConfig, locationConfig);
    } else if (request.method == "DELETE") {
        return _handleDelete(request, serverConfig, locationConfig);
    } else {
        std::cerr << "ERROR: handleRequest: Method '" << request.method << "' not implemented, returning 501." << std::endl;
        return _generateErrorResponse(501, serverConfig, locationConfig);
    }
}

bool HttpRequestHandler::isCgiRequest(const MatchedConfig& matchedConfig) const {
    const LocationConfig* location = matchedConfig.location_config;
    // Check if location has CGI paths/extensions defined (assuming cgiExecutables stores this mapping)
    return (location && !location->cgiExecutables.empty()); // This implies cgiExecutables is populated from cgi_path/cgi_extension
}