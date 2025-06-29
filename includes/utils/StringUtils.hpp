/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   StringUtils.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: baptistevieilhescaze <baptistevieilhesc    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/21 15:27:27 by baptistevie       #+#    #+#             */
/*   Updated: 2025/06/29 02:08:32 by baptistevie      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef STRING_UTILS_HPP
# define STRING_UTILS_HPP

#include <string>
#include <vector>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <algorithm>

// Provides a collection of utility functions for string manipulation.
namespace StringUtils {

    void trim(std::string& s);
    void toLower(std::string& str);
    bool ciCompare(const std::string& s1, const std::string& s2);
    std::vector<std::string> split(const std::string& s, char delimiter);
    bool isDigits(const std::string& str);
    long stringToLong(const std::string& str);
    std::string longToString(long val);
    bool startsWith(const std::string& str, const std::string& prefix);
    bool endsWith(const std::string& str, const std::string& suffix);
}

#endif
