#include "../../includes/server/Uri.hpp"

Uri::Uri() {
}

Uri::~Uri() {
}

std::string &Uri::getPath(void) {
    return (_path);
}   

std::string &Uri::getQuery(void) {
    return (_query);
}  

void    Uri::setPath(std::string path) {
    this->_path = path; 
} 
void    Uri::setQuery(std::string query) {
    this->_query = query;
}  