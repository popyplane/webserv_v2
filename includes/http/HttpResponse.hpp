/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpResponse.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bvieilhe <bvieilhe@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/25 09:50:26 by baptistevie       #+#    #+#             */
/*   Updated: 2025/06/28 18:13:35 by bvieilhe         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTP_RESPONSE_HPP
# define HTTP_RESPONSE_HPP

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <ctime>

std::string getHttpStatusMessage(int statusCode);
std::string getMimeType(const std::string& filePath);

// Represents an HTTP response to be sent back to a client.
class HttpResponse {
public:
	HttpResponse();
	~HttpResponse();

	void	setStatus(int code);
	void	addHeader(const std::string& name, const std::string& value);
	void	setBody(const std::string& content);
	void	setBody(const std::vector<char>& content);

	std::string	toString() const;

	// Getters for Response Components.
	int	getStatusCode() const { return _statusCode; }
	const std::string&	getStatusMessage() const { return _statusMessage; }
	const std::string&	getProtocolVersion() const { return _protocolVersion; }
	const std::map<std::string, std::string>&	getHeaders() const { return _headers; }
	const std::vector<char>&	getBody() const { return _body; }

private:
	std::string							_protocolVersion;	// e.g., "HTTP/1.1".
	int									_statusCode;		// e.g., 200, 404.
	std::string							_statusMessage;		// e.g., "OK", "Not Found".
	std::map<std::string, std::string>	_headers;			// Header names are typically canonical.
	std::vector<char>					_body;				// Use std::vector<char> for the body to handle binary data safely.

	std::string	getCurrentGmTime() const;
	void		setDefaultHeaders();
};

#endif
