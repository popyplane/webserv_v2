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
#include "../config/ServerStructures.hpp"

#include <string>
#include <vector>
#include <map>
#include <utility>

class HttpRequest;

struct MatchedConfig {
	const ServerConfig*		server_config;
	const LocationConfig*	location_config;
	MatchedConfig() : server_config(NULL), location_config(NULL) {}
};

class RequestDispatcher {
private:
	const GlobalConfig&		_globalConfig;

	const ServerConfig*		findMatchingServer(const HttpRequest& request,
										   const std::string& clientHost, int clientPort) const;

	std::string	getEffectiveRoot(const ServerConfig* server, const LocationConfig* location) const;
	long		getEffectiveClientMaxBodySize(const ServerConfig* server, const LocationConfig* location) const;
	const std::map<int, std::string>&	getEffectiveErrorPages(const ServerConfig* server, const LocationConfig* location) const;

public:
	static const LocationConfig*	findMatchingLocation(const HttpRequest& request,
														const ServerConfig& serverConfig); // No 'const' at the end
	RequestDispatcher(const GlobalConfig& globalConfig);
	MatchedConfig	dispatch(const HttpRequest& request, const std::string& clientHost, int clientPort) const;
};

#endif