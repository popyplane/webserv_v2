/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Parser.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: baptistevieilhescaze <baptistevieilhesc    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/09 16:48:56 by baptistevie       #+#    #+#             */
/*   Updated: 2025/06/22 15:56:01 by baptistevie      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../../includes/config/Parser.hpp"

// ParseError constructor: Initializes error message, line, and column.
ParseError::ParseError(const std::string& msg, int line, int col) : std::runtime_error(msg), _line(line), _col(col)
{ }

// Returns the line number where the error occurred.
int ParseError::getLine() const
{ return (_line); }

// Returns the column number where the error occurred.
int ParseError::getColumn() const
{ return (_col); }


// Parser
    // constructor
Parser::Parser(const std::vector<token>& tokens) : _tokens(tokens), _current(0)
{ }

Parser::~Parser()
{ }

    // token management
token   Parser::peek(int offset = 0) const
{
    if (offset < 0)
        throw (std::invalid_argument("peek offset must be non-negative"));
    if (_current + offset >= _tokens.size())
        return (token(T_EOF, "", -1, -1));
    return  (_tokens[_current + offset]);
}
        
token   Parser::consume()
{
    if (_current < _tokens.size())
        ++_current;
    return (_tokens[_current - 1]);
}

bool    Parser::isAtEnd() const
{ return (checkCurrentType(T_EOF)); }
        
bool    Parser::checkCurrentType(tokenType type) const
{ return (peek().type == type); }

token Parser::expectToken(tokenType type, const std::string& context) {
    if (!checkCurrentType(type)) {
        std::ostringstream oss;
        oss << "Expected token type " << tokenTypeToString(type) << " in " << context 
            << ", but got '" << peek().value << "' (type: " << tokenTypeToString(peek().type) << ")";
        error(oss.str());
    }
    return consume();
}
    // parsing methods
std::vector<ASTnode*> Parser::parse()
{
    try {
        return (parseConfig());
    } catch (const ParseError& e) {
        std::cerr   << "Parse Error at line " << e.getLine() << ", col " << e.getColumn() 
                    << ": " << e.what() << std::endl;
        throw;
    }
}

std::vector<ASTnode *>  Parser::parseConfig()
{
    std::vector<ASTnode *>  astNodes;

    while (!isAtEnd()){
        token   current = peek();

        if (checkCurrentType(T_SERVER)) {
            astNodes.push_back(parseServerBlock());
        } else {
            std::stringstream oss;
            oss << "Unexpected token '" << current.value
                << "' (type: " << tokenTypeToString(current.type)
                << ") at top level. Expected 'server' block or end of file.";
            error(oss.str());
        }
    }
    return (astNodes);
}
        
BlockNode * Parser::parseServerBlock()
{
    token   serverToken = expectToken(T_SERVER, "server block definition");
    BlockNode* serverBlock = new BlockNode();

    serverBlock->name = "server";
    serverBlock->line = serverToken.line;
    serverBlock->column = serverToken.column; // Added column

    expectToken(T_LBRACE, "server block opening brace");
    
    // loop through the scope
    while (!checkCurrentType(T_RBRACE) && !isAtEnd()) {
        token current = peek();
        
        if (checkCurrentType(T_LOCATION)) {
            serverBlock->children.push_back(parseLocationBlock());
        } else if (checkCurrentType(T_LISTEN) || checkCurrentType(T_SERVER_NAME) ||
                    checkCurrentType(T_ERROR_PAGE) || checkCurrentType(T_CLIENT_MAX_BODY) ||
                    checkCurrentType(T_INDEX) || checkCurrentType(T_ERROR_LOG) ||
                    checkCurrentType(T_ROOT) || checkCurrentType(T_AUTOINDEX)) { // Added ROOT, AUTOINDEX
            serverBlock->children.push_back(parseDirective());
        } else {
            std::ostringstream oss;
            oss << "Unexpected token '" << current.value 
                << "' (type: " << tokenTypeToString(current.type)
                << ") in server context. Expected 'location' block or a valid directive.";
            error(oss.str());
        }
    }

    if (isAtEnd() && !checkCurrentType(T_RBRACE))
        error("Missing closing brace '}' for server block.");
    
    expectToken(T_RBRACE, "server block closing brace");
    return (serverBlock);
}

BlockNode * Parser::parseLocationBlock()
{
    token       locationToken = expectToken(T_LOCATION, "location block definition");
    BlockNode * locationBlock = new BlockNode();
    
    locationBlock->name = "location";
    locationBlock->line = locationToken.line;
    locationBlock->column = locationToken.column; // Added column

    // MODIFIED: Removed modifier parsing based on subject's "no regexp"
    // The subject implies only simple path matching, not regex or prefix/exact modifiers.
    // So, 'location' should be directly followed by the path.
    // Removed: std::string modifier = parseModifier(); if (!modifier.empty()) locationBlock->args.push_back(modifier);
    
    // get path
    token   pathToken = peek();
    if (checkCurrentType(T_IDENTIFIER) || checkCurrentType(T_STRING)) {
        locationBlock->args.push_back(pathToken.value);
        consume();
    } else {
        // Updated error message to be more specific
        unexpectedToken("location path (identifier or string)");
    }

    expectToken(T_LBRACE, "location block opening brace");;

        // loop through the scope
    while (!isAtEnd() && !checkCurrentType(T_RBRACE)) {
        token   current = peek();

        if (checkCurrentType(T_LOCATION)) { // nested location blocks
            locationBlock->children.push_back(parseLocationBlock());
        } else if (checkCurrentType(T_ALLOWED_METHODS) || checkCurrentType(T_ROOT) || checkCurrentType(T_INDEX)
                    || checkCurrentType(T_AUTOINDEX) || checkCurrentType(T_UPLOAD_ENABLED) || checkCurrentType(T_UPLOAD_STORE)
                    || checkCurrentType(T_CGI_EXTENSION) || checkCurrentType(T_CGI_PATH) || checkCurrentType(T_RETURN)
                    || checkCurrentType(T_ERROR_PAGE) || checkCurrentType(T_CLIENT_MAX_BODY) || checkCurrentType(T_ERROR_LOG)) { // Added ERROR_LOG
            locationBlock->children.push_back(parseDirective());
        } else {
            std::ostringstream oss;
            oss << "Unexpected token '" << current.value 
                << "' (type: " << tokenTypeToString(current.type)
                << ") in location context. Expected a valid directive or nested location block.";
            error(oss.str());
        }
    }
    
    if (isAtEnd() && !checkCurrentType(T_RBRACE))
        error("Missing closing brace '}' for location block.");

    expectToken(T_RBRACE, "location block closing brace");
    return (locationBlock);
}
        
DirectiveNode * Parser::parseDirective()
{
    token   directiveToken = peek();
    
    DirectiveNode* directive = new DirectiveNode();
    directive->name = directiveToken.value;
    directive->line = directiveToken.line;
    directive->column = directiveToken.column; // Added column
    
    consume(); // consume directive name
    
    directive->args = parseArgs();

    validateDirectiveArguments(directive);
    
    expectToken(T_SEMICOLON, "directive ending");
    
    return (directive);
}

std::vector<std::string>    Parser::parseArgs()
{
    std::vector<std::string>    args;

    // MODIFIED: Added T_LBRACE to stop parsing arguments if a block starts
    while (!isAtEnd() && !checkCurrentType(T_SEMICOLON) && !checkCurrentType(T_LBRACE)) {
        if (checkCurrentType(T_STRING) || checkCurrentType(T_NUMBER) || checkCurrentType(T_IDENTIFIER)) {
            args.push_back(peek().value);
            consume();
        } else {
            std::ostringstream oss;
            oss << "Unexpected token '" << peek().value 
                << "' (type: " << tokenTypeToString(peek().type)
                << ") while parsing arguments. Expected string, number, or identifier.";
            error(oss.str());
        }
    }
    return (args);
}
        
// MODIFIED: Removed std::string Parser::parseModifier() function as modifiers are not supported for location paths.
// MODIFIED: Removed bool Parser::isModifier(tokenType type) function as modifiers are not supported for location paths.

bool    Parser::isValidDirective(const std::string& name, const std::string& context) const
{
    if (context == "server") {
        return (name == "listen" || name == "server_name" || name == "error_page" ||
                name == "client_max_body_size" || name == "index" || name == "error_log" ||
                name == "root" || name == "autoindex"); // Added root, autoindex for server context
    }

    if (context == "location") {
        return (name == "allowed_methods" || name == "root" || name == "index" ||
                name == "autoindex" || name == "upload_enabled" || name == "upload_store" ||
                name == "cgi_extension" || name == "cgi_path" || name == "return" ||
                name == "error_page" || name == "client_max_body_size" || name == "error_log"); // Added error_page, client_max_body_size, error_log for location context
    }

    return (false);
}

    // error management
void    Parser::error(const std::string& msg) const
{
    token   current = peek();
    int errorLine = (current.line != -1) ? current.line : 
                    (_current > 0 ? _tokens[_current - 1].line : 0);
    int errorCol = (current.column != -1) ? current.column : 
                   (_current > 0 ? _tokens[_current - 1].column : 0);

    throw (ParseError(msg, errorLine, errorCol));
}
        
void    Parser::unexpectedToken(const std::string& expected) const
{
    std::ostringstream  oss;
    
    oss << "Expected: '" << expected
        << "', but got '" << peek().value
        << "' (type: " << tokenTypeToString(peek().type) << ")";
    error(oss.str());
}

    // cleanup
void    Parser::cleanupAST(std::vector<ASTnode*>& nodes)
{
    // MODIFIED: Iteration using modern C++ style for clarity and safety, compatible with C++98
    for (std::vector<ASTnode*>::iterator it = nodes.begin(); it != nodes.end(); ++it) {
        BlockNode* block = dynamic_cast<BlockNode*>(*it);

        if (block)
            cleanupAST(block->children); // Recursively clean up children
        delete *it; // Delete the current node
    }
    nodes.clear();
}

void Parser::validateDirectiveArguments(DirectiveNode* directive) const {
    const std::vector<std::string>& args = directive->args;
    const std::string& name = directive->name;
    std::ostringstream oss;

    if (name == "listen") {
        if (args.empty()) {
            oss << "Listen directive: requires at least one argument (port or IP:port).";
            error(oss.str());
        }
        std::string host_str = "0.0.0.0"; // Default host, or parsed from args[0]
        std::string port_str;

        size_t colon_pos = args[0].find(":");
        if (colon_pos != std::string::npos) {
            host_str = args[0].substr(0, colon_pos);
            port_str = args[0].substr(colon_pos + 1);
        } else {
            port_str = args[0];
        }

        // Basic host validation (could be more robust with regex for IPs)
        if (host_str.empty()) { // e.g. ":80"
             oss << "Listen directive: Invalid host format.";
             error(oss.str());
        }
        // Port validation
        if (port_str.empty()) {
            oss << "Listen directive: Port cannot be empty.";
            error(oss.str());
        }
        for (size_t i = 0; i < port_str.length(); ++i) {
            if (!std::isdigit(port_str[i])) {
                oss << "Listen directive: Invalid port format. Argument must be a port number.";
                error(oss.str());
            }
        }
        int port_num = std::atoi(port_str.c_str());
        if (port_num < 1 || port_num > 65535) { // Common port range
            oss << "Listen directive: Port number out of valid range (1-65535).";
            error(oss.str());
        }

    } else if (name == "server_name") {
        if (args.empty()) {
            oss << "Directive 'server_name' requires at least one argument (hostname).";
            error(oss.str());
        }
    } else if (name == "error_page") {
        if (args.size() < 2) {
            oss << "Directive 'error_page' requires at least two arguments: one or more error codes followed by a URI.";
            error(oss.str());
        }
        // Validate error codes are numbers and in range. The last argument is the URI.
        for (size_t i = 0; i < args.size() - 1; ++i) { // Loop up to second-to-last argument
            const std::string& code_str = args[i];
            if (code_str.empty()) {
                 oss << "Error code for 'error_page' cannot be empty.";
                 error(oss.str());
            }
            for (size_t j = 0; j < code_str.length(); ++j) {
                if (!std::isdigit(code_str[j])) {
                    oss << "Error code '" << code_str << "' for 'error_page' must be a number.";
                    error(oss.str());
                }
            }
            int code = std::atoi(code_str.c_str());
            if (code < 100 || code > 599) { // Standard HTTP status code range
                oss << "Error code for 'error_page' Error code out of valid HTTP status code range (100-599).";
                error(oss.str());
            }
        }
        // The last argument (URI) is not validated here beyond being a string/identifier.
    } else if (name == "client_max_body_size") {
        if (args.size() != 1) {
            oss << "Directive 'client_max_body_size' requires exactly one argument (size with optional units).";
            error(oss.str());
        }
        std::string size_str = args[0];
        if (size_str.empty()) {
             oss << "Directive 'client_max_body_size' argument cannot be empty.";
             error(oss.str());
        }
        size_t i = 0;
        while (i < size_str.length() && std::isdigit(size_str[i])) {
            i++;
        }
        if (i == 0 && !size_str.empty()) { // Not starting with digit
             oss << "Directive 'client_max_body_size' argument must start with a number.";
             error(oss.str());
        }
        if (i < size_str.length()) { // Has units
            char unit = std::tolower(size_str[i]);
            // MODIFIED: Corrected unit validation to ensure no extra characters after unit
            if (! (unit == 'k' || unit == 'm' || unit == 'g') || (i + 1 < size_str.length())) {
                oss << "Invalid unit or extra characters for 'client_max_body_size' argument: '" << size_str << "'. Expected 'k', 'm', or 'g'.";
                error(oss.str());
            }
        }
    } else if (name == "index") {
        if (args.empty()) {
            oss << "Directive 'index' requires at least one argument (filename).";
            error(oss.str());
        }
    } else if (name == "cgi_extension") {
        if (args.empty()) {
            oss << "Directive 'cgi_extension' requires at least one argument (file extension).";
            error(oss.str());
        }
    } else if (name == "cgi_path") {
        if (args.size() != 1) {
            oss << "Directive 'cgi_path' requires exactly one argument (path to CGI executable).";
            error(oss.str());
        }
        // This validation check is now better handled in ConfigLoader if cgi_path exists without cgi_extension
    } else if (name == "allowed_methods") {
        if (args.empty()) {
            oss << "Directive 'allowed_methods' requires at least one argument (HTTP method).";
            error(oss.str());
        }
        // MODIFIED: Restricted to GET, POST, DELETE as per common interpretation of the subject's requirements
        const char* valid_methods[] = {"GET", "POST", "DELETE"};
        for (size_t i = 0; i < args.size(); ++i) {
            bool found = false;
            for (int j = 0; j < 3; ++j) { // 3 is count of valid_methods
                if (args[i] == valid_methods[j]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                oss << "Invalid HTTP method '" << args[i] << "' for 'allowed_methods'. Expected GET, POST, or DELETE.";
                error(oss.str());
            }
        }
    } else if (name == "return") {
        if (args.empty() || args.size() > 2) {
            oss << "Directive 'return' requires one or two arguments: a status code and optional URL/text.";
            error(oss.str());
        }
        // First arg must be a number (status code)
        const std::string& code_str = args[0];
        if (code_str.empty()) {
             oss << "Status code for 'return' cannot be empty.";
             error(oss.str());
        }
        for (size_t j = 0; j < code_str.length(); ++j) {
            if (!std::isdigit(code_str[j])) {
                oss << "Status code '" << code_str << "' for 'return' must be a number.";
                error(oss.str());
            }
        }
        int code = std::atoi(code_str.c_str());
        if (code < 100 || code > 599) { // Valid HTTP status code range (100-599)
            oss << "Status code out of valid HTTP status code range (100-599).";
            error(oss.str());
        }
        // Second argument (if exists) is a URI/text, no special validation here.
    } else if (name == "root") {
        if (args.size() != 1) {
            oss << "Directive 'root' requires exactly one argument (directory path).";
            error(oss.str());
        }
    } else if (name == "autoindex") {
        if (args.size() != 1) {
            oss << "Directive 'autoindex' requires exactly one argument ('on' or 'off').";
            error(oss.str());
        } else if (args[0] != "on" && args[0] != "off") {
            oss << "Argument for 'autoindex' must be 'on' or 'off', but got '" << args[0] << "'.";
            error(oss.str());
        }
    } else if (name == "upload_enabled") {
        if (args.size() != 1) {
            oss << "Directive 'upload_enabled' requires exactly one argument ('on' or 'off').";
            error(oss.str());
        } else if (args[0] != "on" && args[0] != "off") {
            oss << "Argument for 'upload_enabled' must be 'on' or 'off', but got '" << args[0] << "'.";
            error(oss.str());
        }
    } else if (name == "upload_store") {
        if (args.size() != 1) {
            oss << "Directive 'upload_store' requires exactly one argument (directory path).";
            error(oss.str());
        }
    } else if (name == "error_log") {
        if (args.empty() || args.size() > 2) {
            oss << "Directive 'error_log' requires one or two arguments: a file path and optional log level.";
            error(oss.str());
        }
        if (args.size() == 2) {
            const std::string& level = args[1];
            if (!(level == "debug" || level == "info" || level == "warn" || 
                  level == "error" || level == "crit" || level == "alert" || level == "emerg")) {
                oss << "Invalid log level '" << level << "' for 'error_log'.";
                error(oss.str());
            }
        }
    }
}
