#ifndef URI_HPP
# define URI_HPP

# include <iostream>

class Uri {
	private:
		std::string     _path;
		std::string     _query;

	public:
		Uri();
		virtual	~Uri();  
		std::string	&getPath(void);    
		std::string	&getQuery(void);    
		void		setPath(std::string path); 
		void		setQuery(std::string query);
};

#endif