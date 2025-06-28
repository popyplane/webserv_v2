/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerStructures.hpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bvieilhe <bvieilhe@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/20 17:41:50 by baptistevie       #+#    #+#             */
/*   Updated: 2025/06/28 17:47:52 by bvieilhe         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVERSTRUCTURES_HPP
# define SERVERSTRUCTURES_HPP

#include <string>
#include <vector>
#include <map>
#include <stdexcept>

#include "../http/HttpRequest.hpp"

// Enum for log levels.
enum LogLevel {
	DEBUG_LOG,
	INFO_LOG,
	WARN_LOG,
	ERROR_LOG,
	CRIT_LOG,
	ALERT_LOG,
	EMERG_LOG,
	DEFAULT_LOG
};

// Represents the configuration for a single 'location' block.
struct LocationConfig {
	std::string							root;				// Root directory for this location.
	std::vector<HttpMethod>				allowedMethods;		// List of allowed HTTP methods.
	std::vector<std::string>			indexFiles;			// Default files to serve if URI is a directory.
	bool								autoindex;			// Enable/disable directory listing.
	bool								uploadEnabled;		// Enable/disable file uploads.
	std::string							uploadStore;		// Directory to store uploaded files.
	std::map<std::string, std::string>	cgiExecutables;		// Maps file extensions to CGI executable paths.
	int									returnCode;			// HTTP status code for redirection.
	std::string							returnUrlOrText;	// URL or text for redirection.
	std::string							path;				// URI path for this location.
	std::string							matchType;			// Type of path matching (e.g., prefix, exact).
	std::vector<LocationConfig>			nestedLocations;	// Nested location blocks.
    std::map<int, std::string>			errorPages;			// Custom error pages for this location.
    long								clientMaxBodySize;	// Maximum allowed size for client request bodies.

	// Constructor to set sensible defaults.
	LocationConfig() : root(""), autoindex(false), uploadEnabled(false), uploadStore(""),
					   returnCode(0), path("/"), matchType("") {}
};

// Represents the configuration for a single 'server' block.
struct ServerConfig {
	std::string					host;				// Host to listen on.
	int							port;				// Port to listen on.
	std::vector<std::string>	serverNames;		// List of server names.
	std::map<int, std::string>	errorPages;			// Custom error pages for this server.
	long						clientMaxBodySize;	// Maximum allowed size for client request bodies.
	std::string					errorLogPath;		// Path to the error log file.
	LogLevel					errorLogLevel;		// Log level for error logging.
	std::string					root;				// Default root directory for this server.
	std::vector<std::string>	indexFiles;			// Default index files for this server.
	bool						autoindex;			// Default autoindex setting for this server.
	std::vector<LocationConfig>	locations;			// Location blocks within this server.

	// Constructor to set sensible defaults.
	ServerConfig() : host("0.0.0.0"), port(80), clientMaxBodySize(1048576),
					 errorLogPath(""), errorLogLevel(DEFAULT_LOG),
					 root(""), autoindex(false) {}
};

// Top-level configuration: a list of server blocks.
struct GlobalConfig {
	std::vector<ServerConfig> servers;
};

// Helper functions for parsing string to enum/long.
HttpMethod	stringToHttpMethod(const std::string& methodStr);
LogLevel	stringToLogLevel(const std::string& levelStr);
long		parseSizeToBytes(const std::string& sizeStr);

#endif