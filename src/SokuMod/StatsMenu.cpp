//
// Created by PinkySmile on 09/11/2022.
//

#include <filesystem>
#include <../directx/dinput.h>
#include "StatsMenu.hpp"
#include "LobbyData.hpp"
#include "data.hpp"

StatsMenu::StatsMenu()
{
	SokuLib::FontDescription desc;
	bool hasEnglishPatch = (*(int *)0x411c64 == 1);

	desc.r1 = 255;
	desc.r2 = 255;
	desc.g1 = 255;
	desc.g2 = 255;
	desc.b1 = 255;
	desc.b2 = 255;
	desc.height = 12 + hasEnglishPatch * 2;
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
	this->_defaultFont12.create();
	this->_defaultFont12.setIndirect(desc);

	this->title.texture.loadFromFile((std::filesystem::path(profileFolderPath) / "assets/menu/titleStats.png").string().c_str());
	this->title.setSize(this->title.texture.getSize());
	this->title.setPosition({23, 6});
	this->title.rect.width = this->title.getSize().x;
	this->title.rect.height = this->title.getSize().y;

	this->_stats.emplace_back(new ChrEntry());

	auto ptr = this->_stats.front();
	auto start = ptr;
	SokuLib::Vector2i size;
	unsigned id = 0;

	ptr->portrait.texture.createFromText("Global Stats", this->_defaultFont12, {200, 20}, &size);
	ptr->portrait.setSize(size.to<unsigned>());
	ptr->portrait.setPosition({50, 98});
	ptr->portrait.rect.width = size.x;
	ptr->portrait.rect.height = size.y;

	ptr->name.texture.createFromText("Name", this->_defaultFont12, {200, 20}, &size);
	ptr->name.setSize(size.to<unsigned>());
	ptr->name.setPosition({146, 118});
	ptr->name.rect.width = size.x;
	ptr->name.rect.height = size.y;

	ptr->wins.texture.createFromText("Wins", this->_defaultFont12, {200, 20}, &size);
	ptr->wins.setSize(size.to<unsigned>());
	ptr->wins.setPosition({245, 118});
	ptr->wins.rect.width = size.x;
	ptr->wins.rect.height = size.y;

	ptr->losses.texture.createFromText("Losses", this->_defaultFont12, {200, 20}, &size);
	ptr->losses.setSize(size.to<unsigned>());
	ptr->losses.setPosition({297, 118});
	ptr->losses.rect.width = size.x;
	ptr->losses.rect.height = size.y;

	ptr->total.texture.createFromText("Total Games", this->_defaultFont12, {200, 20}, &size);
	ptr->total.setSize(size.to<unsigned>());
	ptr->total.setPosition({358, 118});
	ptr->total.rect.width = size.x;
	ptr->total.rect.height = size.y;

	ptr->winratio.texture.createFromText("Win Ratio", this->_defaultFont12, {200, 20}, &size);
	ptr->winratio.setSize(size.to<unsigned>());
	ptr->winratio.setPosition({460, 118});
	ptr->winratio.rect.width = size.x;
	ptr->winratio.rect.height = size.y;

	for (auto &chr : characters) {
		auto stat = lobbyData->loadedCharacterStats.find(chr.first);
		auto val = stat == lobbyData->loadedCharacterStats.end() ? LobbyData::CharacterStatEntry{} : stat->second;

		this->_stats.emplace_back(new ChrEntry());
		ptr = this->_stats.back();

		ptr->portrait.texture.loadFromGame(("data/character/" + chr.second.codeName + "/face/face000.png").c_str());
		ptr->portrait.setSize({ptr->portrait.texture.getSize().x / 2, ptr->portrait.texture.getSize().y / 2});
		ptr->portrait.rect.width = ptr->portrait.texture.getSize().x;
		ptr->portrait.rect.height = ptr->portrait.texture.getSize().y;

		ptr->name.texture.createFromText(chr.second.fullName.c_str(), this->_defaultFont12, {200, 20}, &size);
		ptr->name.setSize(size.to<unsigned>());
		ptr->name.rect.width = size.x;
		ptr->name.rect.height = size.y;

		ptr->wins.texture.createFromText(std::to_string(val.wins).c_str(), this->_defaultFont12, {200, 20}, &size);
		ptr->wins.setSize(size.to<unsigned>());
		ptr->wins.rect.width = size.x;
		ptr->wins.rect.height = size.y;

		ptr->losses.texture.createFromText(std::to_string(val.losses).c_str(), this->_defaultFont12, {200, 20}, &size);
		ptr->losses.setSize(size.to<unsigned>());
		ptr->losses.rect.width = size.x;
		ptr->losses.rect.height = size.y;

		ptr->total.texture.createFromText(std::to_string(val.wins + val.losses).c_str(), this->_defaultFont12, {200, 20}, &size);
		ptr->total.setSize(size.to<unsigned>());
		ptr->total.rect.width = size.x;
		ptr->total.rect.height = size.y;

		ptr->winratio.texture.createFromText((val.wins + val.losses == 0) ? "N/A" : (std::to_string((val.wins * 100) / (val.wins + val.losses)) + "%").c_str(), this->_defaultFont12, {200, 20}, &size);
		ptr->winratio.setSize(size.to<unsigned>());
		ptr->winratio.rect.width = size.x;
		ptr->winratio.rect.height = size.y;
		id++;
	}
}

void StatsMenu::_()
{
	puts("_ !");
	*(int *)0x882a94 = 0x16;
}

int StatsMenu::onProcess()
{
	if (SokuLib::checkKeyOneshot(DIK_ESCAPE, 0, 0, 0) || SokuLib::inputMgrs.input.b == 1) {
		SokuLib::playSEWaveBuffer(0x29);
		return false;
	}
	if (std::abs(SokuLib::inputMgrs.input.verticalAxis) == 1 || (std::abs(SokuLib::inputMgrs.input.verticalAxis) >= 36 && std::abs(SokuLib::inputMgrs.input.verticalAxis) % 6 == 0)) {
		if (this->_start && SokuLib::inputMgrs.input.verticalAxis < 0) {
			SokuLib::playSEWaveBuffer(0x27);
			this->_start--;
		} else if (this->_start < this->_stats.size() - 11 && SokuLib::inputMgrs.input.verticalAxis > 0) {
			SokuLib::playSEWaveBuffer(0x27);
			this->_start++;
		}
	}

	unsigned id = 0;
	unsigned startI = this->_start + 1;
	auto start = this->_stats.front();

	for (const auto &ptr : this->_stats) {
		if (startI) {
			startI--;
			continue;
		}

		ptr->portrait.setPosition({22, static_cast<int>(135 + 27 * id)});
		ptr->name.setPosition({
			static_cast<int>(start->name.getPosition().x + start->name.getSize().x / 2 - ptr->name.getSize().x / 2),
			static_cast<int>(139 + 27 * id)
		});
		ptr->wins.setPosition({
			static_cast<int>(start->wins.getPosition().x + start->wins.getSize().x / 2 - ptr->wins.getSize().x / 2),
			static_cast<int>(139 + 27 * id)
		});
		ptr->losses.setPosition({
			static_cast<int>(start->losses.getPosition().x + start->losses.getSize().x / 2 - ptr->losses.getSize().x / 2),
			static_cast<int>(139 + 27 * id)
		});
		ptr->total.setPosition({
			static_cast<int>(start->total.getPosition().x + start->total.getSize().x / 2 - ptr->total.getSize().x / 2),
			static_cast<int>(139 + 27 * id)
		});
		ptr->winratio.setPosition({
			static_cast<int>(start->winratio.getPosition().x + start->winratio.getSize().x / 2 - ptr->winratio.getSize().x / 2),
			static_cast<int>(139 + 27 * id)
		});
		id++;
		if (id == 10)
			break;
	}
	return true;
}

int StatsMenu::onRender()
{
	unsigned id = 0;

	this->title.draw();
	for (auto &stat : this->_stats) {
		if (id != 0 && id < this->_start + 1) {
			id++;
			continue;
		}
		id++;
		stat->portrait.draw();
		stat->name.draw();
		stat->wins.draw();
		stat->losses.draw();
		stat->total.draw();
		stat->winratio.draw();
		if (id > this->_start + 10)
			break;
	}
	return 0;
}
