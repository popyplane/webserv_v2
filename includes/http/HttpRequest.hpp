/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequest.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bvieilhe <bvieilhe@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/23 15:26:53 by baptistevie       #+#    #+#             */
/*   Updated: 2025/06/28 17:57:00 by bvieilhe         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTPREQUEST_HPP
# define HTTPREQUEST_HPP

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <algorithm>
#include <sstream>

// Enum for HTTP Methods.
enum HttpMethod {
	HTTP_GET,
	HTTP_POST,
	HTTP_DELETE,
	HTTP_UNKNOWN
};

std::string httpMethodToString(HttpMethod method);

class HttpRequest {
public:
	std::string	method;
	std::string	uri;
	std::string	protocolVersion;

	// URI Decomposed Components.
	std::string							path;
	std::map<std::string, std::string>	queryParams;

	std::map<std::string, std::string>	headers;
	std::vector<char>					body;
	size_t								expectedBodyLength;

	// Parsing State.
	typedef enum ParsingState {
		RECV_REQUEST_LINE,
		RECV_HEADERS,
		RECV_BODY,
		COMPLETE,
		ERROR
	} ParsingState;
	ParsingState	currentState;

	HttpRequest();
	std::string	getHeader(const std::string& name) const;
	void		print() const;
};

#endif