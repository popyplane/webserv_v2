#ifndef CONFIG_LOADER_HPP
# define CONFIG_LOADER_HPP

#include <vector>
#include <string>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <limits>

#include "ASTnode.hpp"
#include "ServerStructures.hpp"
#include "../utils/StringUtils.hpp"

// Loads server configurations from an Abstract Syntax Tree (AST).
class ConfigLoader {
public:
	// Constructor: Initializes the ConfigLoader.
	ConfigLoader();
	// Destructor: Cleans up ConfigLoader resources.
	~ConfigLoader();

	// Main function to load the entire server configuration from the AST.
	std::vector<ServerConfig> loadConfig(const std::vector<ASTnode*>& astNodes);

private:
	// Parses a single 'server' block from its AST node.
	ServerConfig    parseServerBlock(const BlockNode* serverBlockNode);

	// Parses a top-level 'location' block, inheriting defaults from a parent ServerConfig.
	LocationConfig  parseLocationBlock(const BlockNode* locationBlockNode, const ServerConfig& parentServerDefaults);

	// Parses a nested 'location' block, inheriting defaults from a parent LocationConfig.
	LocationConfig  parseLocationBlock(const BlockNode* locationBlockNode, const LocationConfig& parentLocationDefaults);


	// Dispatches a DirectiveNode to the appropriate handler for ServerConfig.
	void            processDirective(const DirectiveNode* directive, ServerConfig& serverConfig);
	// Dispatches a DirectiveNode to the appropriate handler for LocationConfig.
	void            processDirective(const DirectiveNode* directive, LocationConfig& locationConfig);


	// Handles the 'listen' directive for a ServerConfig.
	void            handleListenDirective(const DirectiveNode* directive, ServerConfig& serverConfig);
	// Handles the 'server_name' directive for a ServerConfig.
	void            handleServerNameDirective(const DirectiveNode* directive, ServerConfig& serverConfig);
	// Handles the 'error_log' directive for a ServerConfig.
	void            handleErrorLogDirective(const DirectiveNode* directive, ServerConfig& serverConfig);

	// Handles the 'root' directive for a ServerConfig.
	void            handleRootDirective(const DirectiveNode* directive, ServerConfig& serverConfig);
	// Handles the 'root' directive for a LocationConfig.
	void            handleRootDirective(const DirectiveNode* directive, LocationConfig& locationConfig);
	// Handles the 'index' directive for a ServerConfig.
	void            handleIndexDirective(const DirectiveNode* directive, ServerConfig& serverConfig);
	// Handles the 'index' directive for a LocationConfig.
	void            handleIndexDirective(const DirectiveNode* directive, LocationConfig& locationConfig);
	// Handles the 'autoindex' directive for a ServerConfig.
	void            handleAutoindexDirective(const DirectiveNode* directive, ServerConfig& serverConfig);
	// Handles the 'autoindex' directive for a LocationConfig.
	void            handleAutoindexDirective(const DirectiveNode* directive, LocationConfig& locationConfig);
	// Handles the 'error_page' directive for a ServerConfig.
	void            handleErrorPageDirective(const DirectiveNode* directive, ServerConfig& serverConfig);
	// Handles the 'error_page' directive for a LocationConfig.
	void            handleErrorPageDirective(const DirectiveNode* directive, LocationConfig& locationConfig);
	// Handles the 'client_max_body_size' directive for a ServerConfig.
	void            handleClientMaxBodySizeDirective(const DirectiveNode* directive, ServerConfig& serverConfig);
	// Handles the 'client_max_body_size' directive for a LocationConfig.
	void            handleClientMaxBodySizeDirective(const DirectiveNode* directive, LocationConfig& locationConfig);

	// Handles the 'allowed_methods' directive for a LocationConfig.
	void            handleAllowedMethodsDirective(const DirectiveNode* directive, LocationConfig& locationConfig);
	// Handles the 'upload_enabled' directive for a LocationConfig.
	void            handleUploadEnabledDirective(const DirectiveNode* directive, LocationConfig& locationConfig);
	// Handles the 'upload_store' directive for a LocationConfig.
	void            handleUploadStoreDirective(const DirectiveNode* directive, LocationConfig& locationConfig);
	// Handles the 'cgi_extension' directive for a LocationConfig.
	void            handleCgiExtensionDirective(const DirectiveNode* directive, LocationConfig& locationConfig);
	// Handles the 'cgi_path' directive for a LocationConfig.
	void            handleCgiPathDirective(const DirectiveNode* directive, LocationConfig& locationConfig);
	// Handles the 'return' directive for a LocationConfig.
	void            handleReturnDirective(const DirectiveNode* directive, LocationConfig& locationConfig);


	// Converts a string HTTP method (e.g., "GET") to its HttpMethod enum.
	HttpMethod      stringToHttpMethod(const std::string& methodStr) const;
	// Converts a string log level (e.g., "error") to its LogLevel enum.
	LogLevel        stringToLogLevel(const std::string& levelStr) const;
	// Parses a size string (e.g., "10m", "512k", "2g") into bytes.
	long            parseSizeToBytes(const std::string& sizeStr) const;

	// Throws a ConfigLoadError with the given message, line, and column.
	void            error(const std::string& msg, int line, int col) const;
};

// Custom exception class for configuration loading errors.
class ConfigLoadError : public std::runtime_error {
private:
	int _line;
	int _column;
public:
	// Constructor: Initializes error message, line, and column.
	ConfigLoadError(const std::string& msg, int line = 0, int column = 0)
		: std::runtime_error(msg), _line(line), _column(column) {}
	// Returns the line number where the error occurred.
	int getLine() const { return _line; }
	// Returns the column number where the error occurred.
	int getColumn() const { return _column; }
};

#endif // CONFIG_LOADER_HPP
