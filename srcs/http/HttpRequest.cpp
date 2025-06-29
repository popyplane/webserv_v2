/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequest.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: baptistevieilhescaze <baptistevieilhesc    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/23 15:27:00 by baptistevie       #+#    #+#             */
/*   Updated: 2025/06/29 01:52:48 by baptistevie      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../../includes/http/HttpRequest.hpp"
#include <cctype> // For std::isprint

HttpRequest::HttpRequest() : expectedBodyLength(0), currentState(RECV_REQUEST_LINE)
{}

// Retrieves the value of a specified HTTP header (case-insensitive).
std::string HttpRequest::getHeader(const std::string& name) const
{
	std::string lowerName = name;
	// Convert header name to lowercase for case-insensitive lookup.
	for (size_t i = 0; i < lowerName.length(); ++i) {
		lowerName[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowerName[i])));
	}
	std::map<std::string, std::string>::const_iterator it = headers.find(lowerName);
	if (it != headers.end()) {
		return (it->second);
	}
	return (""); // Return empty string if header not found.
}

// Prints the details of the HTTP request to standard output for debugging.
void HttpRequest::print() const
{
	std::cout << "--- HTTP Request ---\n";
	std::cout << "Method: " << method << "\n";
	std::cout << "URI: " << uri << "\n";
	std::cout << "Path: " << path << "\n";
	std::cout << "Protocol Version: " << protocolVersion << "\n";
	std::cout << "Query Parameters:\n";
	for (std::map<std::string, std::string>::const_iterator it = queryParams.begin(); it != queryParams.end(); ++it) {
		std::cout << "  " << it->first << " = " << it->second << "\n";
	}
	std::cout << "Headers:\n";
	for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it) {
		std::cout << "  " << it->first << ": " << it->second << "\n";
	}
	std::cout << "Body Length: " << body.size() << " bytes (Expected: " << expectedBodyLength << ")\n";
	std::cout << "Raw Body Bytes:\n";
	// Print body content, showing printable characters or a dot for non-printable ones.
	if (body.empty()) {
		std::cout << "  (Body is empty)\n";
	} else {
		for (size_t i = 0; i < body.size(); ++i) {
			if (std::isprint(static_cast<unsigned char>(body[i]))) {
				std::cout << "  char[" << i << "]: '" << body[i] << "' (ASCII: " << static_cast<int>(body[i]) << ")\n";
			} else {
				std::cout << "  char[" << i << "]: '.' (Non-printable ASCII: " << static_cast<int>(body[i]) << ")\n";
			}
		}
	}
	std::cout << "Current State: " << static_cast<int>(currentState) << "\n";
	std::cout << "--------------------\n";
}