//
// Created by PinkySmile on 10/11/2022.
//

#include <fstream>
#include <filesystem>
#include "nlohmann/json.hpp"
#include "data.hpp"
#include "LobbyData.hpp"
#include "dinput.h"

#define ACH_GET_TOTAL_TIME 420
#define ACH_GET_FADE_IN_ANIM 45
#define ACH_GET_FADE_OUT_ANIM 45

std::unique_ptr<LobbyData> lobbyData;

void LobbyData::saveStats()
{
	auto path = std::filesystem::path(profileFolderPath) / "stats.dat";

	_wrename(path.wstring().c_str(), (path.wstring() + L".backup").c_str());

	std::ofstream stream{path, std::fstream::binary};

	if (stream.fail()) {
		MessageBoxA(SokuLib::window, ("Cannot open " + path.string() + ": " + strerror(errno)).c_str(), "Stat saving error", MB_ICONERROR);
		return;
	}

	unsigned magic = this->_getExpectedMagic();

	stream.write((char *)&magic, sizeof(magic));
	this->_saveCharacterStats(stream);
	this->_saveCharacterCardUsage(stream);
	this->_saveMatchupStats(stream);
	if (stream.fail()) {
		MessageBoxA(SokuLib::window, ("Error when saving stats to file: " + std::string(errno ? strerror(errno) : "Unknown error")).c_str(), "Save error", MB_ICONERROR);
		unlink(path.string().c_str());
		_wrename((path.wstring() + L".backup").c_str(), path.wstring().c_str());
	}
}

void LobbyData::_loadStats()
{
	std::ifstream stream{std::filesystem::path(profileFolderPath) / "stats.dat", std::fstream::binary};
	unsigned magic;

	this->loadedCharacterStats.clear();
	if (stream.fail())
		return;
	stream.read((char *)&magic, sizeof(magic));
	if (stream.fail())
		throw std::invalid_argument("Cannot load stats.dat: Unexpected end of file in header");
	this->_loadCharacterStats(stream);
	this->_loadCharacterCardUsage(stream);
	this->_loadMatchupStats(stream);
	if (magic != this->_getExpectedMagic())
		throw std::invalid_argument("Cannot load stats.dat: Invalid magic");
}

void LobbyData::_loadAvatars()
{
	std::filesystem::path folder = profileFolderPath;
	auto path = folder / "assets/avatars/list.json";
	std::ifstream stream{path};
	nlohmann::json j;

	if (stream.fail())
		throw std::runtime_error("Cannot open file " + path.string() + ": " + strerror(errno));
	printf("Loading %s\n", path.string().c_str());
	stream >> j;
	stream.close();
	this->avatars.reserve(j.size());
	for (auto &val : j) {
		this->avatars.emplace_back();

		auto &avatar = this->avatars.back();

		avatar.id = this->avatars.size() - 1;
		avatar.name = val["name"];
		avatar.scale = val["scale"];
		avatar.nbAnimations = val["animations"];
		avatar.animationsStep = val["anim_step"];
		avatar.accessoriesPlacement = val["accessories"];
		avatar.sprite.texture.loadFromFile((std::filesystem::path(profileFolderPath) / val["spritesheet"].get<std::string>()).string().c_str());
		avatar.sprite.rect.width = avatar.sprite.texture.getSize().x / avatar.nbAnimations;
		avatar.sprite.rect.height = avatar.sprite.texture.getSize().y / 2;
		avatar.sprite.setSize({
			static_cast<unsigned int>(avatar.sprite.rect.width * avatar.scale),
			static_cast<unsigned int>(avatar.sprite.rect.height * avatar.scale)
		});
	}
	printf("There are %zu avatars\n", this->avatars.size());
}

void LobbyData::_loadAchievements()
{
	std::filesystem::path folder = profileFolderPath;
	auto path = folder / "assets/achievements.json";
	std::ifstream stream{path};
	nlohmann::json j;
	char *buffer;
	int index = 0;

	if (stream.fail())
		throw std::runtime_error("Cannot open file " + path.string() + ": " + strerror(errno));
	printf("Loading %s\n", path.string().c_str());
	stream >> j;
	stream.close();

	size_t nbBytes = std::ceil(j.size() / 7.f);

	stream.open(folder / "achievements.dat", std::fstream::binary);
	buffer = new char[nbBytes];
	memset(buffer, 0, nbBytes);
	if (stream) {
		stream.read(buffer, nbBytes);
		stream.close();
	} else
		printf("Cannot open file %s: %s\n", (folder / "achievements.dat").string().c_str(), strerror(errno));

	this->achievements.reserve(j.size());
	for (size_t i = 0; i < nbBytes; i++)
		if (buffer[i] & (1 << (i % 8))) {
			this->achievementsLocked = true;
			break;
		}
	for (auto &val : j) {
		this->achievements.emplace_back();

		auto &achievement = this->achievements.back();
		auto arrayIndex = index / 7;
		auto bit = index % 7;
		auto flagBit = arrayIndex % 8;

		bit += bit >= flagBit;
		achievement.shortDescription = val["short_description"];
		achievement.description = val["description"];
		achievement.name = val["name"];
		achievement.requirement = val["requirement"];
		achievement.rewards = val["rewards"].get<std::vector<nlohmann::json>>();
		achievement.awarded = (buffer[arrayIndex] & (1 << bit)) != 0;

		achievement.nameSprite.texture.createFromText(achievement.name.c_str(), this->getFont(14), {400, 20});
		achievement.nameSprite.rect.width = achievement.nameSprite.texture.getSize().x;
		achievement.nameSprite.rect.height = achievement.nameSprite.texture.getSize().y;
		achievement.nameSprite.setSize({
			static_cast<unsigned int>(achievement.nameSprite.rect.width),
			static_cast<unsigned int>(achievement.nameSprite.rect.height)
		});
		achievement.nameSprite.setPosition({0, -20});

		achievement.descSprite.texture.createFromText(achievement.description.c_str(), this->getFont(12), {400, 50});
		achievement.descSprite.rect.width = achievement.descSprite.texture.getSize().x;
		achievement.descSprite.rect.height = achievement.descSprite.texture.getSize().y;
		achievement.descSprite.setSize({
			static_cast<unsigned int>(achievement.descSprite.rect.width),
			static_cast<unsigned int>(achievement.descSprite.rect.height)
		});
		achievement.descSprite.setPosition({0, -20});

		achievement.shortDescSprite.texture.createFromText(achievement.shortDescription.c_str(), this->getFont(12), {400, 50});
		achievement.shortDescSprite.rect.width = achievement.shortDescSprite.texture.getSize().x;
		achievement.shortDescSprite.rect.height = achievement.shortDescSprite.texture.getSize().y;
		achievement.shortDescSprite.setSize({
			static_cast<unsigned int>(achievement.shortDescSprite.rect.width),
			static_cast<unsigned int>(achievement.shortDescSprite.rect.height)
		});
		achievement.shortDescSprite.setPosition({0, -20});
		this->achievementByRequ[achievement.requirement["type"]].push_back(&achievement);
		index++;
	}
	delete[] buffer;
	printf("There are %zu achievements and they are %slocked\n", this->achievements.size(), (this->achievementsLocked ? "" : "un"));
}

void LobbyData::saveAchievements()
{
	std::filesystem::path folder = profileFolderPath;
	size_t nbBytes = std::ceil(this->achievements.size() / 7.f);
	auto buffer = new char[nbBytes];
	auto index = 0;

	memset(buffer, 0, nbBytes);
	if (this->achievementsLocked)
		for (size_t i = 0; i < nbBytes; i++)
			buffer[i] |= 1 << (i % 8);
	for (auto &achievement : this->achievements) {
		auto arrayIndex = index / 7;
		auto bit = index % 7;
		auto flagBit = arrayIndex % 8;

		bit += bit >= flagBit;
		if (achievement.awarded)
			buffer[arrayIndex] |= 1 << bit;
		index++;
	}

	std::ofstream stream{folder / "achievements.dat", std::fstream::binary};

	if (stream) {
		stream.write(buffer, nbBytes);
		stream.close();
	}
	delete[] buffer;
}

void LobbyData::_loadBackgrounds()
{
	std::filesystem::path folder = profileFolderPath;
	auto path = folder / "assets/backgrounds/list.json";
	std::ifstream stream{path};
	nlohmann::json j;

	if (stream.fail())
		throw std::runtime_error("Cannot open file " + path.string() + ": " + strerror(errno));
	printf("Loading %s\n", path.string().c_str());
	stream >> j;
	stream.close();
	this->backgrounds.reserve(j.size());
	for (auto &val : j) {
		this->backgrounds.emplace_back();

		auto &bg = this->backgrounds.back();

		bg.id = this->backgrounds.size() - 1;
		bg.groundPos = val["ground"];
		bg.parallaxFactor = val["parallax_factor"];
		bg.platformInterval = val["platform_interval"];
		bg.platformWidth = val["platform_width"];
		bg.platformCount = val["platform_count"];
		bg.fg.texture.loadFromFile((folder / val["fg"].get<std::string>()).string().c_str());
		bg.fg.setSize(bg.fg.texture.getSize());
		bg.fg.rect.width = bg.fg.getSize().x;
		bg.fg.rect.height = bg.fg.getSize().y;
		bg.bg.texture.loadFromFile((folder / val["bg"].get<std::string>()).string().c_str());
		bg.bg.setSize(bg.bg.texture.getSize());
		bg.bg.rect.width = bg.bg.getSize().x;
		bg.bg.rect.height = bg.bg.getSize().y;
	}
	printf("There are %zu backgrounds\n", this->avatars.size());
}

void LobbyData::_loadEmotes()
{
	std::filesystem::path folder = profileFolderPath;
	auto path = folder / "assets/emotes/list.json";
	std::ifstream stream{path};
	nlohmann::json j;

	if (stream.fail())
		throw std::runtime_error("Cannot open file " + path.string() + ": " + strerror(errno));
	printf("Loading %s\n", path.string().c_str());
	stream >> j;
	stream.close();
	this->emotes.reserve(j.size());
	for (auto &val : j) {
		this->emotes.emplace_back();

		auto &emote = this->emotes.back();

		emote.id = this->emotes.size() - 1;
		emote.filepath = val["path"];
		emote.alias = val["alias"].get<std::vector<std::string>>();
		emote.sprite.texture.loadFromFile((folder / emote.filepath).string().c_str());
		emote.sprite.setSize({EMOTE_SIZE, EMOTE_SIZE});
		emote.sprite.rect.width = emote.sprite.texture.getSize().x;
		emote.sprite.rect.height = emote.sprite.texture.getSize().y;
		for (auto &alias : emote.alias) {
			auto it = this->emotesByName.find(alias);

			if (it != this->emotesByName.end())
				throw std::runtime_error("Duplicate alias " + alias);
			this->emotesByName[alias] = &emote;
		}
	}
	printf("There are %zu emotes (%zu different alias)\n", this->emotes.size(), this->emotesByName.size());
}

static void extractArcadeAnimation(LobbyData::ArcadeAnimation &animation, const nlohmann::json &j)
{
	animation.file = j["file"];
	animation.size.x = j["sizeX"];
	animation.size.y = j["sizeY"];
	animation.frameRate = j["rate"];
	animation.frameCount = j["frames"];
	animation.loop = j.contains("loop") && j["loop"];
	animation.sprite.texture.loadFromFile((std::filesystem::path(profileFolderPath) / animation.file).string().c_str());
	animation.sprite.setSize(animation.size);
	animation.sprite.rect.width = animation.size.x;
	animation.sprite.rect.height = animation.size.y;
	animation.tilePerLine = animation.sprite.texture.getSize().x / animation.size.x;
}

void LobbyData::_loadArcades()
{
	std::filesystem::path folder = profileFolderPath;
	auto path = folder / "assets/arcades/list.json";
	std::ifstream stream{path};
	nlohmann::json j;

	if (stream.fail())
		throw std::runtime_error("Cannot open file " + path.string());
	printf("Loading %s\n", path.string().c_str());
	stream >> j;
	stream.close();
	extractArcadeAnimation(this->arcades.intro, j["intro"]);
	extractArcadeAnimation(this->arcades.select, j["select"]);
	this->arcades.game.reserve(j["games"].size());
	for (auto &val : j["games"]) {
		this->arcades.game.emplace_back();
		extractArcadeAnimation(this->arcades.game.back(), val);
	}
	this->arcades.skins.reserve(j["arcades"].size());
	for (auto &val : j["arcades"]) {
		this->arcades.skins.emplace_back();

		auto &skin = this->arcades.skins.back();

		skin.file = val["file"];
		skin.animationOffsets.x = val["offsetX"];
		skin.animationOffsets.y = val["offsetY"];
		skin.frameRate = val["rate"];
		skin.frameCount = val["frames"];
		skin.sprite.texture.loadFromFile((std::filesystem::path(profileFolderPath) / skin.file).string().c_str());
		skin.sprite.setSize(skin.sprite.texture.getSize());
		skin.sprite.rect.width = skin.sprite.getSize().x;
		skin.sprite.rect.height = skin.sprite.getSize().y;
	}
}

LobbyData::LobbyData()
{
	this->_loadStats();
	this->_loadAvatars();
	this->_loadBackgrounds();
	this->_loadEmotes();
	this->_loadArcades();
	this->_loadAchievements();

	this->achHolder.holder.texture.loadFromFile((std::filesystem::path(profileFolderPath) / "assets/ACHIEVEMENT_BAR.png").string().c_str());
	this->achHolder.holder.setSize(this->achHolder.holder.texture.getSize());
	this->achHolder.holder.rect.width = this->achHolder.holder.getSize().x;
	this->achHolder.holder.rect.height = this->achHolder.holder.getSize().y;

	this->achHolder.behindGear.texture.loadFromGame("data/menu/gear/4L-mid_S.png");
	this->achHolder.behindGear.setSize(this->achHolder.behindGear.texture.getSize());
	this->achHolder.behindGear.rect.width = this->achHolder.behindGear.getSize().x;
	this->achHolder.behindGear.rect.height = this->achHolder.behindGear.getSize().y;
}

LobbyData::~LobbyData()
{
	this->saveStats();
	this->saveAchievements();
	for (auto &font : this->_fonts)
		font.second.destruct();
}

bool LobbyData::isLocked(const LobbyData::Emote &emote)
{
	return false;
}

bool LobbyData::isLocked(const LobbyData::Avatar &avatar)
{
	return false;
}

bool LobbyData::isLocked(const LobbyData::Background &background)
{
	return false;
}

unsigned LobbyData::_getExpectedMagic()
{
	unsigned magic = CHR_STATS_MAGIC;

	for (auto &entry : this->loadedCharacterStats) {
		magic -= entry.first;
		magic -= entry.second.losses;
		magic -= entry.second.wins;
		magic -= entry.second.againstWins;
		magic -= entry.second.againstLosses;
	}
	magic -= this->loadedCharacterStats.size();

	for (auto &entry : this->loadedCharacterCardUsage) {
		magic -= entry.first;
		magic -= entry.second.totalCards;
		for (auto &card : entry.second.cards) {
			magic -= card.burnt;
			magic -= card.inDeck;
			magic -= card.used;
		}
	}
	magic -= this->loadedCharacterCardUsage.size();

	for (auto &entry : this->loadedMatchupStats) {
		magic -= entry.first.first;
		magic -= entry.first.second;
		magic -= entry.second.losses;
		magic -= entry.second.wins;
	}
	magic -= this->loadedMatchupStats.size();
	return magic;
}

void LobbyData::_loadCharacterStats(std::istream &stream)
{
	unsigned len;

	stream.read((char *)&len, sizeof(len));
	while (len--) {
		CharacterStatEntry entry;
		unsigned char id;

		stream.read((char *)&id, sizeof(id));
		stream.read((char *)&entry.losses, sizeof(entry.losses));
		stream.read((char *)&entry.wins, sizeof(entry.wins));
		stream.read((char *)&entry.againstWins, sizeof(entry.againstWins));
		stream.read((char *)&entry.againstLosses, sizeof(entry.againstLosses));
		if (stream.fail())
			throw std::invalid_argument("Cannot load stats.dat: Reading ChrStats Unexpected end of file");
		if (this->loadedCharacterStats.find(id) != this->loadedCharacterStats.end())
			throw std::invalid_argument("Cannot load stats.dat: Reading ChrStats Duplicated entry " + std::to_string(id));
		this->loadedCharacterStats[id] = entry;
	}
}

void LobbyData::_loadCharacterCardUsage(std::istream &stream)
{
	unsigned len;

	stream.read((char *)&len, sizeof(len));
	while (len--) {
		CardChrStatEntry entry;
		unsigned char id;

		stream.read((char *)&id, sizeof(id));
		stream.read((char *)&entry.totalCards, sizeof(entry.totalCards));
		for (auto &card : entry.cards) {
			stream.read((char *)&card.inDeck, sizeof(card.inDeck));
			stream.read((char *)&card.used, sizeof(card.used));
			stream.read((char *)&card.burnt, sizeof(card.burnt));
		}
		if (stream.fail())
			throw std::invalid_argument("Cannot load stats.dat: Reading CardChrStats Unexpected end of file");
		if (this->loadedCharacterCardUsage.find(id) != this->loadedCharacterCardUsage.end())
			throw std::invalid_argument("Cannot load stats.dat: Reading CardChrStats Duplicated entry " + std::to_string(id));
		this->loadedCharacterCardUsage[id] = entry;
	}
}

void LobbyData::_loadMatchupStats(std::istream &stream)
{
	unsigned len;

	stream.read((char *)&len, sizeof(len));
	while (len--) {
		MatchupStatEntry entry;
		std::pair<unsigned char, unsigned char> id;

		stream.read((char *)&id.first, sizeof(id.first));
		stream.read((char *)&id.second, sizeof(id.second));
		stream.read((char *)&entry.losses, sizeof(entry.losses));
		stream.read((char *)&entry.wins, sizeof(entry.wins));
		if (stream.fail())
			throw std::invalid_argument("Cannot load stats.dat: Reading MUStats Unexpected end of file");
		if (this->loadedMatchupStats.find(id) != this->loadedMatchupStats.end())
			throw std::invalid_argument("Cannot load stats.dat: Reading MUStats Duplicated entry " + std::to_string(id.first) + "," + std::to_string(id.second));
		this->loadedMatchupStats[id] = entry;
	}
}

void LobbyData::_saveCharacterStats(std::ostream &stream)
{
	unsigned size = this->loadedCharacterStats.size();

	stream.write((char *)&size, sizeof(size));
	for (auto &entry : this->loadedCharacterStats) {
		stream.write((char *)&entry.first, sizeof(entry.first));
		stream.write((char *)&entry.second.losses, sizeof(entry.second.losses));
		stream.write((char *)&entry.second.wins, sizeof(entry.second.wins));
		stream.write((char *)&entry.second.againstWins, sizeof(entry.second.againstWins));
		stream.write((char *)&entry.second.againstLosses, sizeof(entry.second.againstLosses));
	}
}

void LobbyData::_saveCharacterCardUsage(std::ostream &stream)
{
	unsigned size = this->loadedCharacterCardUsage.size();

	stream.write((char *)&size, sizeof(size));
	for (auto &entry : this->loadedCharacterCardUsage) {
		stream.write((char *)&entry.first, sizeof(entry.first));
		stream.write((char *)&entry.second.totalCards, sizeof(entry.second.totalCards));
		for (auto &card : entry.second.cards) {
			stream.write((char *)&card.inDeck, sizeof(card.inDeck));
			stream.write((char *)&card.used, sizeof(card.used));
			stream.write((char *)&card.burnt, sizeof(card.burnt));
		}
	}
}

void LobbyData::_saveMatchupStats(std::ostream &stream)
{
	unsigned size = this->loadedMatchupStats.size();

	stream.write((char *)&size, sizeof(size));
	for (auto &entry : this->loadedMatchupStats) {
		stream.write((char *)&entry.first.first, sizeof(entry.first.first));
		stream.write((char *)&entry.first.second, sizeof(entry.first.second));
		stream.write((char *)&entry.second.losses, sizeof(entry.second.losses));
		stream.write((char *)&entry.second.wins, sizeof(entry.second.wins));
	}
}

void LobbyData::_loadFont(SokuLib::SWRFont &font, unsigned int size)
{
	SokuLib::FontDescription desc;
	bool hasEnglishPatch = (*(int *)0x411c64 == 1);

	desc.r1 = 255;
	desc.r2 = 255;
	desc.g1 = 255;
	desc.g2 = 255;
	desc.b1 = 255;
	desc.b2 = 255;
	desc.height = size + hasEnglishPatch * 2;
	desc.weight = FW_NORMAL;
	desc.italic = 0;
	desc.shadow = 1;
	desc.bufferSize = 1000000;
	desc.charSpaceX = 0;
	desc.charSpaceY = hasEnglishPatch * -2;
	desc.offsetX = 0;
	desc.offsetY = 0;
	desc.useOffset = 0;
	strcpy(desc.faceName, "MonoSpatialModSWR");
	desc.weight = FW_REGULAR;

	printf("Loading font of size %i (actual size %li)\n", size, desc.height);
	font.create();
	font.setIndirect(desc);
}

SokuLib::SWRFont &LobbyData::getFont(unsigned int size)
{
	auto it = this->_fonts.find(size);

	if (it != this->_fonts.end())
		return it->second;

	auto &font = this->_fonts[size];

	this->_loadFont(font, size);
	return font;
}

void LobbyData::update()
{
	if (this->achievementAwardQueue.empty())
		return;
	if (this->_achTimer == 0)
		SokuLib::playSEWaveBuffer(33);
	this->_achTimer++;
	if (this->_achTimer < ACH_GET_FADE_IN_ANIM)
		this->achHolder.behindGear.setPosition({
			320 - this->achHolder.holder.rect.width / 2 - this->achHolder.behindGear.rect.width / 2,
			480 - static_cast<int>(this->achHolder.behindGear.rect.height * this->_achTimer / ACH_GET_FADE_IN_ANIM) + (this->achHolder.behindGear.rect.height - this->achHolder.holder.rect.height) / 2
		});
	else if (this->_achTimer > ACH_GET_TOTAL_TIME - ACH_GET_FADE_OUT_ANIM)
		this->achHolder.behindGear.setPosition({
			320 - this->achHolder.holder.rect.width / 2 - this->achHolder.behindGear.rect.width / 2,
			480 - static_cast<int>(this->achHolder.behindGear.rect.height * (ACH_GET_TOTAL_TIME - this->_achTimer) / ACH_GET_FADE_OUT_ANIM) + (this->achHolder.behindGear.rect.height - this->achHolder.holder.rect.height) / 2
		});
	else
		this->achHolder.behindGear.setPosition({
			320 - this->achHolder.holder.rect.width / 2 - this->achHolder.behindGear.rect.width / 2,
			480 - this->achHolder.behindGear.rect.height + (this->achHolder.behindGear.rect.height - this->achHolder.holder.rect.height) / 2
		});

	auto &achievement = this->achievementAwardQueue.front();
	auto reward = achievement->rewards[0];
	auto type = reward["type"];

	if (achievement->rewards.empty() || achievement->rewards[0]["type"] == "title") {
		achievement->nameSprite.setPosition(this->achHolder.holder.getPosition() + SokuLib::Vector2i{5, 5});
		this->achHolder.holder.setPosition(this->achHolder.behindGear.getPosition() + SokuLib::Vector2i{
			this->achHolder.behindGear.rect.width / 2,
			(this->achHolder.behindGear.rect.height - this->achHolder.holder.rect.height) / 2
		});
	} else {
		this->achHolder.holder.setPosition(this->achHolder.behindGear.getPosition() + SokuLib::Vector2i{
			this->achHolder.behindGear.rect.width * 3 / 4,
			(this->achHolder.behindGear.rect.height - this->achHolder.holder.rect.height) / 2
		});
		achievement->nameSprite.setPosition({
			static_cast<int>(this->achHolder.behindGear.getPosition().x + this->achHolder.behindGear.getSize().x + 4),
			this->achHolder.holder.getPosition().y + 5
		});
		if (type == "avatar") {
			auto &avatar = lobbyData->avatars[reward["id"]];

			this->_animCtr++;
			if (this->_animCtr >= avatar.animationsStep) {
				this->_animCtr = 0;
				this->_anim++;
				if (this->_anim >= avatar.nbAnimations)
					this->_anim = 0;
			}
		} else if (type == "emote") {
			auto emote = this->emotesByName[reward["name"]];
			unsigned offset = 20;

			emote->sprite.setPosition(this->achHolder.behindGear.getPosition() + SokuLib::Vector2u{offset, offset - 2});
			emote->sprite.setSize(this->achHolder.behindGear.getSize() - SokuLib::Vector2u{offset * 2, offset * 2});
			emote->sprite.draw();
		} else if (type == "prop") {

		} else if (type == "accessory") {

		}
	}

	achievement->shortDescSprite.setPosition(achievement->nameSprite.getPosition() + SokuLib::Vector2i{0, achievement->nameSprite.rect.height - 2});
	this->achHolder.behindGear.setRotation(this->achHolder.behindGear.getRotation() + 0.05);
	if (this->_achTimer >= ACH_GET_TOTAL_TIME) {
		this->_achTimer = 0;
		this->achievementAwardQueue.pop_front();
	}
}

void LobbyData::render()
{
	if (this->achievementAwardQueue.empty())
		return;

	SokuLib::SpriteEx sprite;
	auto &achievement = this->achievementAwardQueue.front();
	auto reward = achievement->rewards.empty() ? nlohmann::json{} : achievement->rewards[0];
	auto type = achievement->rewards.empty() ? "title" : reward["type"];
	unsigned offset = 20;

	sprite.render();
	this->achHolder.holder.draw();
	if (type != "title") {
		this->achHolder.behindGear.setPosition(this->achHolder.behindGear.getPosition() + SokuLib::Vector2i{2, 2});
		this->achHolder.behindGear.tint = SokuLib::Color::Black;
		this->achHolder.behindGear.draw();
		this->achHolder.behindGear.setPosition(this->achHolder.behindGear.getPosition() - SokuLib::Vector2i{2, 2});
		this->achHolder.behindGear.tint = SokuLib::Color::White;
		this->achHolder.behindGear.draw();
	}

	achievement->nameSprite.draw();
	achievement->shortDescSprite.draw();
	if (type == "avatar") {
		auto &avatar = lobbyData->avatars[reward["id"]];
		auto pos = this->achHolder.behindGear.getPosition();

		pos.x += this->achHolder.behindGear.getSize().x / 2;
		pos.x -= avatar.sprite.rect.width / 2;
		if (this->_achTimer < ACH_GET_FADE_IN_ANIM)
			pos.y = 480 - avatar.sprite.rect.height + static_cast<int>(this->achHolder.behindGear.rect.height * (ACH_GET_FADE_IN_ANIM - this->_achTimer) / ACH_GET_FADE_IN_ANIM);
		else if (this->_achTimer > ACH_GET_TOTAL_TIME - ACH_GET_FADE_OUT_ANIM)
			pos.y = 480 - avatar.sprite.rect.height + static_cast<int>(this->achHolder.behindGear.rect.height * (ACH_GET_FADE_OUT_ANIM - (ACH_GET_TOTAL_TIME - this->_achTimer)) / ACH_GET_FADE_OUT_ANIM);
		else
			pos.y = 480 - avatar.sprite.rect.height;
		avatar.sprite.setPosition(pos);
		avatar.sprite.setSize({
			static_cast<unsigned int>(avatar.sprite.rect.width),
			static_cast<unsigned int>(avatar.sprite.rect.height)
		});
		avatar.sprite.setMirroring(false, false);
		avatar.sprite.rect.left = avatar.sprite.rect.width * this->_anim;
		avatar.sprite.rect.top = 0;
		avatar.sprite.draw();
		avatar.sprite.setSize({
			static_cast<unsigned int>(avatar.sprite.rect.width * avatar.scale),
			static_cast<unsigned int>(avatar.sprite.rect.height * avatar.scale)
		});
	} else if (type == "emote") {
		auto emote = this->emotesByName[reward["name"]];

		emote->sprite.setPosition(this->achHolder.behindGear.getPosition() + SokuLib::Vector2u{offset, offset - 2});
		emote->sprite.setSize(this->achHolder.behindGear.getSize() - SokuLib::Vector2u{offset * 2, offset * 2});
		emote->sprite.draw();
	} else if (type == "prop") {

	} else if (type == "accessory") {

	}
}