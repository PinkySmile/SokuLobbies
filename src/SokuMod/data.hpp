//
// Created by PinkySmile on 08/11/2022.
//

#ifndef SOKULOBBIES_DATA_HPP
#define SOKULOBBIES_DATA_HPP


#include <windows.h>
#include <map>
#include <string>
#include <functional>

struct Character {
	std::string firstName;
	std::string fullName;
	std::string codeName;
	unsigned nbSkills;
};

enum ChatPopupMode {
	CHAT_POPUP_ALL,
	CHAT_POPUP_OPPONENTS,
	CHAT_POPUP_NEVER
};

extern wchar_t profilePath[MAX_PATH];
extern wchar_t profileFolderPath[MAX_PATH];
extern char servHost[64];
extern char redirectIp[64];
extern char modVersion[16];
extern char *wineVersion;
extern unsigned lobbyJoinTries;
extern unsigned lobbyJoinInterval;
extern unsigned maxChatMessages;
extern unsigned opponentChatColor;
extern bool showTextBubbles;
extern ChatPopupMode chatPopupMode;
extern unsigned hostPref;
extern unsigned chatKey;
extern bool chineseLanguage;
extern std::wstring quickMessages[9];
extern unsigned short servPort;
extern unsigned short hostPort;
extern bool hasSoku2;
extern std::map<unsigned int, Character> characters;
extern std::function<int ()> onGameEnd;

void playSound(int se);


#endif //SOKULOBBIES_DATA_HPP
