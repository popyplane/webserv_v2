/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestDispatcher.hpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: baptistevieilhescaze <baptistevieilhesc    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+            */
/*   Created: 2025/06/24 14:45:07 by baptistevie       #+#    #+#             */
/*   Updated: 2025/06/24 14:45:33 by baptistevie      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef REQUEST_DISPATCHER_HPP
# define REQUEST_DISPATCHER_HPP

#include "../config/ServerStructures.hpp"
#include "HttpRequest.hpp"

#include <string>
#include <vector>
#include <map>
#include <utility>

// Forward declaration to avoid circular dependencies.
class HttpRequest;

// Represents the result of dispatching an HttpRequest to a configuration.
struct MatchedConfig {
    const ServerConfig* server_config; // Pointer to the matched ServerConfig.
    const LocationConfig* location_config; // Pointer to the most specific matched LocationConfig.

    // Constructor to initialize pointers to nullptr.
    MatchedConfig() : server_config(NULL), location_config(NULL) {}
};

// Dispatches incoming HTTP requests to the appropriate server and location configuration.
class RequestDispatcher {
private:
    const GlobalConfig& _globalConfig; // Reference to the loaded global configuration.

    // Finds the best-matching server configuration for a given host and port.
    const ServerConfig* findMatchingServer(const HttpRequest& request,
                                           const std::string& clientHost, int clientPort) const;

    // Finds the most specific location configuration within a server block.
    const LocationConfig* findMatchingLocation(const HttpRequest& request,
                                               const ServerConfig& serverConfig) const;

    // Helper to get the effective root path for a request.
    std::string getEffectiveRoot(const ServerConfig* server, const LocationConfig* location) const;

    // Helper to get the effective client max body size.
    long getEffectiveClientMaxBodySize(const ServerConfig* server, const LocationConfig* location) const;

    // Helper to get the effective error pages map.
    const std::map<int, std::string>& getEffectiveErrorPages(const ServerConfig* server, const LocationConfig* location) const;


public:
    // Constructor: Initializes the RequestDispatcher with the global configuration.
    RequestDispatcher(const GlobalConfig& globalConfig);

    // Dispatches an HTTP request to find the most appropriate server and location configuration.
    MatchedConfig dispatch(const HttpRequest& request, const std::string& clientHost, int clientPort) const;
};

#endif // REQUEST_DISPATCHER_HPP
