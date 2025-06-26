/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   token.hpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: baptistevieilhescaze <baptistevieilhesc    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/02 17:22:40 by baptistevie       #+#    #+#             */
/*   Updated: 2025/06/22 15:56:59 by baptistevie      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */


#ifndef TOKEN_HPP
# define TOKEN_HPP

# include <string>

// Defines the types of tokens recognized by the lexer.
typedef enum tokenType {
	// End of file token.
	T_EOF,

	// Structure symbols.
	T_LBRACE, // '{'
	T_RBRACE, // '}'
	T_SEMICOLON, // ';'

	// Directives (keywords).
	T_SERVER,
	T_LISTEN,
	T_SERVER_NAME,
	T_ERROR_PAGE,
	T_CLIENT_MAX_BODY,
	T_INDEX,
	T_CGI_EXTENSION,
	T_CGI_PATH,
	T_ALLOWED_METHODS,
	T_RETURN,
	T_ROOT,
	T_AUTOINDEX,
	T_UPLOAD_ENABLED,
	T_UPLOAD_STORE,
	T_LOCATION,
	T_ERROR_LOG,

	// Other data/values.
	T_IDENTIFIER, // Generic identifier (e.g., variable names, unquoted strings).
	T_STRING, // Quoted string literal.
	T_NUMBER // Numeric value.
} tokenType;

// Represents a single token extracted by the lexer.
typedef struct token {
	tokenType   type; // The type of the token.
	std::string value; // The string value of the token.
	int         line, column; // Line and column number where the token was found.

	// Constructor: Initializes token properties.
	token(tokenType type, std::string value, int line, int column)
		: type(type), value(value), line(line), column(column)
		{}
} token;

// Converts a tokenType enum value to its string representation for debugging/logging.
const std::string       tokenTypeToString(tokenType type);

#endif