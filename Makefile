# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: bvieilhe <bvieilhe@student.42.fr>          +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/06/25 22:41:20 by baptistevie       #+#    #+#              #
#    Updated: 2025/06/28 15:09:11 by bvieilhe         ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

# Compiler and flags
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -I./includes

# Directories
SRCDIR = srcs
CONFIGDIR = $(SRCDIR)/config
HTTPDIR = $(SRCDIR)/http
SERVERDIR = $(SRCDIR)/server
UTILSDIR = $(SRCDIR)/utils

# Source files for the main webserv executable
SRCS = \
	$(SRCDIR)/main.cpp \
	$(CONFIGDIR)/token.cpp \
	$(CONFIGDIR)/Lexer.cpp \
	$(CONFIGDIR)/Parser.cpp \
	$(CONFIGDIR)/ConfigLoader.cpp \
	$(CONFIGDIR)/ConfigPrinter.cpp \
	$(UTILSDIR)/StringUtils.cpp \
	$(HTTPDIR)/HttpRequest.cpp \
	$(HTTPDIR)/HttpRequestParser.cpp \
	$(HTTPDIR)/RequestDispatcher.cpp \
	$(HTTPDIR)/HttpResponse.cpp \
	$(HTTPDIR)/HttpRequestHandler.cpp \
	$(HTTPDIR)/CGIHandler.cpp \
	$(SERVERDIR)/Server.cpp \
	$(SERVERDIR)/Socket.cpp \
	$(SERVERDIR)/Connection.cpp \
	$(SERVERDIR)/Uri.cpp

# Object files for the main webserv executable
OBJS = $(SRCS:.cpp=.o)

# Executable name
NAME = webserv

# Phony targets
.PHONY: all clean fclean re help

# Default target
all: $(NAME)

# Rule to build the main executable
$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJS)

# Rule to compile source files into object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Cleaning rules
clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

# Help
help:
	@echo "Usage: make [target]"
	@echo ""
	@echo "Targets:"
	@echo "  all                 - Build the webserv executable (default)"
	@echo "  clean               - Remove object files and test executables"
	@echo "  fclean              - Remove all generated files, including webserv"
	@echo "  re                  - Rebuild the project"
	@echo "  help                - Show this help message"