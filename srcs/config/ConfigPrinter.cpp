#include "../../includes/config/ConfigPrinter.hpp"
#include <algorithm> // For std::transform
#include <cctype>    // For std::tolower
#include <iomanip>   // For std::fixed, std::setprecision (optional)
#include "../../includes/http/HttpRequest.hpp" // for httpMethodToString()

namespace ConfigPrinter {

    // Helper to generate indentation string for pretty printing.
    std::string getIndent(int level) {
        std::string indent = "";
        for (int i = 0; i < level; ++i) {
            indent += "    "; // 4 spaces per level
        }
        return indent;
    }

    // Helper to convert LogLevel enum to string for display.
    std::string logLevelToString(LogLevel level) {
        switch (level) {
            case DEBUG_LOG: return "debug";
            case INFO_LOG: return "info";
            case WARN_LOG: return "warn";
            case ERROR_LOG: return "error";
            case CRIT_LOG: return "crit";
            case ALERT_LOG: return "alert";
            case EMERG_LOG: return "emerg";
            case DEFAULT_LOG: return "default";
            default: return "invalid_log_level";
        }
    }

    // Prints a single LocationConfig object recursively to the output stream.
    void printLocationConfig(std::ostream& os, const LocationConfig& loc, int indentLevel) {
        std::string indent = getIndent(indentLevel);
        os << indent << "Location Block: ";
        if (!loc.matchType.empty()) {
            os << "Match Type: '" << loc.matchType << "', ";
        }
        os << "Path: '" << loc.path << "'\n";
        
        os << indent << "    Root: '" << loc.root << "'\n";
        
        os << indent << "    Index Files: [";
        for (size_t i = 0; i < loc.indexFiles.size(); ++i) {
            os << "'" << loc.indexFiles[i] << "'";
            if (i < loc.indexFiles.size() - 1) os << ", ";
        }
        os << "]\n";

        os << indent << "    Autoindex: " << (loc.autoindex ? "on" : "off") << "\n";
        
        os << indent << "    Allowed Methods: [";
        for (size_t i = 0; i < loc.allowedMethods.size(); ++i) {
            os << httpMethodToString(loc.allowedMethods[i]);
            if (i < loc.allowedMethods.size() - 1) os << ", ";
        }
        os << "]\n";

        os << indent << "    Upload Enabled: " << (loc.uploadEnabled ? "on" : "off") << "\n";
        os << indent << "    Upload Store: '" << loc.uploadStore << "'\n";

        os << indent << "    CGI Executables:\n";
        if (loc.cgiExecutables.empty()) {
            os << indent << "        (none)\n";
        } else {
            // Iterate through map to print CGI executable paths.
            std::map<std::string, std::string>::const_iterator it;
            for (it = loc.cgiExecutables.begin(); it != loc.cgiExecutables.end(); ++it) {
                os << indent << "        Extension: '" << it->first << "', Path: '" << it->second << "'\n";
            }
        }

        os << indent << "    Return: ";
        if (loc.returnCode != 0) {
            os << loc.returnCode;
            if (!loc.returnUrlOrText.empty()) os << " '" << loc.returnUrlOrText << "'";
            os << "\n";
        } else {
            os << "None\n";
        }

        os << indent << "    Error Pages:\n";
        if (loc.errorPages.empty()) {
            os << indent << "        (none)\n";
        } else {
            // Iterate through map to print custom error pages.
            std::map<int, std::string>::const_iterator it;
            for (it = loc.errorPages.begin(); it != loc.errorPages.end(); ++it) {
                os << indent << "        " << it->first << ": '" << it->second << "'\n";
            }
        }

        os << indent << "    Client Max Body Size: " << loc.clientMaxBodySize << " bytes\n";

        // Recursively print nested locations.
        if (!loc.nestedLocations.empty()) {
            os << indent << "    Nested Locations (" << loc.nestedLocations.size() << "):\n";
            for (size_t i = 0; i < loc.nestedLocations.size(); ++i) {
                printLocationConfig(os, loc.nestedLocations[i], indentLevel + 2);
            }
        }
    }

    // Prints a single ServerConfig object recursively to the output stream.
    void printServerConfig(std::ostream& os, const ServerConfig& server, int indentLevel) {
        std::string indent = getIndent(indentLevel);
        os << indent << "Server Block:\n";
        os << indent << "    Listen: " << server.host << ":" << server.port << "\n";
        
        os << indent << "    Server Names: [";
        for (size_t i = 0; i < server.serverNames.size(); ++i) {
            os << "'" << server.serverNames[i] << "'";
            if (i < server.serverNames.size() - 1) os << ", ";
        }
        os << "]\n";

        os << indent << "    Root (Default): '" << server.root << "'\n";
        
        os << indent << "    Index Files (Default): [";
        for (size_t i = 0; i < server.indexFiles.size(); ++i) {
            os << "'" << server.indexFiles[i] << "'";
            if (i < server.indexFiles.size() - 1) os << ", ";
        }
        os << "]\n";

        os << indent << "    Autoindex (Default): " << (server.autoindex ? "on" : "off") << "\n";

        os << indent << "    Error Pages:\n";
        if (server.errorPages.empty()) {
            os << indent << "        (none)\n";
        } else {
            // Iterate through map to print custom error pages.
            std::map<int, std::string>::const_iterator it;
            for (it = server.errorPages.begin(); it != server.errorPages.end(); ++it) {
                os << indent << "        " << it->first << ": '" << it->second << "'\n";
            }
        }

        os << indent << "    Client Max Body Size: " << server.clientMaxBodySize << " bytes\n";
        os << indent << "    Error Log Path: '" << server.errorLogPath << "'\n";
        os << indent << "    Error Log Level: " << logLevelToString(server.errorLogLevel) << "\n";

        // Print locations within this server.
        if (!server.locations.empty()) {
            os << indent << "    Locations (" << server.locations.size() << "):\n";
            for (size_t i = 0; i < server.locations.size(); ++i) {
                printLocationConfig(os, server.locations[i], indentLevel + 1); // Indent one level more for locations
            }
        }
        os << "\n"; // Newline after each server for readability
    }

    // Main function to print the entire loaded server configuration.
    void printConfig(std::ostream& os, const std::vector<ServerConfig>& servers) {
        os << "--- Loaded WebServ Configuration ---\n";
        if (servers.empty()) {
            os << "No server blocks loaded.\n";
            return;
        }
        // Iterate through each server and print its configuration.
        for (size_t i = 0; i < servers.size(); ++i) {
            printServerConfig(os, servers[i], 0); // Start with no indentation for top-level servers
        }
        os << "--- End of Configuration ---\n";
    }

} // namespace ConfigPrinter
