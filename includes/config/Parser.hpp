/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Parser.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bvieilhe <bvieilhe@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/07 19:19:02 by baptistevie       #+#    #+#             */
/*   Updated: 2025/06/28 17:42:04 by bvieilhe         ###   ########.fr       */
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
		ParseError(const std::string& msg, int line, int col);
		
		int getLine() const;
		int getColumn() const;
};

// Parses a stream of tokens into an Abstract Syntax Tree (AST).
class Parser {
	private :
		std::vector<token>  _tokens;
		size_t              _current;

		token       peek(int offset) const;
		token       consume();
		bool        isAtEnd() const;
		bool        checkCurrentType(tokenType type) const;
		token       expectToken(tokenType type, const std::string& context);

		std::vector<ASTnode*>		parseConfig();
		BlockNode *					parseServerBlock();
		BlockNode *					parseLocationBlock();
		DirectiveNode *				parseDirective();
		std::vector<std::string>	parseArgs();

		void	validateDirectiveArguments(DirectiveNode* directive) const;
		bool	isValidDirective(const std::string& name, const std::string& context) const;

		void	error(const std::string& msg) const;
		void	unexpectedToken(const std::string& expected) const;

	public :
		Parser(const std::vector<token>& tokens);
		~Parser();
	
		std::vector<ASTnode*>	parse();
	
		void	cleanupAST(std::vector<ASTnode*>& nodes);
};

#endif
