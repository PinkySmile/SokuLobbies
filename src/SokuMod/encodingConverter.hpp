//
// Created by PinkySmile on 29/01/2023.
//

#ifndef SOKULOBBIES_ENCODINGCONVERTER_HPP
#define SOKULOBBIES_ENCODINGCONVERTER_HPP


#include <string>

template<typename T1, typename T2, std::basic_string<unsigned> decoder(const std::basic_string<T1> &), std::basic_string<T2> encoder(const std::basic_string<unsigned> &)>
std::basic_string<T2> convertEncoding(const std::basic_string<T1> &str)
{
	return encoder(decoder(str));
}

std::basic_string<unsigned> shiftJISDecode(const std::string &str);
std::basic_string<unsigned> UTF8Decode(const std::string &str);
std::basic_string<unsigned> UTF16Decode(const std::wstring &str);

std::string shiftJISEncode(const std::basic_string<unsigned> &str);
std::string UTF8Encode(const std::basic_string<unsigned> &str);
std::wstring UTF16Encode(const std::basic_string<unsigned> &str);


#endif //SOKULOBBIES_ENCODINGCONVERTER_HPP
