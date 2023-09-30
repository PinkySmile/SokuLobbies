//
// Created by PinkySmile on 10/11/2022.
//

#ifndef SOKULOBBIES_LOBBYDATA_HPP
#define SOKULOBBIES_LOBBYDATA_HPP


#include <map>
#include <mutex>
#include <vector>
#include <string>
#include <optional>
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
	void _loadElevators();
	void _loadAchievements();
	void _loadFlags();
	void _loadGameCards();

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
	void _grantDebugAchievements();

	static size_t LobbyData::writeMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);

	std::map<unsigned, SokuLib::SWRFont> _fonts;
	unsigned _achTimer = 0;
	unsigned _animCtr = 0;
	unsigned _anim = 0;
	unsigned _reward = 0;

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
		unsigned nbGames;
		std::map<unsigned, CardStatEntry> cards;
	};

	struct Achievement {
		std::string code;
		std::string description;
		std::string name;
		std::string category;
		nlohmann::json requirement;
		std::vector<nlohmann::json> rewards;
		bool awarded;
		bool hidden;
		SokuLib::DrawUtils::Sprite nameSprite;
		SokuLib::DrawUtils::Sprite nameSpriteFull;
		SokuLib::DrawUtils::Sprite nameSpriteTitle;
		SokuLib::DrawUtils::Sprite descSprite;

		Achievement() = default;
	};
	struct Avatar {
		std::string code;
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
	};
	struct Platform {
		SokuLib::Vector2i pos;
		unsigned width;
	};
	struct ArcadePlacement {
		SokuLib::Vector2i pos;
		bool old;
		bool special;
	};
	struct ElevatorLink {
		unsigned elevator;
		unsigned platform;
	};
	struct ElevatorPlacement {
		SokuLib::Vector2i pos;
		std::optional<ElevatorLink> leftLink;
		std::optional<ElevatorLink> rightLink;
		std::optional<ElevatorLink> upLink;
		std::optional<ElevatorLink> downLink;
		bool noIndicator;
		bool hidden;
	};
	enum LayerType {
		LAYERTYPE_IMAGE,
		LAYERTYPE_SYSTEM,
		LAYERTYPE_CLOCK,
	};
	struct Layer {
		LayerType type;
		SokuLib::DrawUtils::Sprite *image;
	};
	struct ClockLayer {
		SokuLib::DrawUtils::Sprite *hour;
		SokuLib::DrawUtils::Sprite *minute;
		SokuLib::DrawUtils::Sprite *second;
		SokuLib::Vector2i center;
	};
	struct Background {
		unsigned short id = 0;
		SokuLib::Vector2u size;
		std::list<SokuLib::DrawUtils::Sprite> images;
		std::vector<Layer> layers;
		std::vector<Platform> platforms;
		std::vector<ArcadePlacement> arcades;
		std::vector<ElevatorPlacement> elevators;
		std::optional<ClockLayer> clock;
		int startX;
		int startPlatform;
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
	struct ElevatorSkin {
		std::string file;
		SokuLib::Vector2i doorOffset;
		SokuLib::DrawUtils::Sprite sprite;
		SokuLib::DrawUtils::TextureRect cage;
		SokuLib::DrawUtils::TextureRect indicator;
		SokuLib::DrawUtils::TextureRect arrow;
		SokuLib::DrawUtils::TextureRect doorLeft;
		SokuLib::DrawUtils::TextureRect doorRight;
		unsigned frameRate;
		unsigned frameCount;
		Achievement *requirement = nullptr;

		ElevatorSkin() = default;
		ElevatorSkin(const ElevatorSkin &) { assert(false); }
	};
	struct AchievementHolder {
		SokuLib::DrawUtils::Sprite getText;
		SokuLib::DrawUtils::Sprite holder;
		SokuLib::DrawUtils::Sprite behindGear;
	};
	struct CardInfos : public SokuLib::DrawUtils::Sprite {
		std::string cardName;
	};

	bool achievementsLocked = false;
	ArcadeData arcades;
	AchievementHolder achHolder;
	std::vector<Emote> emotes;
	std::vector<Avatar> avatars;
	std::vector<ElevatorSkin> elevators;
	std::vector<Background> backgrounds;
	std::vector<Achievement> achievements;
	std::map<std::string, Emote *> emotesByName;
	std::map<std::string, Avatar *> avatarsByCode;
	std::map<std::string, std::unique_ptr<SokuLib::DrawUtils::Sprite>> flags;
	std::map<unsigned char, CharacterStatEntry> loadedCharacterStats;
	std::map<unsigned char, CardChrStatEntry> loadedCharacterCardUsage;
	std::map<std::string, std::vector<Achievement *>> achievementByRequ;
	std::map<std::pair<unsigned char, unsigned char>, MatchupStatEntry> loadedMatchupStats;
	std::map<unsigned char, std::map<unsigned short, CardInfos>> cardsTextures;
	std::list<Achievement *> achievementAwardQueue;

	LobbyData();
	~LobbyData();
	std::string httpRequest(const std::string &url, const std::string &method = "GET", const std::string &data = "", long timeoutMs = 20000L);
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
