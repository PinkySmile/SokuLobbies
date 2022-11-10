//
// Created by PinkySmile on 08/11/2022.
//

#ifndef SOKULOBBIES_DATA_HPP
#define SOKULOBBIES_DATA_HPP


#include <windows.h>
#include <map>

struct Character {
	std::string firstName;
	std::string fullName;
	std::string codeName;
};

extern wchar_t profilePath[MAX_PATH];
extern wchar_t profileFolderPath[MAX_PATH];
extern char servHost[64];
extern unsigned short servPort;
extern bool hasSoku2;
extern std::map<unsigned int, Character> characters;


#endif //SOKULOBBIES_DATA_HPP
