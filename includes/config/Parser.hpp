/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Parser.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: baptistevieilhescaze <baptistevieilhesc    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/07 19:19:02 by baptistevie       #+#    #+#             */
/*   Updated: 2025/06/24 23:08:05 by baptistevie      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef PARSER_HPP
# define PARSER_HPP

# include <sstream>
# include <iostream>
# include <string>
# include <vector>

# include "token.hpp"
# include "Lexer.hpp"
# include "ASTnode.hpp"
# include "ServerStructures.hpp"

// Custom exception class for parser errors.
class ParseError : public std::runtime_error {
    private:
        int _line, _col;

    public:
        // Constructor: Initializes error message, line, and column.
        ParseError(const std::string& msg, int line, int col);
        
        // Returns the line number where the error occurred.
        int getLine() const;
        // Returns the column number where the error occurred.
        int getColumn() const;
};

// Parses a stream of tokens into an Abstract Syntax Tree (AST).
class Parser {
    private :
        std::vector<token>  _tokens; // The input token stream.
        size_t              _current; // Current position in the token stream.

        // Peeks at a token at a given offset from the current position.
        token       peek(int offset) const;
        // Consumes the current token and advances the position.
        token       consume();
        // Checks if the parser has reached the end of the token stream.
        bool        isAtEnd() const;
        // Checks if the current token's type matches the given type.
        bool        checkCurrentType(tokenType type) const;
        // Expects a token of a specific type and consumes it, otherwise throws an error.
        token       expectToken(tokenType type, const std::string& context);

        // Parses the top-level configuration.
        std::vector<ASTnode *>      parseConfig();
        // Parses a 'server' block.
        BlockNode * parseServerBlock();
        // Parses a 'location' block.
        BlockNode * parseLocationBlock();
        // Parses a directive.
        DirectiveNode * parseDirective();
        // Parses arguments for a directive.
        std::vector<std::string>    parseArgs();

        // Validates the arguments of a directive based on its name.
        void                        validateDirectiveArguments(DirectiveNode* directive) const;
        // Checks if a directive name is valid within a given context.
        bool                        isValidDirective(const std::string& name, const std::string& context) const;

        // Throws a ParseError with the given message and current token's line/column.
        void        error(const std::string& msg) const;
        // Throws a ParseError for an unexpected token.
        void        unexpectedToken(const std::string& expected) const;

    public :
        // Constructor: Initializes the parser with a vector of tokens.
        Parser(const std::vector<token>& tokens);
        // Destructor: Cleans up parser resources.
        ~Parser();
    
        // Main function to start parsing and return the AST.
        std::vector<ASTnode *> parse();
    
        // Recursively cleans up the Abstract Syntax Tree (AST) nodes.
        void cleanupAST(std::vector<ASTnode*>& nodes);
};

#endif
