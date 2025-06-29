/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestDispatcher.cpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: baptistevieilhescaze <baptistevieilhesc    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/24 14:46:23 by baptistevie       #+#    #+#             */
/*   Updated: 2025/06/29 01:58:45 by baptistevie      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../../includes/http/RequestDispatcher.hpp"
#include "../../includes/utils/StringUtils.hpp"

#include <iostream>
#include <limits>

RequestDispatcher::RequestDispatcher(const GlobalConfig& globalConfig)
    : _globalConfig(globalConfig) {}

const ServerConfig* RequestDispatcher::findMatchingServer(const HttpRequest& request, const std::string& clientHost, int clientPort) const {
    const ServerConfig* defaultServer = NULL;
    std::string requestHostHeader = request.getHeader("host");
    size_t colonPos = requestHostHeader.find(':');
    if (colonPos != std::string::npos) {
        requestHostHeader = requestHostHeader.substr(0, colonPos);
    }
    StringUtils::toLower(requestHostHeader);

    for (size_t i = 0; i < _globalConfig.servers.size(); ++i) {
        const ServerConfig& currentServer = _globalConfig.servers[i];
        bool hostMatches = (currentServer.host == "0.0.0.0" || currentServer.host == clientHost);
        bool portMatches = (currentServer.port == clientPort);

        if (hostMatches && portMatches) {
            if (!defaultServer) {
                defaultServer = &currentServer;
            }
            for (size_t j = 0; j < currentServer.serverNames.size(); ++j) {
                std::string serverNameLower = currentServer.serverNames[j];
                StringUtils::toLower(serverNameLower);
                if (serverNameLower == requestHostHeader) {
                    return &currentServer;
                }
            }
        }
    }
    return defaultServer;
}

// CORRECTED: Static method definition. No trailing 'const'.
const LocationConfig* RequestDispatcher::findMatchingLocation(const HttpRequest& request, const ServerConfig& serverConfig) {
    const LocationConfig* bestMatch = NULL;
    size_t longestMatchLength = 0;

    for (size_t i = 0; i < serverConfig.locations.size(); ++i) {
        const LocationConfig& currentLocation = serverConfig.locations[i];
        if (request.path.rfind(currentLocation.path, 0) == 0) {
            if (currentLocation.path.length() > longestMatchLength) {
                longestMatchLength = currentLocation.path.length();
                bestMatch = &currentLocation;
            }
        }
    }
    return bestMatch;
}

std::string RequestDispatcher::getEffectiveRoot(const ServerConfig* server, const LocationConfig* location) const {
    if (location && !location->root.empty()) {
        return location->root;
    }
    if (server && !server->root.empty()) {
        return server->root;
    }
    return "";
}

long RequestDispatcher::getEffectiveClientMaxBodySize(const ServerConfig* server, const LocationConfig* location) const {
    if (location && location->clientMaxBodySize != 0) {
        return location->clientMaxBodySize;
    }
    if (server && server->clientMaxBodySize != 0) {
        return server->clientMaxBodySize;
    }
    return std::numeric_limits<long>::max();
}

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

MatchedConfig RequestDispatcher::dispatch(const HttpRequest& request, const std::string& clientHost, int clientPort) const {
    MatchedConfig result;
    result.server_config = findMatchingServer(request, clientHost, clientPort);
    if (result.server_config) {
        result.location_config = RequestDispatcher::findMatchingLocation(request, *result.server_config);
    }
    return result;
}
