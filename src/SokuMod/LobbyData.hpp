//
// Created by PinkySmile on 10/11/2022.
//

#ifndef SOKULOBBIES_LOBBYDATA_HPP
#define SOKULOBBIES_LOBBYDATA_HPP


#include <map>
#include <mutex>
#include <vector>
#include <string>
#include <SokuLib.hpp>
#include <curl/curl.h>
#include "nlohmann/json.hpp"

#define EMOTE_SIZE 32
#define CHR_STATS_MAGIC 0xF2A6E790

class LobbyData {
private:
	struct MemoryStruct {
		char *memory;
		size_t size;
	};

	void _loadStats();
	void _loadAvatars();
	void _loadBackgrounds();
	void _loadEmotes();
	void _loadArcades();
	void _loadAchievements();
	void _loadFlags();

	unsigned _getExpectedMagic();
	void _loadCharacterStats(std::istream &stream);
	void _loadCharacterCardUsage(std::istream &stream);
	void _loadMatchupStats(std::istream &stream);
	void _saveCharacterStats(std::ostream &stream);
	void _saveCharacterCardUsage(std::ostream &stream);
	void _saveMatchupStats(std::ostream &stream);

	void _loadFont(SokuLib::SWRFont &font, unsigned size);

	void _grantStatsAchievements();
	void _grantCrashAchievements();

	static size_t LobbyData::writeMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);

	std::map<unsigned, SokuLib::SWRFont> _fonts;
	unsigned _achTimer = 0;
	unsigned _animCtr = 0;
	unsigned _anim = 0;

public:
	//Stats
	struct CharacterStatEntry {
		unsigned wins;
		unsigned losses;
		unsigned againstWins;
		unsigned againstLosses;
	};
	struct MatchupStatEntry {
		unsigned wins;
		unsigned losses;
	};
	struct CardStatEntry {
		unsigned inDeck;
		unsigned used;
		unsigned burnt;
	};
	struct CardChrStatEntry {
		unsigned totalCards;
		CardStatEntry cards[35];
	};

	struct Achievement {
		std::string description;
		std::string name;
		nlohmann::json requirement;
		std::vector<nlohmann::json> rewards;
		bool awarded;
		bool hidden;
		SokuLib::DrawUtils::Sprite nameSprite;
		SokuLib::DrawUtils::Sprite nameSpriteFull;
		SokuLib::DrawUtils::Sprite nameSpriteTitle;
		SokuLib::DrawUtils::Sprite descSprite;

		Achievement() = default;
		Achievement(const Achievement &) { assert(false); }
	};
	struct Avatar {
		unsigned short id = 0;
		std::string name;
		float scale = 0;
		SokuLib::DrawUtils::Sprite sprite;
		unsigned accessoriesPlacement = 0;
		unsigned animationsStep = 0;
		unsigned nbAnimations = 0;
		unsigned animationStyle = 0;
		bool hidden = false;
		Achievement *requirement = nullptr;

		Avatar() = default;
		Avatar(const Avatar &) { assert(false); }
	};
	struct Background {
		unsigned short id = 0;
		SokuLib::DrawUtils::Sprite bg;
		SokuLib::DrawUtils::Sprite fg;
		unsigned groundPos = 0;
		float parallaxFactor = 0;
		unsigned platformInterval = 0;
		unsigned platformWidth = 0;
		unsigned platformCount = 0;
		Achievement *requirement = nullptr;

		Background() = default;
		Background(const Background &) { assert(false); }
	};
	struct Emote {
		unsigned short id = 0;
		std::string filepath;
		std::vector<std::string> alias;
		SokuLib::DrawUtils::Sprite sprite;
		Achievement *requirement = nullptr;

		Emote() = default;
		Emote(const Emote &) { assert(false); }
	};
	struct ArcadeAnimation {
		std::string file;
		SokuLib::DrawUtils::Sprite sprite;
		unsigned tilePerLine;
		SokuLib::Vector2u size;
		unsigned frameRate;
		unsigned frameCount;
		bool loop;

		ArcadeAnimation() = default;
		ArcadeAnimation(const ArcadeAnimation &) { assert(false); }
	};
	struct ArcadeSkin {
		std::string file;
		SokuLib::DrawUtils::Sprite sprite;
		SokuLib::DrawUtils::Sprite overlay;
		SokuLib::Vector2i animationOffsets;
		unsigned frameRate;
		unsigned frameCount;
		Achievement *requirement = nullptr;

		ArcadeSkin() = default;
		ArcadeSkin(const ArcadeSkin &) { assert(false); }
	};
	struct ArcadeData {
		ArcadeAnimation intro;
		ArcadeAnimation select;
		ArcadeAnimation hostlist;
		std::vector<ArcadeAnimation> game;
		std::vector<ArcadeSkin> skins;
	};
	struct AchievementHolder {
		SokuLib::DrawUtils::Sprite getText;
		SokuLib::DrawUtils::Sprite holder;
		SokuLib::DrawUtils::Sprite behindGear;
	};

	bool achievementsLocked = false;
	ArcadeData arcades;
	AchievementHolder achHolder;
	std::vector<Emote> emotes;
	std::vector<Avatar> avatars;
	std::vector<Background> backgrounds;
	std::vector<Achievement> achievements;
	std::map<std::string, Emote *> emotesByName;
	std::map<std::string, std::unique_ptr<SokuLib::DrawUtils::Sprite>> flags;
	std::map<unsigned char, CharacterStatEntry> loadedCharacterStats;
	std::map<unsigned char, CardChrStatEntry> loadedCharacterCardUsage;
	std::map<std::string, std::vector<Achievement *>> achievementByRequ;
	std::map<std::pair<unsigned char, unsigned char>, MatchupStatEntry> loadedMatchupStats;
	std::list<Achievement *> achievementAwardQueue;

	LobbyData();
	~LobbyData();
	std::string httpRequest(const std::string &url, const std::string &method = "GET", const std::string &data = "");
	bool isLocked(const Emote &emote);
	bool isLocked(const Avatar &avatar);
	bool isLocked(const Background &background);
	SokuLib::SWRFont &getFont(unsigned size);
	void saveAchievements();
	void saveStats();
	void update();
	void render();
};

extern std::unique_ptr<LobbyData> lobbyData;

#endif //SOKULOBBIES_LOBBYDATA_HPP
