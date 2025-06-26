/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Lexer.cpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: baptistevieilhescaze <baptistevieilhesc    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/02 10:47:25 by baptistevie       #+#    #+#             */
/*   Updated: 2025/06/25 08:53:13 by baptistevie      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../../includes/config/Lexer.hpp"

// Reads the content of a file into a string.
bool    readFile(const std::string &fileName, std::string &out)
{
    std::ifstream   file(fileName);
    std::string     buffer;

    if (!file)
        return false;
    while (std::getline(file, buffer))
        out += buffer + "\n";
    return true;
}

// LexerError constructor: Initializes error message, line, and column.
LexerError::LexerError(const std::string& msg, int line, int col) : std::runtime_error(msg), _line(line), _col(col)
{ }
    
// Returns the line number where the error occurred.
int LexerError::getLine() const
{ return (_line); }
    
// Returns the column number where the error occurred.
int LexerError::getColumn() const
{ return (_col); }

// Lexer constructor: Initializes input string and current position.
Lexer::Lexer(const std::string &input) : _input(input), _pos(0), _line(1), _column(1)
{ lexConf(); }

// Lexer destructor.
Lexer::~Lexer()
{}

// Checks if the lexer has reached the end of the input.
bool    Lexer::isAtEnd() const
{ return (_pos >= _input.size()); }

// Peeks at the current character without advancing the position.
char    Lexer::peek() const
{ return (isAtEnd() ? '\0' : _input[_pos]); }

// Gets the current character and advances the position.
char    Lexer::get()
{
    char    c = _input[_pos++];

    if (c == '\n') {
        ++_line;
        _column = 1;
    } else {
        ++_column;
    }
    return (c);
}

// Skips whitespace and comments in the input.
void    Lexer::skipWhitespaceAndComments()
{
    while (!isAtEnd()) {
        char    c = peek();

        if (std::isspace(c)) {
            get();
        } else if (c == '#'){
            // Skip characters until newline or end of input.
            while (!isAtEnd() && peek() != '\n')
                get();
        } else {
            break;
        }
    }
}

// Tokenizes the next significant unit from the input.
token   Lexer::nextToken()
{
    skipWhitespaceAndComments();
    if (isAtEnd())
        return (token(T_EOF, "", _line, _column + 1));

    char    curr = peek();
    
    // Tokenize symbols, strings, identifiers, or numbers.
    if (curr == '{' || curr == '}' || curr == ';')
        return tokeniseSymbol();
    if (curr == '"' || curr == '\'')
        return tokeniseString();
    if (std::isalpha(curr) || curr == '_' || curr == '.' || curr == '-' || curr == '/' || curr == '$')
        return (tokeniseIdentifier());
    if (std::isdigit(curr))
        return (tokeniseNumber());
    
    // Handle unexpected characters.
    std::ostringstream oss;
    oss << "Unexpected char: '" << get() << "' from Lexer::nextToken()";
    error(oss.str());
    return (token(T_EOF, "", -1, -1));
}

// Tokenizes a symbol character.
token   Lexer::tokeniseSymbol()
{
    int         startLn = _line, startCol = _column;
    char    c = get();

    if (c == '{')
        return (token(T_LBRACE, "{", startLn, startCol));
    if (c == '}')
        return (token(T_RBRACE, "}", startLn, startCol));
    if (c == ';')
        return (token(T_SEMICOLON, ";", startLn, startCol));

    std::ostringstream  oss;
    oss << "Unexpected symbol '" << c << "' from Lexer::tokeniseSymbol().";
    throw (LexerError(oss.str(), _line, _column + 1));
}

// Tokenizes a string literal enclosed in quotes.
token   Lexer::tokeniseString()
{
    int         startLn = _line, startCol = _column;
    char        quote = get();
    std::string buffer;

    // Read characters until the closing quote or end of input.
    while (!isAtEnd() && peek() != quote) {
        if (peek() == '\\') {
            get(); // Consume the backslash for escape sequences.
            if (!isAtEnd()) {
                buffer += get();
            } else {
                error("Unterminated string (escape sequence incomplete)");
            }
        } else {
            buffer += get();
        }
    }
    // Consume the closing quote.
    if (peek() == quote) {
        get();
        return (token(T_STRING, buffer, startLn, startCol));
    }

    // Handle unterminated string error.
    error("Unterminated string (missing closing quote)");
    return (token(T_EOF, "", -1, -1));
}

// Tokenizes a number, including potential units (k, m, g).
token   Lexer::tokeniseNumber()
{
    std::string buffer;
    int         startLn = _line, startCol = _column;

    // Read digits, dots, colons, and units.
    while (!isAtEnd()) {
        char c = peek();
        if (std::isdigit(c) || c == '.' || c == ':') {
            buffer += get();
        } else {
            char lower_c = std::tolower(c);
            if (lower_c == 'k' || lower_c == 'm' || lower_c == 'g') {
                buffer += get();
                break;
            } else {
                break;
            }
        }
    }

    return (token(T_NUMBER, buffer, startLn, startCol));
}

// Tokenizes an identifier or a keyword.
token Lexer::tokeniseIdentifier()
{
    std::string buffer;
    int         startLn = _line, startCol = _column;

    // Read alphanumeric characters and specific symbols.
    while (!isAtEnd() && (std::isalnum(peek()) || peek() == '_' || peek() == '.'
                        || peek() == '-' || peek() == ':' || peek() == '/' || peek() == '$'))
        buffer += get();

    // Check for keywords and return appropriate token type.
    if (buffer == "server")                 return (token(T_SERVER, buffer, startLn, startCol));
    if (buffer == "listen")                 return (token(T_LISTEN, buffer, startLn, startCol));
    if (buffer == "server_name")            return (token(T_SERVER_NAME, buffer, startLn, startCol));
    if (buffer == "error_page")             return (token(T_ERROR_PAGE, buffer, startLn, startCol));
    if (buffer == "client_max_body_size")   return (token(T_CLIENT_MAX_BODY, buffer, startLn, startCol));
    if (buffer == "index")                  return (token(T_INDEX, buffer, startLn, startCol));
    if (buffer == "cgi_extension")          return (token(T_CGI_EXTENSION, buffer, startLn, startCol));
    if (buffer == "cgi_path")               return (token(T_CGI_PATH, buffer, startLn, startCol));
    if (buffer == "allowed_methods")        return (token(T_ALLOWED_METHODS, buffer, startLn, startCol));
    if (buffer == "return")                 return (token(T_RETURN, buffer, startLn, startCol));
    if (buffer == "root")                   return (token(T_ROOT, buffer, startLn, startCol));
    if (buffer == "autoindex")              return (token(T_AUTOINDEX, buffer, startLn, startCol));
    if (buffer == "upload_enabled")         return (token(T_UPLOAD_ENABLED, buffer, startLn, startCol));
    if (buffer == "upload_store")           return (token(T_UPLOAD_STORE, buffer, startLn, startCol));
    if (buffer == "location")               return (token(T_LOCATION, buffer, startLn, startCol));
    if (buffer == "error_log")              return (token(T_ERROR_LOG, buffer, startLn, startCol));

    // Return as a generic identifier if not a keyword.
    return (token(T_IDENTIFIER, buffer, startLn, startCol));
}

// Lexes the entire configuration file and stores tokens.
void    Lexer::lexConf()
{
    token   t = nextToken();

    while (t.type != T_EOF) {
        _tokens.push_back(t);
        t = nextToken();
    }
    _tokens.push_back(t); // Add EOF token at the end.
}

// Dumps all tokenized elements to standard output for debugging.
void    Lexer::dumpTokens()
{
    for (size_t i = 0; i < _tokens.size(); i++) {
        std::cout   << tokenTypeToString(_tokens[i].type) << " : ["
                    << _tokens[i].value << "] "
                    << "Ln " << _tokens[i].line
                    << ", Col " << _tokens[i].column
                    << std::endl;
    }
}

// Returns the vector of tokens.
std::vector<token>  Lexer::getTokens() const
{ return (_tokens); }

// Throws a LexerError with the given message and current line/column.
void    Lexer::error(const std::string& msg) const
{ throw (LexerError(msg, _line, _column + 1)); }