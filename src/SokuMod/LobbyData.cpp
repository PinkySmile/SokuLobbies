//
// Created by PinkySmile on 10/11/2022.
//

#include <fstream>
#include <filesystem>
#include "nlohmann/json.hpp"
#include "data.hpp"
#include "LobbyData.hpp"

std::unique_ptr<LobbyData> lobbyData;

void LobbyData::_saveStats()
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
		throw std::runtime_error("Cannot open file " + path.string());
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

void LobbyData::_loadBackgrounds()
{
	std::filesystem::path folder = profileFolderPath;
	auto path = folder / "assets/backgrounds/list.json";
	std::ifstream stream{path};
	nlohmann::json j;

	if (stream.fail())
		throw std::runtime_error("Cannot open file " + path.string());
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
		throw std::runtime_error("Cannot open file " + path.string());
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
}

LobbyData::~LobbyData()
{
	this->_saveStats();
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
