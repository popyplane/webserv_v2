/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   StringUtils.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: baptistevieilhescaze <baptistevieilhesc    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/21 15:27:27 by baptistevie       #+#    #+#             */
/*   Updated: 2025/06/25 18:20:55 by baptistevie      ###   ########.fr       */
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

    // Trims leading and trailing whitespace from a string.
    void trim(std::string& s);

    // Converts a string to lowercase.
    void toLower(std::string& str);

    // Performs a case-insensitive string comparison.
    bool ciCompare(const std::string& s1, const std::string& s2);

    // Splits a string into a vector of substrings based on a single character delimiter.
    std::vector<std::string> split(const std::string& s, char delimiter);

    // Checks if an entire string consists only of digit characters.
    bool isDigits(const std::string& str);

    // Converts a string to a long integer.
    long stringToLong(const std::string& str);

    // Converts a long integer to a string.
    std::string longToString(long val);

    // Checks if a string starts with a given prefix.
    bool startsWith(const std::string& str, const std::string& prefix);

    // Checks if a string ends with a given suffix.
    bool endsWith(const std::string& str, const std::string& suffix);


} // namespace StringUtils

#endif
