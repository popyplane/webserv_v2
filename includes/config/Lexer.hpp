/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Lexer.hpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bvieilhe <bvieilhe@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/28 15:56:14 by baptistevie       #+#    #+#             */
/*   Updated: 2025/06/28 17:40:03 by bvieilhe         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef LEXER_HPP
# define LEXER_HPP

# include <string>
# include <fstream>
# include <sstream>
# include <iostream>
# include <cctype>
# include <vector>
# include "token.hpp"

// Reads the content of a file into a string.
bool	readFile(const std::string &fileName, std::string &out);

// Custom exception class for lexer errors.
class LexerError : public std::runtime_error {
	private:
		int _line, _col;

	public:
		LexerError(const std::string& msg, int line, int col);
		
		int getLine() const;
		int getColumn() const;
};

// Tokenizes the input configuration string into a stream of tokens.
class Lexer {
	private:
		const std::string&	_input;
		size_t				_pos;
		int					_line, _column;
		std::vector<token>	_tokens;

		token	nextToken();
		
		void	skipWhitespaceAndComments();
		token	tokeniseIdentifier();
		token	tokeniseNumber();
		token	tokeniseString();
		token	tokeniseSymbol();
		
		char	peek() const;
		char	get();
		bool	isAtEnd() const;

		void	error(const std::string& msg) const;

	public:
		Lexer(const std::string &input);
		~Lexer();

		void				lexConf();
		std::vector<token>	getTokens() const;

		void				dumpTokens(); // utils
};


#endif