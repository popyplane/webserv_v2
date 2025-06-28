/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigLoader.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bvieilhe <bvieilhe@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/23 11:22:17 by baptistevie       #+#    #+#             */
/*   Updated: 2025/06/28 18:28:53 by bvieilhe         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../../includes/config/ConfigLoader.hpp"

// Constructor: Initializes the ConfigLoader.
ConfigLoader::ConfigLoader() {}

// Destructor: Cleans up ConfigLoader resources.
ConfigLoader::~ConfigLoader() {}

// Main function to load the entire server configuration from the AST.
std::vector<ServerConfig>	ConfigLoader::loadConfig(const std::vector<ASTnode *> & astNodes)
{
	std::vector<ServerConfig>	loadedServers;

	// Iterate through top-level AST nodes, expecting server blocks.
	for (size_t i = 0; i < astNodes.size(); ++i) {
		ASTnode * node = astNodes[i];
		BlockNode * serverBlockNode = dynamic_cast<BlockNode *>(node);

		if (serverBlockNode) {
			// If it's a server block, parse it.
			if (serverBlockNode->name == "server") {
				loadedServers.push_back(parseServerBlock(serverBlockNode));
			} else {
				// Handle unexpected block types at the top level.
				error("Unexpected block type '" + serverBlockNode->name + "' at top level. Expected 'server' block.",
					  serverBlockNode->line, serverBlockNode->column);
			}
		} else {
			DirectiveNode* directiveNode = dynamic_cast<DirectiveNode *>(node);

			// Handle unexpected directive nodes or unknown AST node types.
			if (directiveNode) {
				error("Unexpected directive '" + directiveNode->name + "' at top level. Expected 'server' block.",
					   directiveNode->line, directiveNode->column);
			} else {
				error("Unknown AST node type encountered at top level.", node->line, node->column);
			}
		}
	}
	// Safeguard: Ensure at least one server is defined if the config is not empty.
	if (loadedServers.empty() && !astNodes.empty()) {
		error("No valid server blocks found in configuration.", 0, 0);
	}
	return loadedServers;
}

// Parses a single 'server' block from its AST node into a ServerConfig object.
ServerConfig    ConfigLoader::parseServerBlock(const BlockNode * serverBlockNode)
{
	ServerConfig serverConf;

	// Iterate through child nodes of the server block.
	for (size_t i = 0; i < serverBlockNode->children.size(); ++i) {
		ASTnode * childNode = serverBlockNode->children[i];
		DirectiveNode * directive = dynamic_cast<DirectiveNode *>(childNode);
		BlockNode * nestedBlock = dynamic_cast<BlockNode *>(childNode);

		if (directive) {
			// Process directives within the server block.
			processDirective(directive, serverConf);
		} else if (nestedBlock && nestedBlock->name == "location") {
			// Recursively parse nested location blocks.
			serverConf.locations.push_back(parseLocationBlock(nestedBlock, serverConf));
		} else {
			// Handle unexpected child nodes.
			error("Unexpected child node in server block. Expected a directive or 'location' block.",
				  childNode->line, childNode->column);
		}
	}
	// Server-specific semantic validations.
	if (serverConf.port == 0) {
		error("Server block is missing a 'listen' directive or it's invalid.",
			  serverBlockNode->line, serverBlockNode->column);
	}
	if (serverConf.root.empty() && serverConf.locations.empty()) {
		// If there's no root AND no locations, this server can't serve anything.
			error("Server block has no 'root' directive and no 'location' blocks defined. Cannot serve content.",
			  serverBlockNode->line, serverBlockNode->column);
	}
	return (serverConf);
}

// Parses a top-level 'location' block from its AST node into a LocationConfig object.
LocationConfig    ConfigLoader::parseLocationBlock(const BlockNode * locationBlockNode, const ServerConfig & parentServerDefaults)
{
	LocationConfig locationConf;

	// Initialize LocationConfig with inherited defaults from parentServerDefaults.
	locationConf.root = parentServerDefaults.root;
	locationConf.indexFiles = parentServerDefaults.indexFiles;
	locationConf.autoindex = parentServerDefaults.autoindex;
	locationConf.errorPages = parentServerDefaults.errorPages;
	locationConf.clientMaxBodySize = parentServerDefaults.clientMaxBodySize;

	// Load the location block's own arguments (path and matchType).
	if (locationBlockNode->args.empty()) {
		error("Location block requires at least a path argument.",
			  locationBlockNode->line, locationBlockNode->column);
	} else if (locationBlockNode->args.size() == 1) {
		locationConf.path = locationBlockNode->args[0];
		locationConf.matchType = "";
	} else if (locationBlockNode->args.size() == 2) {
		locationConf.matchType = locationBlockNode->args[0];
		locationConf.path = locationBlockNode->args[1];
		if (locationConf.matchType != "=" &&
			locationConf.matchType != "~" &&
			locationConf.matchType != "~*" &&
			locationConf.matchType != "^~") {
			error("Invalid location match type '" + locationConf.matchType + "'. Expected '=', '~', '~*', or '^~'.",
				  locationBlockNode->line, locationBlockNode->column);
		}
	} else {
		error("Location block has too many arguments. Expected a path or a modifier and a path.",
			  locationBlockNode->line, locationBlockNode->column);
	}

	// Iterate and process child directives and nested location blocks.
	for (size_t i = 0; i < locationBlockNode->children.size(); ++i) {
		ASTnode * childNode = locationBlockNode->children[i];
		DirectiveNode * directive = dynamic_cast<DirectiveNode *>(childNode);
		BlockNode * nestedBlock = dynamic_cast<BlockNode *>(childNode);

		if (directive) {
			processDirective(directive, locationConf);
		} else if (nestedBlock && nestedBlock->name == "location") {
			// Recursively call the overload for nested locations.
			locationConf.nestedLocations.push_back(parseLocationBlock(nestedBlock, locationConf));
		} else {
			error("Unexpected child node in location block. Expected a directive or a nested 'location' block.",
				  childNode->line, childNode->column);
		}
	}

	// Location-specific semantic validations.
	if (locationConf.root.empty()) {
		error("Location block is missing a 'root' directive or it's not inherited.",
			  locationBlockNode->line, locationBlockNode->column);
	}
	if (locationConf.uploadEnabled && locationConf.uploadStore.empty()) {
		error("Uploads are enabled but 'upload_store' directive is missing or invalid.",
			  locationBlockNode->line, locationBlockNode->column);
	}
	if (!locationConf.cgiExecutables.empty()) {
		bool cgi_path_missing = false;
		std::map<std::string, std::string>::const_iterator it;
		for (it = locationConf.cgiExecutables.begin(); it != locationConf.cgiExecutables.end(); ++it) {
			if (it->second.empty()) {
				cgi_path_missing = true;
				break;
			}
		}
		if (cgi_path_missing) {
			error("CGI extensions defined but corresponding 'cgi_path' is missing or invalid.",
				  locationBlockNode->line, locationBlockNode->column);
		}
	}
	return (locationConf);
}

// Parses a nested 'location' block from its AST node into a LocationConfig object.
LocationConfig    ConfigLoader::parseLocationBlock(const BlockNode * locationBlockNode, const LocationConfig & parentLocationDefaults)
{
	LocationConfig locationConf;

	// Initialize LocationConfig with inherited defaults from parentLocationDefaults.
	locationConf.root = parentLocationDefaults.root;
	locationConf.indexFiles = parentLocationDefaults.indexFiles;
	locationConf.autoindex = parentLocationDefaults.autoindex;
	locationConf.errorPages = parentLocationDefaults.errorPages;
	locationConf.clientMaxBodySize = parentLocationDefaults.clientMaxBodySize;
	locationConf.allowedMethods = parentLocationDefaults.allowedMethods;
	locationConf.uploadEnabled = parentLocationDefaults.uploadEnabled;
	locationConf.uploadStore = parentLocationDefaults.uploadStore;
	locationConf.cgiExecutables = parentLocationDefaults.cgiExecutables;
	locationConf.returnCode = parentLocationDefaults.returnCode;
	locationConf.returnUrlOrText = parentLocationDefaults.returnUrlOrText;

	// Load the location block's own arguments (path and matchType).
	if (locationBlockNode->args.empty()) {
		error("Location block requires at least a path argument.",
			  locationBlockNode->line, locationBlockNode->column);
	} else if (locationBlockNode->args.size() == 1) {
		locationConf.path = locationBlockNode->args[0];
		locationConf.matchType = "";
	} else if (locationBlockNode->args.size() == 2) {
		locationConf.matchType = locationBlockNode->args[0];
		locationConf.path = locationBlockNode->args[1];
		if (locationConf.matchType != "=" &&
			locationConf.matchType != "~" &&
			locationConf.matchType != "~*" &&
			locationConf.matchType != "^~") {
			error("Invalid location match type '" + locationConf.matchType + "'. Expected '=', '~', '~*', or '^~'.",
				  locationBlockNode->line, locationBlockNode->column);
		}
	} else {
		error("Location block has too many arguments. Expected a path or a modifier and a path.",
			  locationBlockNode->line, locationBlockNode->column);
	}

	// Iterate and process child directives and further nested location blocks.
	for (size_t i = 0; i < locationBlockNode->children.size(); ++i) {
		ASTnode * childNode = locationBlockNode->children[i];
		DirectiveNode * directive = dynamic_cast<DirectiveNode *>(childNode);
		BlockNode * nestedBlock = dynamic_cast<BlockNode *>(childNode);

		if (directive) {
			processDirective(directive, locationConf);
		} else if (nestedBlock && nestedBlock->name == "location") {
			// Recursively call this overload for deeper nested locations.
			locationConf.nestedLocations.push_back(parseLocationBlock(nestedBlock, locationConf));
		} else {
			error("Unexpected child node in location block. Expected a directive or a nested 'location' block.",
				  childNode->line, childNode->column);
		}
	}

	// Location-specific semantic validations.
	if (locationConf.root.empty()) {
		error("Location block is missing a 'root' directive or it's not inherited.",
			  locationBlockNode->line, locationBlockNode->column);
	}
	if (locationConf.uploadEnabled && locationConf.uploadStore.empty()) {
		error("Uploads are enabled but 'upload_store' directive is missing or invalid.",
			  locationBlockNode->line, locationBlockNode->column);
	}
	if (!locationConf.cgiExecutables.empty()) {
		bool cgi_path_missing = false;
		std::map<std::string, std::string>::const_iterator it;
		for (it = locationConf.cgiExecutables.begin(); it != locationConf.cgiExecutables.end(); ++it) {
			if (it->second.empty()) {
				cgi_path_missing = true;
				break;
			}
		}
		if (cgi_path_missing) {
			error("CGI extensions defined but corresponding 'cgi_path' is missing or invalid.",
				  locationBlockNode->line, locationBlockNode->column);
		}
	}
	return (locationConf);
}

// Dispatches a DirectiveNode to the appropriate handler for ServerConfig.
void ConfigLoader::processDirective(const DirectiveNode* directive, ServerConfig& serverConfig) {
	const std::string& name = directive->name;

	// Server-specific directives.
	if (name == "listen") {
		handleListenDirective(directive, serverConfig);
	} else if (name == "server_name") {
		handleServerNameDirective(directive, serverConfig);
	} else if (name == "error_log") {
		handleErrorLogDirective(directive, serverConfig);
	} 
	// Directives common to both Server and Location contexts.
	else if (name == "root") {
		handleRootDirective(directive, serverConfig);
	} else if (name == "index") {
		handleIndexDirective(directive, serverConfig);
	} else if (name == "autoindex") {
		handleAutoindexDirective(directive, serverConfig);
	} else if (name == "error_page") {
		handleErrorPageDirective(directive, serverConfig);
	} else if (name == "client_max_body_size") {
		handleClientMaxBodySizeDirective(directive, serverConfig);
	}
	// Handle unexpected directives.
	else {
		error("Unexpected directive '" + name + "' in server context.",
			  directive->line, directive->column);
	}
}

// Dispatches a DirectiveNode to the appropriate handler for LocationConfig.
void ConfigLoader::processDirective(const DirectiveNode* directive, LocationConfig& locationConfig) {
	const std::string& name = directive->name;

	// Directives common to both Server and Location contexts.
	if (name == "root") {
		handleRootDirective(directive, locationConfig);
	} else if (name == "index") {
		handleIndexDirective(directive, locationConfig);
	} else if (name == "autoindex") {
		handleAutoindexDirective(directive, locationConfig);
	} else if (name == "error_page") {
		handleErrorPageDirective(directive, locationConfig);
	} else if (name == "client_max_body_size") {
		handleClientMaxBodySizeDirective(directive, locationConfig);
	}
	// Location-specific directives.
	else if (name == "allowed_methods") {
		handleAllowedMethodsDirective(directive, locationConfig);
	} else if (name == "upload_enabled") {
		handleUploadEnabledDirective(directive, locationConfig);
	} else if (name == "upload_store") {
		handleUploadStoreDirective(directive, locationConfig);
	} else if (name == "cgi_extension") {
		handleCgiExtensionDirective(directive, locationConfig);
	} else if (name == "cgi_path") {
		handleCgiPathDirective(directive, locationConfig);
	} else if (name == "return") {
		handleReturnDirective(directive, locationConfig);
	}
	// Handle unexpected directives.
	else {
		error("Unexpected directive '" + name + "' in location context.",
			  directive->line, directive->column);
	}
}

// Handles the 'listen' directive for a ServerConfig.
void ConfigLoader::handleListenDirective(const DirectiveNode* directive, ServerConfig& serverConfig) {
	const std::vector<std::string>& args = directive->args;
	
	// Validate argument count.
	if (args.size() != 1) {
		error("Directive 'listen' requires exactly one argument (port or IP:port).",
			  directive->line, directive->column);
	}

	const std::string& listenArg = args[0];
	size_t colon_pos = listenArg.find(':');

	if (colon_pos != std::string::npos) {
		// Case: IP:Port.
		std::string ip_str = listenArg.substr(0, colon_pos);
		std::string port_str = listenArg.substr(colon_pos + 1);

		// Basic IP string validation.
		if (ip_str.empty()) {
			error("Listen directive: IP address part cannot be empty in IP:Port format.",
				  directive->line, directive->column);
		}
		serverConfig.host = ip_str;

		// Port string validation and conversion.
		try {
			if (port_str.empty() || !StringUtils::isDigits(port_str)) {
				throw std::invalid_argument("Port must be a number.");
			}
			long port_long = StringUtils::stringToLong(port_str);
			if (port_long < 1024 || port_long > 65535) {
				throw std::out_of_range("Port number out of valid range (1024-65535).");
			}
			serverConfig.port = static_cast<int>(port_long);
		} catch (const std::invalid_argument& e) {
			error("Listen directive: Invalid port format in IP:Port format. " + std::string(e.what()),
				  directive->line, directive->column);
		} catch (const std::out_of_range& e) {
			error("Listen directive: " + std::string(e.what()) + " in IP:Port format.",
				  directive->line, directive->column);
		}

	} else {
		// Case: Just Port.
		// Port string validation and conversion.
		try {
			if (listenArg.empty() || !StringUtils::isDigits(listenArg)) {
				throw std::invalid_argument("Argument must be a port number.");
			}
			long port_long = StringUtils::stringToLong(listenArg);
			if (port_long < 1 || port_long > 65535) {
				throw std::out_of_range("Port number out of valid range (1-65535).");
			}
			serverConfig.host = "0.0.0.0";
			serverConfig.port = static_cast<int>(port_long);
		} catch (const std::invalid_argument& e) {
			error("Listen directive: Invalid port format. " + std::string(e.what()),
				  directive->line, directive->column);
		} catch (const std::out_of_range& e) {
			error("Listen directive: " + std::string(e.what()),
				  directive->line, directive->column);
		}
	}
}

// Handles the 'server_name' directive for a ServerConfig.
void ConfigLoader::handleServerNameDirective(const DirectiveNode* directive, ServerConfig& serverConfig) {
	const std::vector<std::string>& args = directive->args;

	// Validate argument count.
	if (args.empty()) {
		error("Directive 'server_name' requires at least one argument (hostname).",
			  directive->line, directive->column);
	}

	// Assign all arguments as server names.
	serverConfig.serverNames = args;
}

// Handles the 'error_log' directive for a ServerConfig.
void ConfigLoader::handleErrorLogDirective(const DirectiveNode* directive, ServerConfig& serverConfig) {
	const std::vector<std::string>& args = directive->args;

	// Validate argument count.
	if (args.empty() || args.size() > 2) {
		error("Directive 'error_log' requires one or two arguments: a file path and optional log level.",
			  directive->line, directive->column);
	}

	// First argument is the log file path.
	serverConfig.errorLogPath = args[0];
	// Basic validation: Path should not be empty.
	if (serverConfig.errorLogPath.empty()) {
		error("Error log path cannot be empty.", directive->line, directive->column);
	}

	// If a second argument is provided, it's the log level.
	if (args.size() == 2) {
		try {
			serverConfig.errorLogLevel = stringToLogLevel(args[1]);
		} catch (const std::invalid_argument& e) {
			error("Error log level invalid. " + std::string(e.what()),
				  directive->line, directive->column);
		}
	}
}

// Handles the 'root' directive for a ServerConfig.
void ConfigLoader::handleRootDirective(const DirectiveNode* directive, ServerConfig& serverConfig) {
	const std::vector<std::string>& args = directive->args;

	// Validate argument count.
	if (args.size() != 1) {
		error("Directive 'root' requires exactly one argument (directory path).",
			  directive->line, directive->column);
	}
	// Basic path validation: Should not be empty.
	if (args[0].empty()) {
		error("Root path cannot be empty.", directive->line, directive->column);
	}
	serverConfig.root = args[0];
}

// Handles the 'root' directive for a LocationConfig.
void ConfigLoader::handleRootDirective(const DirectiveNode* directive, LocationConfig& locationConfig) {
	const std::vector<std::string>& args = directive->args;

	if (args.size() != 1) {
		error("Directive 'root' requires exactly one argument (directory path).",
			  directive->line, directive->column);
	}
	if (args[0].empty()) {
		error("Root path cannot be empty.", directive->line, directive->column);
	}
	locationConfig.root = args[0];
}

// Handles the 'index' directive for a ServerConfig.
void ConfigLoader::handleIndexDirective(const DirectiveNode* directive, ServerConfig& serverConfig) {
	const std::vector<std::string>& args = directive->args;

	// Validate argument count.
	if (args.empty()) {
		error("Directive 'index' requires at least one argument (filename).",
			  directive->line, directive->column);
	}
	// Assign all arguments as index files.
	serverConfig.indexFiles = args;
}

// Handles the 'index' directive for a LocationConfig.
void ConfigLoader::handleIndexDirective(const DirectiveNode* directive, LocationConfig& locationConfig) {
	const std::vector<std::string>& args = directive->args;

	if (args.empty()) {
		error("Directive 'index' requires at least one argument (filename).",
			  directive->line, directive->column);
	}
	locationConfig.indexFiles = args;
}

// Handles the 'autoindex' directive for a ServerConfig.
void ConfigLoader::handleAutoindexDirective(const DirectiveNode* directive, ServerConfig& serverConfig) {
	const std::vector<std::string>& args = directive->args;

	// Validate argument count.
	if (args.size() != 1) {
		error("Directive 'autoindex' requires exactly one argument ('on' or 'off').",
			  directive->line, directive->column);
	}
	// Validate argument value.
	if (args[0] == "on") {
		serverConfig.autoindex = true;
	} else if (args[0] == "off") {
		serverConfig.autoindex = false;
	} else {
		error("Argument for 'autoindex' must be 'on' or 'off', but got '" + args[0] + "'.",
			  directive->line, directive->column);
	}
}

// Handles the 'autoindex' directive for a LocationConfig.
void ConfigLoader::handleAutoindexDirective(const DirectiveNode* directive, LocationConfig& locationConfig) {
	const std::vector<std::string>& args = directive->args;

	if (args.size() != 1) {
		error("Directive 'autoindex' requires exactly one argument ('on' or 'off').",
			  directive->line, directive->column);
	}
	if (args[0] == "on") {
		locationConfig.autoindex = true;
	} else if (args[0] == "off") {
		locationConfig.autoindex = false;
	} else {
		error("Argument for 'autoindex' must be 'on' or 'off', but got '" + args[0] + "'.",
			  directive->line, directive->column);
	}
}

// Handles the 'error_page' directive for a ServerConfig.
void ConfigLoader::handleErrorPageDirective(const DirectiveNode* directive, ServerConfig& serverConfig) {
	const std::vector<std::string>& args = directive->args;

	// Validate argument count.
	if (args.size() < 2) {
		error("Directive 'error_page' requires at least two arguments: one or more error codes followed by a URI.",
			  directive->line, directive->column);
	}

	// The last argument is the URI for the error page.
	const std::string& uri = args[args.size() - 1];
	if (uri.empty() || uri[0] != '/') {
		error("Error page URI must be an absolute path (e.g., '/error.html').",
			  directive->line, directive->column);
	}

	// Iterate through the error codes.
	for (size_t i = 0; i < args.size() - 1; ++i) {
		try {
			if (!StringUtils::isDigits(args[i])) {
				throw std::invalid_argument("Error code must be a number.");
			}
			long code_long = StringUtils::stringToLong(args[i]);
			// Validate HTTP status code range.
			if (code_long < 100 || code_long > 599) {
				throw std::out_of_range("Error code out of valid HTTP status code range (100-599).");
			}
			// Add or update the mapping in the errorPages map.
			serverConfig.errorPages[static_cast<int>(code_long)] = uri;
		} catch (const std::invalid_argument& e) {
			error("Error code for 'error_page' invalid: " + std::string(e.what()),
				  directive->line, directive->column);
		} catch (const std::out_of_range& e) {
			error("Error code for 'error_page' " + std::string(e.what()),
				  directive->line, directive->column);
		}
	}
}

// Handles the 'error_page' directive for a LocationConfig.
void ConfigLoader::handleErrorPageDirective(const DirectiveNode* directive, LocationConfig& locationConfig) {
	const std::vector<std::string>& args = directive->args;

	if (args.size() < 2) {
		error("Directive 'error_page' requires at least two arguments: one or more error codes followed by a URI.",
			  directive->line, directive->column);
	}

	const std::string& uri = args[args.size() - 1];
	if (uri.empty() || uri[0] != '/') {
		error("Error page URI must be an absolute path (e.g., '/error.html').",
			  directive->line, directive->column);
	}

	for (size_t i = 0; i < args.size() - 1; ++i) {
		try {
			if (!StringUtils::isDigits(args[i])) {
				throw std::invalid_argument("Error code must be a number.");
			}
			long code_long = StringUtils::stringToLong(args[i]);
			if (code_long < 100 || code_long > 599) {
				throw std::out_of_range("Error code out of valid HTTP status code range (100-599).");
			}
			// This overrides/adds to inherited error pages for specific codes.
			locationConfig.errorPages[static_cast<int>(code_long)] = uri;
		} catch (const std::invalid_argument& e) {
			error("Error code for 'error_page' invalid: " + std::string(e.what()),
				  directive->line, directive->column);
		} catch (const std::out_of_range& e) {
			error("Error code for 'error_page' " + std::string(e.what()),
				  directive->line, directive->column);
		}
	}
}

// Handles the 'client_max_body_size' directive for a ServerConfig.
void ConfigLoader::handleClientMaxBodySizeDirective(const DirectiveNode* directive, ServerConfig& serverConfig) {
	const std::vector<std::string>& args = directive->args;

	// Validate argument count.
	if (args.size() != 1) {
		error("Directive 'client_max_body_size' requires exactly one argument (size with optional units).",
			  directive->line, directive->column);
	}
	try {
		// Use the helper function to parse the size string (e.g., "10m") into bytes.
		serverConfig.clientMaxBodySize = parseSizeToBytes(args[0]);
	} catch (const std::invalid_argument& e) {
		error("Invalid client_max_body_size format: " + std::string(e.what()),
			  directive->line, directive->column);
	} catch (const std::out_of_range& e) {
		error("Client_max_body_size value " + std::string(e.what()),
			  directive->line, directive->column);
	}
}

// Handles the 'client_max_body_size' directive for a LocationConfig.
void ConfigLoader::handleClientMaxBodySizeDirective(const DirectiveNode* directive, LocationConfig& locationConfig) {
	const std::vector<std::string>& args = directive->args;

	if (args.size() != 1) {
		error("Directive 'client_max_body_size' requires exactly one argument (size with optional units).",
			  directive->line, directive->column);
	}
	try {
		locationConfig.clientMaxBodySize = parseSizeToBytes(args[0]);
	} catch (const std::invalid_argument& e) {
		error("Invalid client_max_body_size format: " + std::string(e.what()),
			  directive->line, directive->column);
	} catch (const std::out_of_range& e) {
		error("Client_max_body_size value " + std::string(e.what()),
			  directive->line, directive->column);
	}
}

// Handles the 'allowed_methods' directive for a LocationConfig.
void ConfigLoader::handleAllowedMethodsDirective(const DirectiveNode* directive, LocationConfig& locationConfig) {
	const std::vector<std::string>& args = directive->args;

	// Validate argument count.
	if (args.empty()) {
		error("Directive 'allowed_methods' requires at least one argument (HTTP method).",
			  directive->line, directive->column);
	}

	// Clear any previously allowed methods.
	locationConfig.allowedMethods.clear();
	
	// Convert each argument string to HttpMethod enum and add to the list.
	for (size_t i = 0; i < args.size(); ++i) {
		try {
			locationConfig.allowedMethods.push_back(stringToHttpMethod(args[i]));
		} catch (const std::invalid_argument& e) {
			error("Invalid HTTP method '" + args[i] + "'. " + std::string(e.what()),
				  directive->line, directive->column);
		}
	}
}

// Handles the 'upload_enabled' directive for a LocationConfig.
void ConfigLoader::handleUploadEnabledDirective(const DirectiveNode* directive, LocationConfig& locationConfig) {
	const std::vector<std::string>& args = directive->args;

	// Validate argument count.
	if (args.size() != 1) {
		error("Directive 'upload_enabled' requires exactly one argument ('on' or 'off').",
			  directive->line, directive->column);
	}
	// Validate argument value.
	if (args[0] == "on") {
		locationConfig.uploadEnabled = true;
	} else if (args[0] == "off") {
		locationConfig.uploadEnabled = false;
	} else {
		error("Argument for 'upload_enabled' must be 'on' or 'off', but got '" + args[0] + "'.",
			  directive->line, directive->column);
	}
}

// Handles the 'upload_store' directive for a LocationConfig.
void ConfigLoader::handleUploadStoreDirective(const DirectiveNode* directive, LocationConfig& locationConfig) {
	const std::vector<std::string>& args = directive->args;

	// Validate argument count.
	if (args.size() != 1) {
		error("Directive 'upload_store' requires exactly one argument (directory path).",
			  directive->line, directive->column);
	}
	// Basic path validation.
	if (args[0].empty()) {
		error("Upload store path cannot be empty.", directive->line, directive->column);
	}
	locationConfig.uploadStore = args[0];
}

// Handles the 'cgi_extension' directive for a LocationConfig.
void ConfigLoader::handleCgiExtensionDirective(const DirectiveNode* directive, LocationConfig& locationConfig) {
	const std::vector<std::string>& args = directive->args;

	// Validate argument count.
	if (args.empty()) {
		error("Directive 'cgi_extension' requires at least one argument (file extension).",
			  directive->line, directive->column);
	}

	// Add each specified extension to the cgiExecutables map.
	for (size_t i = 0; i < args.size(); ++i) {
		const std::string& ext = args[i];
		if (ext.empty() || ext[0] != '.') {
			error("CGI extension '" + ext + "' must start with a dot (e.g., '.php').",
				  directive->line, directive->column);
		}
		locationConfig.cgiExecutables[ext] = "";
	}
}

// Handles the 'cgi_path' directive for a LocationConfig.
void ConfigLoader::handleCgiPathDirective(const DirectiveNode* directive, LocationConfig& locationConfig) {
	const std::vector<std::string>& args = directive->args;

	// Validate argument count.
	if (args.size() != 1) {
		error("Directive 'cgi_path' requires exactly one argument (path to CGI executable).",
			  directive->line, directive->column);
	}
	// Basic path validation.
	const std::string& cgi_path = args[0];
	if (cgi_path.empty()) {
		error("CGI path cannot be empty.", directive->line, directive->column);
	}

	// If no CGI extensions have been defined yet, it's an error.
	if (locationConfig.cgiExecutables.empty()) {
		error("Directive 'cgi_path' found without preceding 'cgi_extension' directives.",
			  directive->line, directive->column);
	}

	// Update all currently mapped CGI extensions with this path.
	std::map<std::string, std::string>::iterator it;
	for (it = locationConfig.cgiExecutables.begin(); it != locationConfig.cgiExecutables.end(); ++it) {
		it->second = cgi_path;
	}
}

// Handles the 'return' directive for a LocationConfig.
void ConfigLoader::handleReturnDirective(const DirectiveNode* directive, LocationConfig& locationConfig) {
	const std::vector<std::string>& args = directive->args;

	// Validate argument count: 1 (status_code) or 2 (status_code + URL/text).
	if (args.empty() || args.size() > 2) {
		error("Directive 'return' requires one or two arguments: a status code and optional URL/text.",
			  directive->line, directive->column);
	}

	// First argument is the status code.
	try {
		if (!StringUtils::isDigits(args[0])) {
			throw std::invalid_argument("Status code must be a number.");
		}
		long code_long = StringUtils::stringToLong(args[0]);
		// Validate standard HTTP status code range.
		if (code_long < 100 || code_long > 599) {
			throw std::out_of_range("Status code out of valid HTTP status code range (100-599).");
		}
		locationConfig.returnCode = static_cast<int>(code_long);
	} catch (const std::invalid_argument& e) {
		error("Status code for 'return' invalid: " + std::string(e.what()),
			  directive->line, directive->column);
	} catch (const std::out_of_range& e) {
		error("Status code for 'return' " + std::string(e.what()),
			  directive->line, directive->column);
	}

	// If a second argument is present, it's the URL or text.
	if (args.size() == 2) {
		locationConfig.returnUrlOrText = args[1];
		if (locationConfig.returnUrlOrText.empty()) {
			error("Return URL/text cannot be empty if provided.", directive->line, directive->column);
		}
	} else {
		locationConfig.returnUrlOrText = "";
	}
}

// Converts a string HTTP method (e.g., "GET") to its HttpMethod enum.
HttpMethod ConfigLoader::stringToHttpMethod(const std::string& methodStr) const {
	if (methodStr == "GET") return HTTP_GET;
	if (methodStr == "POST") return HTTP_POST;
	if (methodStr == "DELETE") return HTTP_DELETE;

	throw std::invalid_argument("Unknown HTTP method '" + methodStr + "'.");
}

// Converts a string log level (e.g., "error") to its LogLevel enum.
LogLevel ConfigLoader::stringToLogLevel(const std::string& levelStr) const {
	// Convert to lowercase for case-insensitive comparison.
	std::string lowerLevelStr = levelStr;
	for (size_t i = 0; i < lowerLevelStr.length(); ++i) {
		lowerLevelStr[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowerLevelStr[i])));
	}

	if (lowerLevelStr == "debug") return DEBUG_LOG;
	if (lowerLevelStr == "info") return INFO_LOG;
	if (lowerLevelStr == "warn") return WARN_LOG;
	if (lowerLevelStr == "error") return ERROR_LOG;
	if (lowerLevelStr == "crit") return CRIT_LOG;
	if (lowerLevelStr == "alert") return ALERT_LOG;
	if (lowerLevelStr == "emerg") return EMERG_LOG;

	throw std::invalid_argument("Unknown log level '" + levelStr + "'. Expected debug, info, warn, error, crit, alert, or emerg.");
}

// Parses a size string (e.g., "10m", "512k", "2g") into bytes.
long ConfigLoader::parseSizeToBytes(const std::string& sizeStr) const {
	if (sizeStr.empty()) {
		throw std::invalid_argument("Size string cannot be empty.");
	}

	size_t i = 0;
	while (i < sizeStr.length() && std::isdigit(static_cast<unsigned char>(sizeStr[i]))) {
		i++;
	}

	if (i == 0) {
		throw std::invalid_argument("Size string must start with a number: '" + sizeStr + "'.");
	}

	std::string num_part = sizeStr.substr(0, i);
	std::string unit_part = sizeStr.substr(i);

	long value = 0;
	value = StringUtils::stringToLong(num_part);

	long multiplier = 1;
	if (!unit_part.empty()) {
		if (unit_part.length() > 1) {
			throw std::invalid_argument("Invalid unit format in size string: '" + unit_part + "'. Expected 'k', 'm', or 'g'.");
		}
		char unit = static_cast<char>(std::tolower(static_cast<unsigned char>(unit_part[0])));
		if (unit == 'k') {
			multiplier = 1024;
		} else if (unit == 'm') {
			multiplier = 1024 * 1024;
		} else if (unit == 'g') {
			multiplier = 1024 * 1024 * 1024;
		} else {
			throw std::invalid_argument("Unknown unit '" + unit_part + "'. Expected 'k', 'm', or 'g'.");
		}
	}

	// Check for potential overflow during multiplication.
	if (multiplier > 1 && value > std::numeric_limits<long>::max() / multiplier) {
		throw std::out_of_range("Calculated size (positive overflow) exceeds maximum long value: " + sizeStr);
	}
	if (multiplier > 1 && value < std::numeric_limits<long>::min() / multiplier) {
		throw std::out_of_range("Calculated size (negative overflow) exceeds minimum long value: " + sizeStr);
	}

	return value * multiplier;
}

// Throws a ConfigLoadError with the given message, line, and column.
void ConfigLoader::error(const std::string& msg, int line, int col) const {
	std::ostringstream oss;
	oss << "Config Load Error at line " << line << ", col " << col << ": " << msg;
	throw ConfigLoadError(oss.str(), line, col);
}