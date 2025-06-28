/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequestParser.hpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bvieilhe <bvieilhe@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/23 15:26:50 by baptistevie       #+#    #+#             */
/*   Updated: 2025/06/28 18:02:29 by bvieilhe         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTP_REQUEST_PARSER_HPP
# define HTTP_REQUEST_PARSER_HPP

#include "HttpRequest.hpp"
#include <vector>
#include <string>

// Macro
const std::string CRLF = "\r\n";
const std::string DOUBLE_CRLF = "\r\n\r\n";

// Parses raw HTTP request data into an HttpRequest object.
class HttpRequestParser {
private:
	HttpRequest			_request;
	std::vector<char>	_buffer;

	void	parseRequestLine();
	void	parseHeaders();
	void	parseBody();
	void	decomposeURI();

	size_t	findInVector(const std::string& pattern);
	void	consumeBuffer(size_t count);
	void	setError(const std::string& msg);

public:
	HttpRequestParser();
	~HttpRequestParser();

	void	appendData(const char* data, size_t len);

	void	parse();

	bool	isComplete() const;
	bool	hasError() const;

	HttpRequest&		getRequest();
	const HttpRequest&	getRequest() const;

	void	reset();
};

#endif
