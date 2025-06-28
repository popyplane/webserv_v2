#include "webserv.hpp"
#include "config/Lexer.hpp"
#include "config/Parser.hpp"
#include "config/ConfigLoader.hpp"
#include "config/ServerStructures.hpp"
#include "server/Server.hpp"
#include <fstream>
#include <sstream>
#include <vector>
#include <csignal>

volatile sig_atomic_t stopSig = 0;

void handle_signal(int signal) {
    if (signal == SIGINT) {
        std::cout << "Signal SIGINT reçu, arrêt du serveur..." << std::endl;
        stopSig = 1; // Mettre à jour la variable pour indiquer l'arrêt
    }
}

// Main function: Parses configuration and starts the webserv server.
int main(int argc, char **argv) {
    // Validate command line arguments.
    if (argc > 2) {
        std::cerr << "Usage: ./webserv [configuration_file]" << std::endl;
        return 1;
    }

    std::string config_path = (argc == 2) ? argv[1] : "configs/default.conf";
    std::vector<ServerConfig> serverConfigs;

    try {
        // Read configuration file content.
        std::ifstream file(config_path.c_str());
        if (!file.is_open()) {
            throw std::runtime_error("Could not open configuration file: " + config_path);
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string fileContent = buffer.str();

        // Tokenize the configuration file content.
        Lexer lexer(fileContent);
        lexer.lexConf();
        std::vector<token> tokens = lexer.getTokens();

        // Parse the tokens into an Abstract Syntax Tree (AST).
        Parser parser(tokens);
        std::vector<ASTnode*> ast = parser.parse();

        // Load server configurations from the AST.
        ConfigLoader loader;
        serverConfigs = loader.loadConfig(ast);

        // Clean up AST nodes to prevent memory leaks.
        for (size_t i = 0; i < ast.size(); ++i) {
            delete ast[i];
        }

        // Ensure at least one server configuration is loaded.
        if (serverConfigs.empty()) {
            throw std::runtime_error("No server configurations loaded.");
        }

    } catch (const std::exception& e) {
        std::cerr << "Configuration error: " << e.what() << std::endl;
        return 1;
    }

    try {
        // Initialize and run the server with the loaded configurations.
        Server server(serverConfigs);
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Server runtime error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}