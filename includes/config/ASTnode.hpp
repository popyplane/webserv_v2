/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ASTnode.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: baptistevieilhescaze <baptistevieilhesc    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/21 10:25:47 by baptistevie       #+#    #+#             */
/*   Updated: 2025/06/21 10:25:47 by baptistevie      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef ASTNODE_HPP
# define ASTNODE_HPP

# include <string>
# include <vector>

// Base class for all Abstract Syntax Tree (AST) nodes.
class ASTnode {
	public :
		virtual ~ASTnode() {}
		ASTnode() : line(0), column(0) {}

		int	line, column; // Line and column number for error reporting.
};

// Represents a directive in the configuration file (e.g., 'listen 8080;').
class DirectiveNode : public ASTnode {
	public :
		std::string					name; // Name of the directive (e.g., "listen").
		std::vector<std::string>	args; // Arguments of the directive.

		DirectiveNode() : ASTnode() {}
};

// Represents a block in the configuration file (e.g., 'server { ... }').
class BlockNode : public ASTnode {
	public :
		std::string					name; // Name of the block (e.g., "server", "location").
		std::vector<std::string>	args; // Arguments of the block (e.g., location path).
		std::vector<ASTnode*>		children; // Child nodes within the block (directives or nested blocks).

		BlockNode() : ASTnode() {}
};

#endif