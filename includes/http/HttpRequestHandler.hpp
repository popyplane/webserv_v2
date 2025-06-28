#ifndef HTTP_REQUEST_HANDLER_HPP
# define HTTP_REQUEST_HANDLER_HPP

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "RequestDispatcher.hpp"
#include "../utils/StringUtils.hpp"
#include "../config/ServerStructures.hpp"
#include "CGIHandler.hpp"
#include "HttpExceptions.hpp"

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
	HttpRequestHandler();
	~HttpRequestHandler();

	HttpResponse	handleRequest(const HttpRequest& request, const MatchedConfig& matchedConfig);
	bool			isCgiRequest(const MatchedConfig& matchedConfig) const;

	HttpResponse	_generateErrorResponse(int statusCode,
									   const ServerConfig* serverConfig,
									   const LocationConfig* locationConfig);
									   
private:
	std::string							_resolvePath(const std::string& uriPath,
													const ServerConfig* serverConfig,
													const LocationConfig* locationConfig) const;
	HttpResponse						_handleGet(const HttpRequest& request,
													const ServerConfig* serverConfig,
													const LocationConfig* locationConfig);
	HttpResponse						_handlePost(const HttpRequest& request,
													const ServerConfig* serverConfig,
													const LocationConfig* locationConfig);
	HttpResponse						_handleDelete(const HttpRequest& request,
													const ServerConfig* serverConfig,
													const LocationConfig* locationConfig);
	std::string							_generateAutoindexPage(const std::string& directoryPath, const std::string& uriPath) const;
	std::string							_getEffectiveRoot(const ServerConfig* server, const LocationConfig* location) const;
	const std::map<int, std::string>&	_getEffectiveErrorPages(const ServerConfig* server, const LocationConfig* location) const;
	std::string							_getEffectiveUploadStore(const ServerConfig* server, const LocationConfig* location) const;
	long								_getEffectiveClientMaxBodySize(const ServerConfig* server, const LocationConfig* location) const;
	std::string							_getMimeType(const std::string& filePath) const;
	bool								_isRegularFile(const std::string& path) const;
	bool								_isDirectory(const std::string& path) const;
	bool								_fileExists(const std::string& path) const;
	bool								_canRead(const std::string& path) const;
	bool								_canWrite(const std::string& path) const;
};

#endif
