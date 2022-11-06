//
// Created by PinkySmile on 06/11/2022.
//

#ifndef SOKULOBBIES_UTILS_HPP
#define SOKULOBBIES_UTILS_HPP


#include <string>
#include <sstream>

template <typename Iterator>
std::string join(Iterator begin, Iterator end, char separator = '\n')
{
	std::ostringstream o;

	if(begin != end) {
		o << *begin++;
		for(; begin != end; ++begin)
			o << separator << *begin;
	}
	return o.str();
}


#endif //SOKULOBBIES_UTILS_HPP
