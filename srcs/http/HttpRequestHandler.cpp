/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequestHandler.cpp                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: baptistevieilhescaze <baptistevieilhesc    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/25 09:57:18 by baptistevie       #+#    #+#             */
/*   Updated: 2025/06/26 10:00:00 by baptistevie      ###   ########.fr       */
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
#include <string.h>

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
    (void)server;
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
    return std::numeric_limits<long>::max();
}

const std::map<int, std::string>& HttpRequestHandler::_getEffectiveErrorPages(const ServerConfig* server, const LocationConfig* location) const {
    if (location && !location->errorPages.empty()) {
        return location->errorPages;
    }
    if (server) {
        return server->errorPages;
    }
    static const std::map<int, std::string> emptyMap;
    return emptyMap;
}

std::string HttpRequestHandler::_getMimeType(const std::string& filePath) const {
    size_t dotPos = filePath.rfind('.');
    if (dotPos == std::string::npos) {
        return "application/octet-stream";
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

HttpResponse HttpRequestHandler::_generateErrorResponse(int statusCode,
                                                         const ServerConfig* serverConfig,
                                                         const LocationConfig* locationConfig) {
    HttpResponse response;
    response.setStatus(statusCode);
    response.addHeader("Content-Type", "text/html");

    const std::map<int, std::string>& errorPages = _getEffectiveErrorPages(serverConfig, locationConfig);
    std::map<int, std::string>::const_iterator it = errorPages.find(statusCode);

    if (it != errorPages.end() && !it->second.empty()) {
        std::string effectiveRootForError = _getEffectiveRoot(serverConfig, NULL);
        std::string customErrorPagePath = effectiveRootForError;
        if (!customErrorPagePath.empty() && customErrorPagePath[customErrorPagePath.length() - 1] == '/') {
            customErrorPagePath = customErrorPagePath.substr(0, customErrorPagePath.length() - 1);
        }
        customErrorPagePath += it->second;

        if (_isRegularFile(customErrorPagePath) && _canRead(customErrorPagePath)) {
            std::ifstream file(customErrorPagePath.c_str(), std::ios::in | std::ios::binary);
            if (file.is_open()) {
                std::vector<char> fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                response.setBody(fileContent);
                response.addHeader("Content-Type", _getMimeType(customErrorPagePath));
                file.close();
                return response;
            }
        }
    }

    std::ostringstream oss;
    oss << "<html><head><title>Error " << statusCode << "</title></head><body>"
        << "<h1>" << statusCode << " " << response.getStatusMessage() << "</h1>"
        << "<p>The webserv server encountered an error.</p>"
        << "</body></html>";
    response.setBody(oss.str());
    return response;
}

std::string HttpRequestHandler::_resolvePath(const std::string& uriPath,
                                             const ServerConfig* serverConfig,
                                             const LocationConfig* locationConfig) const {
    std::string effectiveRoot = _getEffectiveRoot(serverConfig, locationConfig);
    if (effectiveRoot.empty()) {
        return "";
    }

    if (effectiveRoot.length() > 1 && StringUtils::endsWith(effectiveRoot, "/")) {
        effectiveRoot = effectiveRoot.substr(0, effectiveRoot.length() - 1);
    }

    std::string relativeSuffix = uriPath;

    if (locationConfig) {
        if (StringUtils::startsWith(uriPath, locationConfig->path)) {
            if (StringUtils::endsWith(locationConfig->path, "/")) {
                relativeSuffix = uriPath.substr(locationConfig->path.length());
                if (!relativeSuffix.empty() && relativeSuffix[0] != '/') {
                    relativeSuffix = "/" + relativeSuffix;
                } else if (relativeSuffix.empty() && uriPath == locationConfig->path) {
                    relativeSuffix = "/";
                }
            } else if (uriPath == locationConfig->path) {
                size_t lastSlash = locationConfig->path.rfind('/');
                if (lastSlash != std::string::npos) {
                    relativeSuffix = locationConfig->path.substr(lastSlash);
                } else {
                    relativeSuffix = "/" + locationConfig->path;
                }
            } else {
                relativeSuffix = uriPath.substr(locationConfig->path.length());
                if (!relativeSuffix.empty() && relativeSuffix[0] != '/') {
                    relativeSuffix = "/" + relativeSuffix;
                }
            }
        } else {
            if (!relativeSuffix.empty() && relativeSuffix[0] != '/') {
                relativeSuffix = "/" + relativeSuffix;
            }
        }
    } else {
        if (!relativeSuffix.empty() && relativeSuffix[0] != '/') {
            relativeSuffix = "/" + relativeSuffix;
        } else if (relativeSuffix.empty()) {
            relativeSuffix = "/";
        }
    }
    
    if (relativeSuffix == "/" && effectiveRoot.empty()) {
        return "/";
    }
    if (relativeSuffix == "/" && effectiveRoot.length() > 0 && effectiveRoot[effectiveRoot.length() - 1] != '/') {
        return effectiveRoot + "/";
    }
    return effectiveRoot + relativeSuffix;
}

HttpResponse HttpRequestHandler::_handleGet(const HttpRequest& request,
                                            const ServerConfig* serverConfig,
                                            const LocationConfig* locationConfig) {
    if (!serverConfig) {
        return _generateErrorResponse(500, NULL, NULL);
    }

    std::string fullPath = _resolvePath(request.path, serverConfig, locationConfig);

    if (fullPath.empty()) {
        return _generateErrorResponse(500, serverConfig, locationConfig);
    }

    if (_isDirectory(fullPath)) {
        if (!_canRead(fullPath)) {
            return _generateErrorResponse(403, serverConfig, locationConfig);
        }

        std::vector<std::string> indexFiles;
        if (locationConfig && !locationConfig->indexFiles.empty()) {
            indexFiles = locationConfig->indexFiles;
        } else if (serverConfig && !serverConfig->indexFiles.empty()) {
            indexFiles = serverConfig->indexFiles;
        }

        for (size_t i = 0; i < indexFiles.size(); ++i) {
            std::string indexPath = fullPath;
            if (indexPath[indexPath.length() - 1] != '/') {
                indexPath += "/";
            }
            indexPath += indexFiles[i];
            
            if (_isRegularFile(indexPath) && _canRead(indexPath)) {
                std::ifstream file(indexPath.c_str(), std::ios::in | std::ios::binary);
                if (file.is_open()) {
                    HttpResponse response;
                    response.setStatus(200);
                    std::vector<char> fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                    response.setBody(fileContent);
                    response.addHeader("Content-Type", _getMimeType(indexPath));
                    file.close();
                    return response;
                }
            }
        }
        
        bool autoindexEnabled = false;
        if (locationConfig && locationConfig->autoindex) {
            autoindexEnabled = true;
        } else if (serverConfig && serverConfig->autoindex) {
            autoindexEnabled = true;
        }

        if (autoindexEnabled) {
            HttpResponse response;
            response.setStatus(200);
            response.addHeader("Content-Type", "text/html");
            response.setBody(_generateAutoindexPage(fullPath, request.path));
            return response;
        } else {
            return _generateErrorResponse(403, serverConfig, locationConfig);
        }
    } else if (_isRegularFile(fullPath)) {
        if (!_canRead(fullPath)) {
            return _generateErrorResponse(403, serverConfig, locationConfig);
        }
        
        std::ifstream file(fullPath.c_str(), std::ios::in | std::ios::binary);
        if (file.is_open()) {
            HttpResponse response;
            response.setStatus(200);
            std::vector<char> fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            response.setBody(fileContent);
            response.addHeader("Content-Type", _getMimeType(fullPath));
            file.close();
            return response;
        } else {
            return _generateErrorResponse(500, serverConfig, locationConfig);
        }
    } else {
        return _generateErrorResponse(404, serverConfig, locationConfig);
    }
}

HttpResponse HttpRequestHandler::_handlePost(const HttpRequest& request,
                                             const ServerConfig* serverConfig,
                                             const LocationConfig* locationConfig) {
    std::string uploadStore = _getEffectiveUploadStore(serverConfig, locationConfig);
    long maxBodySize = _getEffectiveClientMaxBodySize(serverConfig, locationConfig);

    if (uploadStore.empty()) {
        return _generateErrorResponse(500, serverConfig, locationConfig);
    }
    
    if (!_fileExists(uploadStore)) {
        if (mkdir(uploadStore.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0) {
            return _generateErrorResponse(500, serverConfig, locationConfig);
        }
    } else if (!_isDirectory(uploadStore)) {
        return _generateErrorResponse(500, serverConfig, locationConfig);
    }

    if (!_canWrite(uploadStore)) {
        return _generateErrorResponse(403, serverConfig, locationConfig);
    }

    std::string contentLengthStr = request.getHeader("content-length");
    long contentLength = 0;
    if (!contentLengthStr.empty()) {
        try {
            contentLength = StringUtils::stringToLong(contentLengthStr);
        } catch (const std::exception& e) {
            return _generateErrorResponse(400, serverConfig, locationConfig);
        }
    } else {
        if (!request.body.empty()) {
            return _generateErrorResponse(411, serverConfig, locationConfig);
        }
    }
    
    if (contentLength > maxBodySize) {
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
        originalFilename = StringUtils::split(originalFilename, '.')[0];
        if (originalFilename.empty()) originalFilename = "sanitized_file";
    }
    if (originalFilename.empty()) {
        originalFilename = "unnamed_file";
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);
    std::ostringstream uniqueNameStream;
    uniqueNameStream << tv.tv_sec << "_" << tv.tv_usec << "_" << originalFilename;
    std::string uniqueFilename = uniqueNameStream.str();
    
    std::string fullUploadPath = uploadStore;
    if (fullUploadPath[fullUploadPath.length() - 1] != '/') {
        fullUploadPath += "/";
    }
    fullUploadPath += uniqueFilename;

    std::ofstream outputFile(fullUploadPath.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!outputFile.is_open()) {
        return _generateErrorResponse(500, serverConfig, locationConfig);
    }

    if (!request.body.empty()) {
        outputFile.write(request.body.data(), request.body.size());
    }
    outputFile.close();

    if (outputFile.fail()) {
        return _generateErrorResponse(500, serverConfig, locationConfig);
    }

    HttpResponse response;
    response.setStatus(201);
    
    std::string locationHeaderUri = request.uri;
    if (!StringUtils::endsWith(locationHeaderUri, "/")) {
        locationHeaderUri += "/";
    }
    locationHeaderUri += originalFilename;

    response.addHeader("Location", locationHeaderUri);
    response.addHeader("Content-Type", "text/html");
    
    std::ostringstream responseBody;
    responseBody << "<html><body><h1>201 Created</h1><p>File uploaded successfully: <a href=\""
                 << locationHeaderUri << "\">" << originalFilename << "</a></p></body></html>";
    response.setBody(responseBody.str());
    return response;
}

HttpResponse HttpRequestHandler::_handleDelete(const HttpRequest& request,
                                               const ServerConfig* serverConfig,
                                               const LocationConfig* locationConfig) {
    std::string fullPath;

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

    } else {
        fullPath = _resolvePath(request.path, serverConfig, locationConfig);
    }

    if (fullPath.empty()) {
        return _generateErrorResponse(500, serverConfig, locationConfig);
    }

    if (!_fileExists(fullPath)) {
        return _generateErrorResponse(404, serverConfig, locationConfig);
    }

    if (!_isRegularFile(fullPath)) {
        return _generateErrorResponse(403, serverConfig, locationConfig);
    }

    size_t lastSlashPos = fullPath.rfind('/');
    std::string parentDir;
    if (lastSlashPos != std::string::npos) {
        parentDir = fullPath.substr(0, lastSlashPos);
    } else {
        parentDir = "/";
    }

    if (!_canWrite(parentDir)) {
        return _generateErrorResponse(403, serverConfig, locationConfig);
    }
    
    if (!_canWrite(fullPath)) {
        return _generateErrorResponse(403, serverConfig, locationConfig);
    }

    if (std::remove(fullPath.c_str()) != 0) {
        if (errno == EACCES) {
            return _generateErrorResponse(403, serverConfig, locationConfig);
        } else if (errno == ENOENT) {
            return _generateErrorResponse(404, serverConfig, locationConfig);
        } else if (errno == EPERM) {
            return _generateErrorResponse(403, serverConfig, locationConfig);
        }
        return _generateErrorResponse(500, serverConfig, locationConfig);
    }

    HttpResponse response;
    response.setStatus(204);
    return response;
}

std::string HttpRequestHandler::_generateAutoindexPage(const std::string& directoryPath, const std::string& uriPath) const {
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
        if (uriPath != "/") {
            size_t lastSlash = uriPath.rfind('/', uriPath.length() - 2);
            std::string parentUri = uriPath.substr(0, lastSlash + 1);
            oss << "<li><a href=\"" << parentUri << "\" class=\"parent-dir\">.. (Parent Directory)</a></li>";
        }

        while ((ent = readdir(dir)) != NULL) {
            std::string name = ent->d_name;
            if (name == "." || name == "..") {
                continue;
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
                oss << "/";
            }
            oss << "\">" << name;
            if (_isDirectory(fullEntryPath)) {
                oss << "/";
            }
            oss << "</a></li>";
        }
        closedir(dir);
    } else {
        oss << "<li>Error: Could not open directory.</li>";
    }

    oss << "</ul></body></html>";
    return oss.str();
}

HttpResponse HttpRequestHandler::handleRequest(const HttpRequest& request, const MatchedConfig& matchedConfig) {
    const ServerConfig* serverConfig = matchedConfig.server_config;
    const LocationConfig* locationConfig = matchedConfig.location_config;

    if (!serverConfig) {
        return _generateErrorResponse(500, NULL, NULL);
    }

    if (locationConfig && locationConfig->returnCode != 0) {
        HttpResponse response;
        response.setStatus(locationConfig->returnCode);
        response.addHeader("Location", locationConfig->returnUrlOrText);
        response.setBody("Redirecting to " + locationConfig->returnUrlOrText);
        return response;
    }

    std::vector<HttpMethod> allowedMethods;
    if (locationConfig && !locationConfig->allowedMethods.empty()) {
        allowedMethods = locationConfig->allowedMethods;
    } else {
        allowedMethods.push_back(HTTP_GET);
        allowedMethods.push_back(HTTP_POST);
        allowedMethods.push_back(HTTP_DELETE);
    }

    bool methodAllowed = false;
    HttpMethod reqMethodEnum;
    if (request.method == "GET") reqMethodEnum = HTTP_GET;
    else if (request.method == "POST") reqMethodEnum = HTTP_POST;
    else if (request.method == "DELETE") reqMethodEnum = HTTP_DELETE;
    else reqMethodEnum = HTTP_UNKNOWN;

    for (size_t i = 0; i < allowedMethods.size(); ++i) {
        if (allowedMethods[i] == reqMethodEnum) {
            methodAllowed = true;
            break;
        }
    }

    if (!methodAllowed) {
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

    if (request.method == "GET") {
        return _handleGet(request, serverConfig, locationConfig);
    } else if (request.method == "POST") {
        return _handlePost(request, serverConfig, locationConfig);
    } else if (request.method == "DELETE") {
        return _handleDelete(request, serverConfig, locationConfig);
    } else {
        return _generateErrorResponse(501, serverConfig, locationConfig);
    }
}

bool HttpRequestHandler::isCgiRequest(const MatchedConfig& matchedConfig) const {
    const LocationConfig* location = matchedConfig.location_config;
    return (location && !location->cgiExecutables.empty());
}