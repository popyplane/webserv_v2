#ifndef CONFIG_PRINTER_HPP
# define CONFIG_PRINTER_HPP

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include "ServerStructures.hpp"

namespace ConfigPrinter {

    // Forward declarations for recursive printing.
    void printLocationConfig(std::ostream& os, const LocationConfig& loc, int indentLevel);
    void printServerConfig(std::ostream& os, const ServerConfig& server, int indentLevel);
    
    // Main function to print all loaded server configurations to an output stream.
    void printConfig(std::ostream& os, const std::vector<ServerConfig>& servers);

    // Helper to generate indentation string for pretty printing.
    std::string getIndent(int level);

} // namespace ConfigPrinter

#endif // CONFIG_PRINTER_HPP
