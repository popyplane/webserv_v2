/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestDispatcher.cpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: baptistevieilhescaze <baptistevieilhesc    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/24 14:46:23 by baptistevie       #+#    #+#             */
/*   Updated: 2025/06/24 17:17:53 by baptistevie      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../../includes/http/RequestDispatcher.hpp"
#include "../../includes/utils/StringUtils.hpp"

#include <iostream>
#include <limits>

// Constructor: Initializes the dispatcher with the global configuration.
RequestDispatcher::RequestDispatcher(const GlobalConfig& globalConfig)
	: _globalConfig(globalConfig) {}

// Finds the best-matching server configuration for a given host and port.
const ServerConfig* RequestDispatcher::findMatchingServer(const HttpRequest& request, const std::string& clientHost, int clientPort) const {
	const ServerConfig* defaultServer = NULL;

	std::string requestHostHeader = request.getHeader("host");

	// Remove port from Host header if present.
	size_t colonPos = requestHostHeader.find(':');
	if (colonPos != std::string::npos) {
		requestHostHeader = requestHostHeader.substr(0, colonPos);
	}
	// Convert to lowercase for case-insensitive comparison.
	StringUtils::toLower(requestHostHeader);

	// Iterate through all configured servers.
	for (size_t i = 0; i < _globalConfig.servers.size(); ++i) {
		const ServerConfig& currentServer = _globalConfig.servers[i];

		// Check if the server listens on the correct host:port.
		bool hostMatches = (currentServer.host == "0.0.0.0" || currentServer.host == clientHost);
		bool portMatches = (currentServer.port == clientPort);

		if (hostMatches && portMatches) {
			// If this is the first server found for this host:port, it becomes the default.
			if (!defaultServer) {
				defaultServer = &currentServer;
			}

			// Check for server_name match.
			for (size_t j = 0; j < currentServer.serverNames.size(); ++j) {
				std::string serverNameLower = currentServer.serverNames[j];
				StringUtils::toLower(serverNameLower);

				if (serverNameLower == requestHostHeader) {
					// Exact match found, return this server.
					return &currentServer;
				}
			}
		}
	}

	// If no exact server_name match was found, return the default server.
	return defaultServer;
}

// Finds the most specific location configuration within a server block.
const LocationConfig* RequestDispatcher::findMatchingLocation(const HttpRequest& request, const ServerConfig& serverConfig) const {
    const LocationConfig* bestMatch = NULL;
    size_t longestMatchLength = 0;

    // Iterate through all location blocks within the server.
    for (size_t i = 0; i < serverConfig.locations.size(); ++i) {
        const LocationConfig& currentLocation = serverConfig.locations[i];

        // Check if the location's path is a prefix of the request's URI path.
        if (request.path.rfind(currentLocation.path, 0) == 0) {
            // If a longer (more specific) match is found, update bestMatch.
            if (currentLocation.path.length() > longestMatchLength) {
                longestMatchLength = currentLocation.path.length();
                bestMatch = &currentLocation;
            }
        }
    }

    // Return the best matching location or NULL if none found.
    return bestMatch;
}

// Helper to get the effective root path, prioritizing location over server.
std::string RequestDispatcher::getEffectiveRoot(const ServerConfig* server, const LocationConfig* location) const {
	if (location && !location->root.empty()) {
		return location->root;
	}
	if (server && !server->root.empty()) {
		return server->root;
	}
	return "";
}

// Helper to get the effective client max body size, prioritizing location over server.
long RequestDispatcher::getEffectiveClientMaxBodySize(const ServerConfig* server, const LocationConfig* location) const {
	if (location && location->clientMaxBodySize != 0) {
		return location->clientMaxBodySize;
	}
	if (server && server->clientMaxBodySize != 0) {
		return server->clientMaxBodySize;
	}
	return std::numeric_limits<long>::max();
}

// Helper to get the effective error pages map, prioritizing location over server.
const std::map<int, std::string>& RequestDispatcher::getEffectiveErrorPages(const ServerConfig* server, const LocationConfig* location) const {
	if (location && !location->errorPages.empty()) {
		return location->errorPages;
	}
	if (server) {
		return server->errorPages;
	}
	static const std::map<int, std::string> emptyMap;
	return emptyMap;
}

// Dispatches an HTTP request to find the most appropriate server and location configuration.
MatchedConfig RequestDispatcher::dispatch(const HttpRequest& request, const std::string& clientHost, int clientPort) const {
	MatchedConfig result;

	// Find the matching ServerConfig.
	result.server_config = findMatchingServer(request, clientHost, clientPort);

	if (result.server_config) {
		// Find the matching LocationConfig within the selected server.
		result.location_config = findMatchingLocation(request, *result.server_config);
	}

	return result;
}
