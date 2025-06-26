/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Lexer.hpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: baptistevieilhescaze <baptistevieilhesc    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/28 15:56:14 by baptistevie       #+#    #+#             */
/*   Updated: 2025/06/19 13:53:41 by baptistevie      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef LEXER_HPP
# define LEXER_HPP

# include <string>
# include <fstream>
# include <sstream>
# include <iostream>
# include <cctype>
# include "token.hpp"

// Reads the content of a file into a string.
bool	readFile(const std::string &fileName, std::string &out);

// Custom exception class for lexer errors.
class LexerError : public std::runtime_error {
	private:
		int _line, _col;

	public:
		// Constructor: Initializes error message, line, and column.
		LexerError(const std::string& msg, int line, int col);
		
		// Returns the line number where the error occurred.
		int getLine() const;
		// Returns the column number where the error occurred.
		int getColumn() const;
};

// Tokenizes the input configuration string into a stream of tokens.
class Lexer {
	private:
		const std::string&	_input;			// The input configuration string.
		size_t				_pos; // Current position in the input string.
		int					_line, _column; // Current line and column for error reporting.	
		std::vector<token>	_tokens; // Stores the tokenized output.

		// Lexes the next token from the input.
		token	nextToken();
		// Skips whitespace and comments.
		void	skipWhitespaceAndComments();
		// Tokenizes an identifier.
		token	tokeniseIdentifier();
		// Tokenizes a number.
		token	tokeniseNumber();
		// Tokenizes a string literal.
		token	tokeniseString();
		// Tokenizes a symbol.
		token	tokeniseSymbol();
		// Peeks at the current character without advancing.
		char	peek() const;
		// Consumes and returns the current character, updating line/column.
		char	get();
		// Checks if the lexer has reached the end of the input.
		bool	isAtEnd() const;

		// Throws a LexerError.
		void	error(const std::string& msg) const;

	public:
		// Constructor: Initializes the lexer with the input string.
		Lexer(const std::string &input);
		// Destructor: Cleans up lexer resources.
		~Lexer();

		// Lexes the entire configuration file.
		void				lexConf();
		// Returns the vector of tokens.
		std::vector<token>	getTokens() const;

		// Dumps all tokenized elements to standard output for debugging.
		void				dumpTokens();
};


#endif