#ifndef URI_HPP
# define URI_HPP

# include <iostream>

// Represents a Uniform Resource Identifier (URI), breaking it down into path and query components.
class Uri {
    private:
        std::string     _path; // The path component of the URI.
        std::string     _query; // The query component of the URI.

    public:
        // Constructor: Initializes a new Uri object.
        Uri();
		// Destructor: Cleans up Uri resources.
		virtual ~Uri();  
        // Returns the path component of the URI.
        std::string &getPath(void);    
        // Returns the query component of the URI.
        std::string &getQuery(void);    
        // Sets the path component of the URI.
        void    setPath(std::string path); 
        // Sets the query component of the URI.
        void    setQuery(std::string query);
};

#endif