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
	ConfigLoader();
	~ConfigLoader();

	std::vector<ServerConfig>	loadConfig(const std::vector<ASTnode*>& astNodes);

private:
	ServerConfig	parseServerBlock(const BlockNode* serverBlockNode);
	LocationConfig	parseLocationBlock(const BlockNode* locationBlockNode, const ServerConfig& parentServerDefaults);
	LocationConfig	parseLocationBlock(const BlockNode* locationBlockNode, const LocationConfig& parentLocationDefaults);

	void	processDirective(const DirectiveNode* directive, ServerConfig& serverConfig);
	void	processDirective(const DirectiveNode* directive, LocationConfig& locationConfig);


	void	handleListenDirective(const DirectiveNode* directive, ServerConfig& serverConfig);
	void	handleServerNameDirective(const DirectiveNode* directive, ServerConfig& serverConfig);
	void	handleErrorLogDirective(const DirectiveNode* directive, ServerConfig& serverConfig);
	void	handleRootDirective(const DirectiveNode* directive, ServerConfig& serverConfig);
	void	handleRootDirective(const DirectiveNode* directive, LocationConfig& locationConfig);
	void	handleIndexDirective(const DirectiveNode* directive, ServerConfig& serverConfig);
	void	handleIndexDirective(const DirectiveNode* directive, LocationConfig& locationConfig);
	void	handleAutoindexDirective(const DirectiveNode* directive, ServerConfig& serverConfig);
	void	handleAutoindexDirective(const DirectiveNode* directive, LocationConfig& locationConfig);
	void	handleErrorPageDirective(const DirectiveNode* directive, ServerConfig& serverConfig);
	void	handleErrorPageDirective(const DirectiveNode* directive, LocationConfig& locationConfig);
	void	handleClientMaxBodySizeDirective(const DirectiveNode* directive, ServerConfig& serverConfig);
	void	handleClientMaxBodySizeDirective(const DirectiveNode* directive, LocationConfig& locationConfig);

	void	handleAllowedMethodsDirective(const DirectiveNode* directive, LocationConfig& locationConfig);
	void	handleUploadEnabledDirective(const DirectiveNode* directive, LocationConfig& locationConfig);
	void	handleUploadStoreDirective(const DirectiveNode* directive, LocationConfig& locationConfig);
	void	handleCgiExtensionDirective(const DirectiveNode* directive, LocationConfig& locationConfig);
	void	handleCgiPathDirective(const DirectiveNode* directive, LocationConfig& locationConfig);
	void	handleReturnDirective(const DirectiveNode* directive, LocationConfig& locationConfig);


	HttpMethod	stringToHttpMethod(const std::string& methodStr) const;
	LogLevel	stringToLogLevel(const std::string& levelStr) const;
	long		parseSizeToBytes(const std::string& sizeStr) const;

	void	error(const std::string& msg, int line, int col) const;
};

// Custom exception class for configuration loading errors.
class ConfigLoadError : public std::runtime_error {
private:
	int	_line;
	int	_column;

public:
	ConfigLoadError(const std::string& msg, int line = 0, int column = 0)
		: std::runtime_error(msg), _line(line), _column(column) {}

	int	getLine() const { return _line; }
	int	getColumn() const { return _column; }
};

#endif
