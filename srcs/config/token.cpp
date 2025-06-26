/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   token.cpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: baptistevieilhescaze <baptistevieilhesc    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/09 18:30:46 by baptistevie       #+#    #+#             */
/*   Updated: 2025/06/22 15:58:36 by baptistevie      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../../includes/config/token.hpp"

// Converts a tokenType enum value to its string representation for debugging/logging.
const std::string       tokenTypeToString(tokenType type)
{
	switch (type) {
		// End / errors.
		case T_EOF: return "T_EOF";

		// Structure symbols.
		case T_LBRACE: return "T_LBRACE";
		case T_RBRACE: return "T_RBRACE";
		case T_SEMICOLON: return "T_SEMICOLON";

		// Directives (keywords).
		case T_SERVER: return "T_SERVER";
		case T_LISTEN: return "T_LISTEN";
		case T_SERVER_NAME: return "T_SERVER_NAME";
		case T_ERROR_PAGE: return "T_ERROR_PAGE";
		case T_CLIENT_MAX_BODY: return "T_CLIENT_MAX_BODY";
		case T_INDEX: return "T_INDEX";
		case T_CGI_EXTENSION: return "T_CGI_EXTENSION";
		case T_CGI_PATH: return "T_CGI_PATH";
		case T_ALLOWED_METHODS: return "T_ALLOWED_METHODS";
		case T_RETURN: return "T_RETURN";
		case T_ROOT: return "T_ROOT";
		case T_AUTOINDEX: return "T_AUTOINDEX";
		case T_UPLOAD_ENABLED: return "T_UPLOAD_ENABLED";
		case T_UPLOAD_STORE: return "T_UPLOAD_STORE";
		case T_LOCATION: return "T_LOCATION";
		case T_ERROR_LOG: return "T_ERROR_LOG";

		// Other values.
		case T_IDENTIFIER: return "T_IDENTIFIER";
		case T_STRING: return "T_STRING";
		case T_NUMBER: return "T_NUMBER";

		default: return "UNKNOWN_TOKEN";
	}
}
