//
// Created by PinkySmile on 09/11/2022.
//

#include <filesystem>
#include <set>
#include <../directx/dinput.h>
#include "StatsMenu.hpp"
#include "LobbyData.hpp"
#include "data.hpp"

void StatsMenu::_createGlobalStats()
{
	this->_globalStats.emplace_back(new ChrEntry());

	auto ptr = this->_globalStats.front();
	SokuLib::Vector2i size;

	ptr->portrait.texture.createFromText("Stats Playing As", lobbyData->getFont(12), {200, 20}, &size);
	ptr->portrait.setSize(size.to<unsigned>());
	ptr->portrait.setPosition({50, 98});
	ptr->portrait.rect.width = size.x;
	ptr->portrait.rect.height = size.y;

	ptr->name.texture.createFromText("Name", lobbyData->getFont(12), {200, 20}, &size);
	ptr->name.setSize(size.to<unsigned>());
	ptr->name.setPosition({146, 118});
	ptr->name.rect.width = size.x;
	ptr->name.rect.height = size.y;

	ptr->wins.texture.createFromText("Wins", lobbyData->getFont(12), {200, 20}, &size);
	ptr->wins.setSize(size.to<unsigned>());
	ptr->wins.setPosition({245, 118});
	ptr->wins.rect.width = size.x;
	ptr->wins.rect.height = size.y;

	ptr->losses.texture.createFromText("Losses", lobbyData->getFont(12), {200, 20}, &size);
	ptr->losses.setSize(size.to<unsigned>());
	ptr->losses.setPosition({297, 118});
	ptr->losses.rect.width = size.x;
	ptr->losses.rect.height = size.y;

	ptr->total.texture.createFromText("Total Games", lobbyData->getFont(12), {200, 20}, &size);
	ptr->total.setSize(size.to<unsigned>());
	ptr->total.setPosition({358, 118});
	ptr->total.rect.width = size.x;
	ptr->total.rect.height = size.y;

	ptr->winratio.texture.createFromText("Win Ratio", lobbyData->getFont(12), {200, 20}, &size);
	ptr->winratio.setSize(size.to<unsigned>());
	ptr->winratio.setPosition({460, 118});
	ptr->winratio.rect.width = size.x;
	ptr->winratio.rect.height = size.y;

	for (auto &chr : characters) {
		auto stat = lobbyData->loadedCharacterStats.find(chr.first);
		auto val = stat == lobbyData->loadedCharacterStats.end() ? LobbyData::CharacterStatEntry{} : stat->second;

		this->_globalStats.emplace_back(new ChrEntry());
		ptr = this->_globalStats.back();

		if (chr.first != SokuLib::CHARACTER_RANDOM)
			ptr->portrait.texture.loadFromGame(("data/character/" + chr.second.codeName + "/face/face000.png").c_str());
		else
			ptr->portrait.texture.loadFromFile((std::filesystem::path(profileFolderPath) / "assets/menu/random_face000.png").string().c_str());
		ptr->portrait.setSize({ptr->portrait.texture.getSize().x / 2, ptr->portrait.texture.getSize().y / 2});
		ptr->portrait.rect.width = ptr->portrait.texture.getSize().x;
		ptr->portrait.rect.height = ptr->portrait.texture.getSize().y;

		ptr->name.texture.createFromText(chr.second.fullName.c_str(), lobbyData->getFont(12), {200, 20}, &size);
		ptr->name.setSize(size.to<unsigned>());
		ptr->name.rect.width = size.x;
		ptr->name.rect.height = size.y;

		ptr->wins.texture.createFromText(std::to_string(val.wins).c_str(), lobbyData->getFont(12), {200, 20}, &size);
		ptr->wins.setSize(size.to<unsigned>());
		ptr->wins.rect.width = size.x;
		ptr->wins.rect.height = size.y;

		ptr->losses.texture.createFromText(std::to_string(val.losses).c_str(), lobbyData->getFont(12), {200, 20}, &size);
		ptr->losses.setSize(size.to<unsigned>());
		ptr->losses.rect.width = size.x;
		ptr->losses.rect.height = size.y;

		ptr->total.texture.createFromText(std::to_string(val.wins + val.losses).c_str(), lobbyData->getFont(12), {200, 20}, &size);
		ptr->total.setSize(size.to<unsigned>());
		ptr->total.rect.width = size.x;
		ptr->total.rect.height = size.y;

		ptr->winratio.texture.createFromText((val.wins + val.losses == 0) ? "N/A" : (std::to_string((val.wins * 100) / (val.wins + val.losses)) + "%").c_str(), lobbyData->getFont(12), {200, 20}, &size);
		ptr->winratio.setSize(size.to<unsigned>());
		ptr->winratio.rect.width = size.x;
		ptr->winratio.rect.height = size.y;
	}
}

void StatsMenu::_createAgainstStats()
{
	this->_againstStats.emplace_back(new ChrEntry());

	auto ptr = this->_againstStats.front();
	SokuLib::Vector2i size;

	ptr->portrait.texture.createFromText("Stats Playing Against", lobbyData->getFont(12), {200, 20}, &size);
	ptr->portrait.setSize(size.to<unsigned>());
	ptr->portrait.setPosition({50, 98});
	ptr->portrait.rect.width = size.x;
	ptr->portrait.rect.height = size.y;

	ptr->name.texture.createFromText("Name", lobbyData->getFont(12), {200, 20}, &size);
	ptr->name.setSize(size.to<unsigned>());
	ptr->name.setPosition({146, 118});
	ptr->name.rect.width = size.x;
	ptr->name.rect.height = size.y;

	ptr->wins.texture.createFromText("Wins", lobbyData->getFont(12), {200, 20}, &size);
	ptr->wins.setSize(size.to<unsigned>());
	ptr->wins.setPosition({245, 118});
	ptr->wins.rect.width = size.x;
	ptr->wins.rect.height = size.y;

	ptr->losses.texture.createFromText("Losses", lobbyData->getFont(12), {200, 20}, &size);
	ptr->losses.setSize(size.to<unsigned>());
	ptr->losses.setPosition({297, 118});
	ptr->losses.rect.width = size.x;
	ptr->losses.rect.height = size.y;

	ptr->total.texture.createFromText("Total Games", lobbyData->getFont(12), {200, 20}, &size);
	ptr->total.setSize(size.to<unsigned>());
	ptr->total.setPosition({358, 118});
	ptr->total.rect.width = size.x;
	ptr->total.rect.height = size.y;

	ptr->winratio.texture.createFromText("Win Ratio", lobbyData->getFont(12), {200, 20}, &size);
	ptr->winratio.setSize(size.to<unsigned>());
	ptr->winratio.setPosition({460, 118});
	ptr->winratio.rect.width = size.x;
	ptr->winratio.rect.height = size.y;

	for (auto &chr : characters) {
		auto stat = lobbyData->loadedCharacterStats.find(chr.first);
		auto val = stat == lobbyData->loadedCharacterStats.end() ? LobbyData::CharacterStatEntry{} : stat->second;

		this->_againstStats.emplace_back(new ChrEntry());
		ptr = this->_againstStats.back();

		if (chr.first != SokuLib::CHARACTER_RANDOM)
			ptr->portrait.texture.loadFromGame(("data/character/" + chr.second.codeName + "/face/face000.png").c_str());
		else
			ptr->portrait.texture.loadFromFile((std::filesystem::path(profileFolderPath) / "assets/menu/random_face000.png").string().c_str());
		ptr->portrait.setSize({ptr->portrait.texture.getSize().x / 2, ptr->portrait.texture.getSize().y / 2});
		ptr->portrait.rect.width = ptr->portrait.texture.getSize().x;
		ptr->portrait.rect.height = ptr->portrait.texture.getSize().y;

		ptr->name.texture.createFromText(chr.second.fullName.c_str(), lobbyData->getFont(12), {200, 20}, &size);
		ptr->name.setSize(size.to<unsigned>());
		ptr->name.rect.width = size.x;
		ptr->name.rect.height = size.y;

		ptr->wins.texture.createFromText(std::to_string(val.againstWins).c_str(), lobbyData->getFont(12), {200, 20}, &size);
		ptr->wins.setSize(size.to<unsigned>());
		ptr->wins.rect.width = size.x;
		ptr->wins.rect.height = size.y;

		ptr->losses.texture.createFromText(std::to_string(val.againstLosses).c_str(), lobbyData->getFont(12), {200, 20}, &size);
		ptr->losses.setSize(size.to<unsigned>());
		ptr->losses.rect.width = size.x;
		ptr->losses.rect.height = size.y;

		ptr->total.texture.createFromText(std::to_string(val.againstWins + val.againstLosses).c_str(), lobbyData->getFont(12), {200, 20}, &size);
		ptr->total.setSize(size.to<unsigned>());
		ptr->total.rect.width = size.x;
		ptr->total.rect.height = size.y;

		ptr->winratio.texture.createFromText((val.againstWins + val.againstLosses == 0) ? "N/A" : (std::to_string((val.againstWins * 100) / (val.againstWins + val.againstLosses)) + "%").c_str(), lobbyData->getFont(12), {200, 20}, &size);
		ptr->winratio.setSize(size.to<unsigned>());
		ptr->winratio.rect.width = size.x;
		ptr->winratio.rect.height = size.y;
	}
}

void StatsMenu::_createMUStats(std::vector<std::shared_ptr<ChrEntry>> &list, const std::pair<unsigned, Character> &chrs)
{
	list.emplace_back(new ChrEntry());

	auto ptr = list.front();
	SokuLib::Vector2i size;

	ptr->portraitTitle.texture.loadFromGame(("data/character/" + chrs.second.codeName + "/face/face000.png").c_str());
	ptr->portraitTitle.setSize({ptr->portraitTitle.texture.getSize().x / 2, ptr->portraitTitle.texture.getSize().y / 2});
	ptr->portraitTitle.setPosition({20, 98});
	ptr->portraitTitle.rect.width = ptr->portraitTitle.texture.getSize().x;
	ptr->portraitTitle.rect.height = ptr->portraitTitle.texture.getSize().y;

	ptr->portrait.texture.createFromText(("Stats Playing As " + chrs.second.firstName + " Against").c_str(), lobbyData->getFont(12), {200, 20}, &size);
	ptr->portrait.setSize(size.to<unsigned>());
	ptr->portrait.setPosition({70, 98});
	ptr->portrait.rect.width = size.x;
	ptr->portrait.rect.height = size.y;

	ptr->name.texture.createFromText("Name", lobbyData->getFont(12), {200, 20}, &size);
	ptr->name.setSize(size.to<unsigned>());
	ptr->name.setPosition({146, 118});
	ptr->name.rect.width = size.x;
	ptr->name.rect.height = size.y;

	ptr->wins.texture.createFromText("Wins", lobbyData->getFont(12), {200, 20}, &size);
	ptr->wins.setSize(size.to<unsigned>());
	ptr->wins.setPosition({245, 118});
	ptr->wins.rect.width = size.x;
	ptr->wins.rect.height = size.y;

	ptr->losses.texture.createFromText("Losses", lobbyData->getFont(12), {200, 20}, &size);
	ptr->losses.setSize(size.to<unsigned>());
	ptr->losses.setPosition({297, 118});
	ptr->losses.rect.width = size.x;
	ptr->losses.rect.height = size.y;

	ptr->total.texture.createFromText("Total Games", lobbyData->getFont(12), {200, 20}, &size);
	ptr->total.setSize(size.to<unsigned>());
	ptr->total.setPosition({358, 118});
	ptr->total.rect.width = size.x;
	ptr->total.rect.height = size.y;

	ptr->winratio.texture.createFromText("Win Ratio", lobbyData->getFont(12), {200, 20}, &size);
	ptr->winratio.setSize(size.to<unsigned>());
	ptr->winratio.setPosition({460, 118});
	ptr->winratio.rect.width = size.x;
	ptr->winratio.rect.height = size.y;

	for (auto &chr : characters) {
		if (chr.first == SokuLib::CHARACTER_RANDOM)
			continue;

		auto stat = lobbyData->loadedMatchupStats.find({chrs.first, chr.first});
		auto val = stat == lobbyData->loadedMatchupStats.end() ? LobbyData::MatchupStatEntry{} : stat->second;

		list.emplace_back(new ChrEntry());
		ptr = list.back();

		ptr->portrait.texture.loadFromGame(("data/character/" + chr.second.codeName + "/face/face000.png").c_str());
		ptr->portrait.setSize({ptr->portrait.texture.getSize().x / 2, ptr->portrait.texture.getSize().y / 2});
		ptr->portrait.rect.width = ptr->portrait.texture.getSize().x;
		ptr->portrait.rect.height = ptr->portrait.texture.getSize().y;

		ptr->name.texture.createFromText(chr.second.fullName.c_str(), lobbyData->getFont(12), {200, 20}, &size);
		ptr->name.setSize(size.to<unsigned>());
		ptr->name.rect.width = size.x;
		ptr->name.rect.height = size.y;

		ptr->wins.texture.createFromText(std::to_string(val.wins).c_str(), lobbyData->getFont(12), {200, 20}, &size);
		ptr->wins.setSize(size.to<unsigned>());
		ptr->wins.rect.width = size.x;
		ptr->wins.rect.height = size.y;

		ptr->losses.texture.createFromText(std::to_string(val.losses).c_str(), lobbyData->getFont(12), {200, 20}, &size);
		ptr->losses.setSize(size.to<unsigned>());
		ptr->losses.rect.width = size.x;
		ptr->losses.rect.height = size.y;

		ptr->total.texture.createFromText(std::to_string(val.wins + val.losses).c_str(), lobbyData->getFont(12), {200, 20}, &size);
		ptr->total.setSize(size.to<unsigned>());
		ptr->total.rect.width = size.x;
		ptr->total.rect.height = size.y;

		ptr->winratio.texture.createFromText((val.wins + val.losses == 0) ? "N/A" : (std::to_string((val.wins * 100) / (val.wins + val.losses)) + "%").c_str(), lobbyData->getFont(12), {200, 20}, &size);
		ptr->winratio.setSize(size.to<unsigned>());
		ptr->winratio.rect.width = size.x;
		ptr->winratio.rect.height = size.y;
	}
}

void StatsMenu::_createCardsStats(std::vector<std::shared_ptr<ChrEntry>> &list, const std::pair<unsigned, struct Character> &chr)
{
	list.emplace_back(new ChrEntry());

	auto ptr = list.front();
	SokuLib::Vector2i size;
	std::set<unsigned short> cards;

	ptr->portraitTitle.texture.loadFromGame(("data/character/" + chr.second.codeName + "/face/face000.png").c_str());
	ptr->portraitTitle.setSize({ptr->portraitTitle.texture.getSize().x / 2, ptr->portraitTitle.texture.getSize().y / 2});
	ptr->portraitTitle.setPosition({20, 98});
	ptr->portraitTitle.rect.width = ptr->portraitTitle.texture.getSize().x;
	ptr->portraitTitle.rect.height = ptr->portraitTitle.texture.getSize().y;

	ptr->portrait.texture.createFromText(("Card Usage As " + chr.second.firstName).c_str(), lobbyData->getFont(12), {200, 20}, &size);
	ptr->portrait.setSize(size.to<unsigned>());
	ptr->portrait.setPosition({70, 98});
	ptr->portrait.rect.width = size.x;
	ptr->portrait.rect.height = size.y;

	ptr->name.texture.createFromText("Name", lobbyData->getFont(12), {450, 20}, &size);
	ptr->name.setSize(size.to<unsigned>());
	ptr->name.setPosition({166, 118});
	ptr->name.rect.width = size.x;
	ptr->name.rect.height = size.y;

/*	ptr->wins.texture.createFromText("In Deck", lobbyData->getFont(12), {200, 20}, &size);
	ptr->wins.setSize(size.to<unsigned>());
	ptr->wins.setPosition({345, 118});
	ptr->wins.rect.width = size.x;
	ptr->wins.rect.height = size.y;

	ptr->losses.texture.createFromText("Used", lobbyData->getFont(12), {200, 20}, &size);
	ptr->losses.setSize(size.to<unsigned>());
	ptr->losses.setPosition({452, 118});
	ptr->losses.rect.width = size.x;
	ptr->losses.rect.height = size.y;

	ptr->total.texture.createFromText("Fuel", lobbyData->getFont(12), {200, 20}, &size);
	ptr->total.setSize(size.to<unsigned>());
	ptr->total.setPosition({558, 118});
	ptr->total.rect.width = size.x;
	ptr->total.rect.height = size.y;*/

	ptr->losses.texture.createFromText("Used?", lobbyData->getFont(12), {200, 20}, &size);
	ptr->losses.setSize(size.to<unsigned>());
	ptr->losses.setPosition({452, 118});
	ptr->losses.rect.width = size.x;
	ptr->losses.rect.height = size.y;

	auto entry = lobbyData->loadedCharacterCardUsage.find(chr.first);
	auto val1 = entry == lobbyData->loadedCharacterCardUsage.end() ? LobbyData::CardChrStatEntry{0} : entry->second;

	for (unsigned i = 0; i <= 20; i++)
		cards.insert(i);
	for (auto &card : lobbyData->cardsTextures[chr.first])
		cards.insert(card.first);
	for (auto &card : val1.cards)
		cards.insert(card.first);

	for (auto &card : cards) {
		char buffer[] = "data/csv/000000000000/spellcard.csv";
		auto stat = val1.cards.find(card);
		auto val2 = stat == val1.cards.end() ? LobbyData::CardStatEntry{0, 0, 0} : stat->second;
		auto &map = lobbyData->cardsTextures[card < 100 ? SokuLib::CHARACTER_RANDOM : chr.first];
		auto nameIt = map.find(card);

		list.emplace_back(new ChrEntry());
		ptr = list.back();

		sprintf(buffer, "data/card/%s/card%03i.bmp", card < 100 ? "common" : chr.second.codeName.c_str(), card);
		if (!ptr->portrait.texture.loadFromGame(buffer))
			ptr->portrait.texture.loadFromGame("data/battle/cardFaceDown.bmp");
		ptr->portrait.setSize({ptr->portrait.texture.getSize().x / 2, ptr->portrait.texture.getSize().y / 2});
		ptr->portrait.rect.width = ptr->portrait.texture.getSize().x;
		ptr->portrait.rect.height = ptr->portrait.texture.getSize().y;

		ptr->name.texture.createFromText(nameIt == map.end() ? ("Unknown card #" + std::to_string(card)).c_str() : nameIt->second.cardName.c_str(), lobbyData->getFont(12), {450, 20}, &size);
		ptr->name.setSize(size.to<unsigned>());
		ptr->name.rect.width = size.x;
		ptr->name.rect.height = size.y;

		/*if (val1.nbGames != 0)
			sprintf(buffer, "%u (%.2f/game)", val2.inDeck, val2.inDeck / (float)val1.nbGames);
		else
			sprintf(buffer, "%u (0/game)", val2.inDeck);
		ptr->wins.texture.createFromText(buffer, lobbyData->getFont(12), {200, 20}, &size);
		ptr->wins.setSize(size.to<unsigned>());
		ptr->wins.rect.width = size.x;
		ptr->wins.rect.height = size.y;

		if (val1.nbGames != 0)
			sprintf(buffer, "%u (%.2f/game)", val2.used, val2.used / (float)val1.nbGames);
		else
			sprintf(buffer, "%u (0/game)", val2.used);
		ptr->losses.texture.createFromText(buffer, lobbyData->getFont(12), {200, 20}, &size);
		ptr->losses.setSize(size.to<unsigned>());
		ptr->losses.rect.width = size.x;
		ptr->losses.rect.height = size.y;

		if (val1.nbGames != 0)
			sprintf(buffer, "%u (%.2f/game)", val2.burnt, val2.burnt / (float)val1.nbGames);
		else
			sprintf(buffer, "%u (0/game)", val2.burnt);
		ptr->total.texture.createFromText(buffer, lobbyData->getFont(12), {200, 20}, &size);
		ptr->total.setSize(size.to<unsigned>());
		ptr->total.rect.width = size.x;
		ptr->total.rect.height = size.y;*/
		ptr->losses.texture.createFromText(val2.used ? "Used" : "Not used", lobbyData->getFont(12), {200, 20}, &size);
		ptr->losses.setSize(size.to<unsigned>());
		ptr->losses.rect.width = size.x;
		ptr->losses.rect.height = size.y;
	}
}

StatsMenu::StatsMenu()
{
	this->title.texture.loadFromFile((std::filesystem::path(profileFolderPath) / "assets/menu/titleStats.png").string().c_str());
	this->title.setSize(this->title.texture.getSize());
	this->title.setPosition({23, 6});
	this->title.rect.width = this->title.getSize().x;
	this->title.rect.height = this->title.getSize().y;
	this->_createGlobalStats();
	this->_matchupStats.reserve(characters.size() - 1);
	for (auto &entry : characters) {
		if (entry.first == SokuLib::CHARACTER_RANDOM)
			continue;
		this->_matchupStats.emplace_back();
	}
	this->_cardsStats.reserve(characters.size() - 1);
	for (auto &entry : characters) {
		if (entry.first == SokuLib::CHARACTER_RANDOM)
			continue;
		this->_cardsStats.emplace_back();
	}
	this->_nbMenus = 2 + characters.size() - 1;
}

void StatsMenu::_()
{
	puts("_ !");
	*(int *)0x882a94 = 0x16;
}

void StatsMenu::_updateNormalStats(const std::vector<std::shared_ptr<ChrEntry>> &list, unsigned maxLine, unsigned lineSize)
{
	if (list.empty())
		return;

	unsigned id = 0;
	auto start = list.front();

	for (int i = this->_start + 1, id = 0; id < maxLine && i < list.size(); i++, id++) {
		const auto &ptr = list[i];

		ptr->portrait.setPosition({
			22,
			static_cast<int>(135 + lineSize * id)
		});
		ptr->name.setPosition({
			static_cast<int>(start->name.getPosition().x + start->name.getSize().x / 2 - ptr->name.getSize().x / 2),
			static_cast<int>(139 + lineSize * id)
		});
		ptr->wins.setPosition({
			static_cast<int>(start->wins.getPosition().x + start->wins.getSize().x / 2 - ptr->wins.getSize().x / 2),
			static_cast<int>(139 + lineSize * id)
		});
		ptr->losses.setPosition({
			static_cast<int>(start->losses.getPosition().x + start->losses.getSize().x / 2 - ptr->losses.getSize().x / 2),
			static_cast<int>(139 + lineSize * id)
		});
		ptr->total.setPosition({
			static_cast<int>(start->total.getPosition().x + start->total.getSize().x / 2 - ptr->total.getSize().x / 2),
			static_cast<int>(139 + lineSize * id)
		});
		ptr->winratio.setPosition({
			static_cast<int>(start->winratio.getPosition().x + start->winratio.getSize().x / 2 - ptr->winratio.getSize().x / 2),
			static_cast<int>(139 + lineSize * id)
		});
	}
}

std::vector<std::shared_ptr<StatsMenu::ChrEntry>> *StatsMenu::_getCurrentList(unsigned *maxLine, unsigned *lineSize)
{
	std::vector<std::shared_ptr<ChrEntry>> *list = nullptr;

	*maxLine = 10;
	*lineSize = 27;
	if (this->_currentMenu == 0)
		list = &this->_globalStats;
	else if (this->_currentMenu == 1) {
		list = &this->_againstStats;
		if (list->empty())
			this->_createAgainstStats();
	} else if (this->_currentMenu < 2 + characters.size() - 1 + characters.size() - 1 && this->_currentMenu % 2 == 0) {
		list = &this->_matchupStats[(this->_currentMenu - 2) / 2];
		if (list->empty()) {
			auto it = characters.begin();

			for (unsigned i = 0; i < (this->_currentMenu - 2) / 2; i++)
				it++;
			this->_createMUStats(this->_matchupStats[(this->_currentMenu - 2) / 2], *it);
		}
	} else if (this->_currentMenu < 2 + characters.size() - 1 + characters.size() - 1) {
		list = &this->_cardsStats[(this->_currentMenu - 2) / 2];
		if (list->empty()) {
			auto it = characters.begin();

			for (unsigned i = 0; i < (this->_currentMenu - 2) / 2; i++)
				it++;
			this->_createCardsStats(this->_cardsStats[(this->_currentMenu - 2) / 2], *it);
		}
		*maxLine = 8;
		*lineSize = 34;
	}
	return list;
}

int StatsMenu::onProcess()
{
	if (SokuLib::checkKeyOneshot(DIK_ESCAPE, 0, 0, 0) || SokuLib::inputMgrs.input.b == 1) {
		playSound(0x29);
		return false;
	}

	unsigned maxLine = 10;
	unsigned lineSize = 27;
	auto list = this->_getCurrentList(&maxLine, &lineSize);

	if (std::abs(SokuLib::inputMgrs.input.verticalAxis) == 1 || (std::abs(SokuLib::inputMgrs.input.verticalAxis) >= 36 && std::abs(SokuLib::inputMgrs.input.verticalAxis) % 6 == 0)) {
		if (this->_start && SokuLib::inputMgrs.input.verticalAxis < 0) {
			playSound(0x27);
			this->_start--;
		} else if (list && this->_start < list->size() - maxLine - 1 && SokuLib::inputMgrs.input.verticalAxis > 0) {
			playSound(0x27);
			this->_start++;
		}
	}
	if (std::abs(SokuLib::inputMgrs.input.horizontalAxis) == 1 || (std::abs(SokuLib::inputMgrs.input.horizontalAxis) >= 36 && std::abs(SokuLib::inputMgrs.input.horizontalAxis) % 6 == 0)) {
		if (SokuLib::inputMgrs.input.horizontalAxis > 0) {
			playSound(0x27);
			this->_currentMenu++;
			if (this->_currentMenu == this->_nbMenus)
				this->_currentMenu = 0;
		} else if (SokuLib::inputMgrs.input.horizontalAxis < 0) {
			playSound(0x27);
			if (this->_currentMenu == 0)
				this->_currentMenu = this->_nbMenus - 1;
			else
				this->_currentMenu--;
		}
		this->_start = 0;
		list = this->_getCurrentList(&maxLine, &lineSize);
	}
	this->_updateNormalStats(*list, maxLine, lineSize);
	return true;
}

void StatsMenu::_renderNormalStats(const std::vector<std::shared_ptr<ChrEntry>> &list, unsigned maxLine)
{
	if (list.empty())
		return;
	this->title.draw();
	list[0]->portraitTitle.draw();
	list[0]->portrait.draw();
	list[0]->name.draw();
	list[0]->wins.draw();
	list[0]->losses.draw();
	list[0]->total.draw();
	list[0]->winratio.draw();
	for (unsigned i = this->_start + 1, id = 0; id < maxLine && i < list.size(); i++, id++) {
		auto &stat = list[i];

		stat->portrait.draw();
		stat->name.draw();
		stat->wins.draw();
		stat->losses.draw();
		stat->total.draw();
		stat->winratio.draw();
	}
}

int StatsMenu::onRender()
{
	if (this->_currentMenu == 0)
		this->_renderNormalStats(this->_globalStats, 10);
	else if (this->_currentMenu == 1)
		this->_renderNormalStats(this->_againstStats, 10);
	else if (this->_currentMenu < 2 + characters.size() - 1 + characters.size() - 1 && this->_currentMenu % 2 == 0)
		this->_renderNormalStats(this->_matchupStats[(this->_currentMenu - 2) / 2], 10);
	else if (this->_currentMenu < 2 + characters.size() - 1 + characters.size() - 1)
		this->_renderNormalStats(this->_cardsStats[(this->_currentMenu - 2) / 2], 8);
	return 0;
}
