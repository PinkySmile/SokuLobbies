//
// Created by PinkySmile on 07/01/2023.
//

#define _USE_MATH_DEFINES
#include <filesystem>
#include "nlohmann/json.hpp"
#include "data.hpp"
#include "SmallHostlist.hpp"
#include "LobbyData.hpp"

#define MAX_OVERLAY_ANIMATION 15
#define modifyPos(x, y) ((SokuLib::Vector2i{static_cast<int>(x), static_cast<int>(y)} * this->_ratio + this->_pos).to<int>())

SmallHostlist::SmallHostlist(float ratio, SokuLib::Vector2i pos, SokuLib::MenuConnect *parent) :
	_parent(parent),
	_pos(pos),
	_ratio(ratio)
{
	SokuLib::Vector2i size;

	for (unsigned i = 0; i < this->_sprites.size(); i++) {
		auto &sprite = this->_sprites[i];

		if (i == this->_sprites.size() - 2) {
			sprite.texture.loadFromFile((std::filesystem::path(profileFolderPath) / "assets/arcades/CRTeffect.png").string().c_str());
			sprite.setSize(sprite.texture.getSize());
		} else {
			if (i == this->_sprites.size() - 1)
				sprite.texture.loadFromFile((std::filesystem::path(profileFolderPath) / "assets/arcades/title.png").string().c_str());
			else if (i == this->_sprites.size() - 3)
				sprite.texture.loadFromFile((std::filesystem::path(profileFolderPath) / "assets/arcades/hostlistBg.png").string().c_str());
			else
				sprite.texture.loadFromGame(_spritesPaths[i]);
			sprite.setSize((sprite.texture.getSize() * ratio).to<unsigned>());
		}
		sprite.rect.width = sprite.texture.getSize().x;
		sprite.rect.height = sprite.texture.getSize().y;
	}
	this->_sprites[1].tint = SokuLib::Color{0x40, 0x40, 0x40, 0xFF};
	this->_sprites[2].tint = SokuLib::Color{0x40, 0x40, 0x40, 0xFF};
	this->_background.emplace_back(new Image(this->_sprites.back(), modifyPos(0, 0)));
	this->_background.emplace_back(new ScrollingImage(this->_sprites[1], modifyPos(-480, 0), modifyPos(0, 0), MAX_OVERLAY_ANIMATION));
	this->_background.emplace_back(new ScrollingImage(this->_sprites[2], modifyPos(640, 0), modifyPos(160, 0), MAX_OVERLAY_ANIMATION));

	this->_foreground.emplace_back(new Image(this->_sprites[0], modifyPos(15, 15)));
	this->_foreground.emplace_back(new Image(this->_sprites[this->_sprites.size() - 2], modifyPos(0, 0)));
	this->_foreground.emplace_back(new Image(this->_sprites[this->_sprites.size() - 3], modifyPos(0, 0)));

	this->_topOverlay.emplace_back(new Image(this->_sprites[24], modifyPos(0, -200)));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[22], modifyPos(203, -270), -0.005235987755982988));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[22], modifyPos(512, -265), 0.0017453292519943296));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[19], modifyPos(177, -205), 0.006981317007977318));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[19], modifyPos(245, -233), -0.005235987755982988));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[19], modifyPos(372, -199), 0.003490658503988659));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[18], modifyPos(24, -267), -0.003490658503988659));
	this->_topOverlay.emplace_back(new Image(this->_sprites[14], modifyPos(0, -200)));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[10], modifyPos(419, -236), 0.0017453292519943296));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[12], modifyPos(162, -206), 0.013981275716910515));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[12], modifyPos(187, -216), -0.013981275716910515));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[12], modifyPos(191, -190), 0.013981275716910515));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[11], modifyPos(217, -194), -0.006981317007977318));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[11], modifyPos(307, -206), 0.003490658503988659));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[11], modifyPos(546, -237), 0.0017453292519943296));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[11], modifyPos(451, -204), 0.0017453292519943296));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[11], modifyPos(362, -224), -0.003490658503988659));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[11], modifyPos(572, -184), -0.0017453292519943296));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[10], modifyPos(-47, -223), 0.005235987755982988));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[10], modifyPos(540, -216), -0.0017453292519943296));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[10], modifyPos(275, -238), 0.003490658503988659));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[10], modifyPos(40, -250), -0.003490658503988659));
	this->_topOverlay.emplace_back(new RotatingImage(this->_sprites[11], modifyPos(-15, -191), 0.005235987755982988));
	this->_topOverlay.emplace_back(new Image(this->_sprites[4], modifyPos(0, -200)));

	this->_botOverlay.emplace_back(new Image(this->_sprites[23], modifyPos(0, 616)));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[21], modifyPos(335, 626), -0.003490658503988659));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[21], modifyPos(490, 618), 0.003490658503988659));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[20], modifyPos(54, 609), 0.0017453292519943296));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[17], modifyPos(-26, 607), 0.006981317007977318));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[17], modifyPos(559, 603), -0.010471975511965976));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[17], modifyPos(632, 593), 0.010471975511965976));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[16], modifyPos(419, 619), 0.005235987755982988));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[15], modifyPos(6, 611), -0.0017453292519943296));
	this->_botOverlay.emplace_back(new Image(this->_sprites[13], modifyPos(0, 612)));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[7], modifyPos(435, 636), 0.005235987755982988));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[7], modifyPos(151, 613), 0.005235987755982988));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[7], modifyPos(-37, 626), 0.005235987755982988));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[8], modifyPos(363, 636), 0.010471975511965976));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[8], modifyPos(285, 622), 0.010471975511965976));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[8], modifyPos(467, 668), 0.005235987755982988));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[9], modifyPos(337, 663), -0.020943951023931952));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[9], modifyPos(262, 620), -0.020943951023931952));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[9], modifyPos(109, 658), 0.020943951023931952));
	this->_botOverlay.emplace_back(new RotatingImage(this->_sprites[9], modifyPos(83, 647), -0.020943951023931952));
	this->_botOverlay.emplace_back(new Image(this->_sprites[3], modifyPos(0, 632)));
	this->_netThread = std::thread([this]{
		while (this->_parent) {
			this->_refreshHostlist();
			for (int i = 0; i < 1000 && this->_parent; i++)
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	});

	this->_playing.texture.createFromText("Playing", lobbyData->getFont(12), {400, 20}, &size);
	this->_playing.setSize(size.to<unsigned>());
	this->_playing.rect.width = size.x;
	this->_playing.rect.height = size.y;
	this->_playing.setPosition(modifyPos(430, 94) + SokuLib::Vector2i{-this->_playing.rect.width / 2, 0});

	this->_hosting.texture.createFromText("Hosting", lobbyData->getFont(12), {400, 20}, &size);
	this->_hosting.setSize(size.to<unsigned>());
	this->_hosting.rect.width = size.x;
	this->_hosting.rect.height = size.y;
	this->_hosting.setPosition(modifyPos(430, 94) + SokuLib::Vector2i{-this->_hosting.rect.width / 2, 0});
}

SmallHostlist::~SmallHostlist()
{
	this->_parent = nullptr;
	if (this->_netThread.joinable())
		this->_netThread.join();
}

#define CRenderer_Unknown1 ((void (__thiscall *)(int, int))0x404AF0)

void SmallHostlist::_displaySokuCursor(SokuLib::Vector2i pos, SokuLib::Vector2u size)
{
	SokuLib::Sprite (&CursorSprites)[3] = *(SokuLib::Sprite (*)[3])0x89A6C0;

	//0x443a50 -> Vanilla display cursor
	CursorSprites[0].scale.x = size.x * 0.00195313f * this->_ratio;
	CursorSprites[0].scale.y = size.y / 16.f * this->_ratio;
	pos.x -= 7;
	CursorSprites[0].render(this->_pos.x + pos.x * this->_ratio, this->_pos.y + pos.y * this->_ratio);
	CRenderer_Unknown1(0x896B4C, 2);
	CursorSprites[1].scale.x = this->_ratio;
	CursorSprites[1].scale.y = this->_ratio;
	CursorSprites[1].rotation = *(float *)0x89A450 * 4.f;
	CursorSprites[1].render(this->_pos.x + pos.x * this->_ratio, this->_pos.y + (pos.y + 8.f) * this->_ratio);
	CursorSprites[2].scale.x = this->_ratio;
	CursorSprites[2].scale.y = this->_ratio;
	CursorSprites[2].rotation = -*(float *)0x89A450 * 4.f;
	CursorSprites[2].render(this->_pos.x + (pos.x - 14.f) * this->_ratio, this->_pos.y + (pos.y - 1.f) * this->_ratio);
	CRenderer_Unknown1(0x896B4C, 1);
	CursorSprites[0].scale.x = 1;
	CursorSprites[0].scale.y = 1;
	CursorSprites[1].scale.x = 1;
	CursorSprites[1].scale.y = 1;
	CursorSprites[2].scale.x = 1;
	CursorSprites[2].scale.y = 1;
}

bool SmallHostlist::update()
{
	if (this->_overlayTimer <= MAX_OVERLAY_ANIMATION) {
		int translate = (200 * this->_ratio) * std::pow((float)this->_overlayTimer / MAX_OVERLAY_ANIMATION, 2);

		if (this->_overlayTimer == 5)
			playSound(61);
		for (auto &elem : this->_topOverlay)
			elem->translate.y = translate;
		for (auto &elem : this->_botOverlay)
			elem->translate.y = 1 - translate;
		this->_overlayTimer++;
	}
	this->_errorMutex.lock();
	if (this->_errorMsg) {
		auto errorMsg = this->_errorMsg;
		SokuLib::Vector2i size{0, 0};

		this->_errorMsg = nullptr;
		this->_errorMutex.unlock();
		if (*errorMsg)
			this->_error.texture.createFromText(("Refresh error: " + std::string(errorMsg)).c_str(), lobbyData->getFont(12), {540, 20}, &size);
		else
			this->_error.texture.destroy();
		free(errorMsg);
		this->_error.setSize(size.to<unsigned>());
		this->_error.rect.width = size.x;
		this->_error.rect.height = size.y;
		this->_error.tint = SokuLib::Color::Red;
		this->_error.setPosition(modifyPos(320, 480) + SokuLib::Vector2i{-size.x / 2, -16});
	} else
		this->_errorMutex.unlock();
	if (this->_parent->choice > 0) {
		if (
			this->_parent->subchoice == 5 || //Already Playing
			this->_parent->subchoice == 10   //Connect Failed
		) {
			*(*(char **)0x89a390 + 20) = false;
			this->_parent->choice = 0;
			this->_parent->subchoice = 0;
			playSound(0x29);
		}
	}
	for (auto &elem : this->_background)
		elem->update();
	for (auto &elem : this->_topOverlay)
		elem->update();
	for (auto &elem : this->_botOverlay)
		elem->update();
	if (this->_overlayTimer <= 5)
		return true;
	for (auto &elem : this->_foreground)
		elem->update();
	if (std::abs(SokuLib::inputMgrs.input.horizontalAxis) == 1) {
		this->_spectator = !this->_spectator;
		this->_hostSelect = 0;
		playSound(0x27);
		return true;
	}
	if (!this->_selected) {
		if (SokuLib::inputMgrs.input.b == 1)
			return false;
		if (SokuLib::inputMgrs.input.a == 1) {
			if (this->_selection)
				return false;
			else {
				this->_selected = true;
				playSound(0x28);
				return true;
			}
		}
		if (std::abs(SokuLib::inputMgrs.input.verticalAxis) == 1) {
			this->_selection = !this->_selection;
			playSound(0x27);
			return true;
		}
	} else if (!this->_parent->choice) {
		if (SokuLib::inputMgrs.input.b == 1) {
			this->_selected = false;
			playSound(0x29);
			return true;
		}
		if (this->_spectator ? this->_playEntries.empty() : this->_hostEntries.empty())
			return true;
		if (SokuLib::inputMgrs.input.a == 1) {
			this->_parent->joinHost(
				(this->_spectator ? this->_playEntries[this->_hostSelect]->ip : this->_hostEntries[this->_hostSelect]->ip).c_str(),
				this->_spectator ? this->_playEntries[this->_hostSelect]->port : this->_hostEntries[this->_hostSelect]->port,
				this->_spectator
			);
			playSound(0x28);
			return true;
		}

		auto axis = std::abs(SokuLib::inputMgrs.input.verticalAxis);

		if (axis == 1 || (axis >= 36 && axis % 6 == 0)) {
			if (SokuLib::inputMgrs.input.verticalAxis > 0)
				this->_hostSelect = (this->_hostSelect + 1) % (this->_spectator ? this->_playEntries.size() : this->_hostEntries.size());
			else if (this->_hostSelect == 0)
				this->_hostSelect = (this->_spectator ? this->_playEntries.size() : this->_hostEntries.size()) - 1;
			else
				this->_hostSelect--;
			playSound(0x27);
			return true;
		}
	}
	return true;
}

void SmallHostlist::render()
{
	for (auto &elem : this->_background)
		elem->render();
	for (auto &elem : this->_topOverlay)
		elem->render();
	for (auto &elem : this->_botOverlay)
		elem->render();
	if (this->_overlayTimer <= 5)
		return;
	if (!this->_selected)
		this->_displaySokuCursor({62, this->_selection ? 364 : 160}, {160, 16});
	for (auto &elem : this->_foreground)
		elem->render();
	this->_entriesMutex.lock();
	(&this->_hosting)[this->_spectator].draw();
	if (!this->_spectator)
		for (unsigned i = 0; i < this->_hostEntries.size(); i++) {
			auto &entry = *this->_hostEntries[i];
			auto flag = lobbyData->flags.find(entry.country);

			if (this->_hostSelect == i && this->_selected)
				this->_displaySokuCursor({262, static_cast<int>(118 + i * 16 / this->_ratio)}, {375, static_cast<unsigned>(16 / this->_ratio)});
			if (flag == lobbyData->flags.end())
				flag = lobbyData->flags.find("default");
			flag->second->setPosition(modifyPos(266, 118) + SokuLib::Vector2i{0, static_cast<int>(i * 16)});
			flag->second->setSize({16, 16});
			flag->second->draw();

			if (!entry.name.texture.hasTexture()) {
				SokuLib::Vector2i size;

				entry.name.texture.createFromText(entry.nameStr.c_str(), lobbyData->getFont(12), {400, 20}, &size);
				entry.name.setSize(size.to<unsigned>());
				entry.name.rect.width = size.x;
				entry.name.rect.height = size.y;

				entry.msg.texture.createFromText(entry.msgStr.c_str(), lobbyData->getFont(12), {400, 20}, &size);
				entry.msg.setSize(size.to<unsigned>());
				entry.msg.rect.width = size.x;
				entry.msg.rect.height = size.y;
			}
			entry.name.setPosition(modifyPos(262, 120) + SokuLib::Vector2i{16, static_cast<int>(i * 16)});
			entry.name.draw();
			entry.msg.setPosition(modifyPos(402, 120) + SokuLib::Vector2i{16, static_cast<int>(i * 16)});
			entry.msg.draw();
		}
	if (this->_spectator)
		for (unsigned i = 0; i < this->_playEntries.size(); i++) {
			auto &entry = *this->_playEntries[i];
			auto flag1 = lobbyData->flags.find(entry.country1);
			auto flag2 = lobbyData->flags.find(entry.country2);

			if (this->_hostSelect == i && this->_selected)
				this->_displaySokuCursor({266, static_cast<int>(118 + i * 16 / this->_ratio)}, {375, static_cast<unsigned>(16 / this->_ratio)});
			if (!entry.names.texture.hasTexture()) {
				SokuLib::Vector2i size;

				entry.names.texture.createFromText(entry.namesStr.c_str(), lobbyData->getFont(12), {400, 20}, &size);
				entry.names.setSize(size.to<unsigned>());
				entry.names.rect.width = size.x;
				entry.names.rect.height = size.y;
			}
			entry.names.setPosition(modifyPos(430, 120) + SokuLib::Vector2i{ - entry.names.rect.width / 2, static_cast<int>(i * 16)});
			entry.names.draw();

			if (flag1 == lobbyData->flags.end())
				flag1 = lobbyData->flags.find("default");
			flag1->second->setPosition(entry.names.getPosition() - SokuLib::Vector2i{18, 0});
			flag1->second->setSize({16, 16});
			flag1->second->draw();
			if (flag2 == lobbyData->flags.end())
				flag2 = lobbyData->flags.find("default");
			flag2->second->setPosition(entry.names.getPosition() + SokuLib::Vector2i{2 + entry.names.rect.width, 0});
			flag2->second->setSize({16, 16});
			flag2->second->draw();

			if (!entry.p1chr.empty() && !entry.p2chr.empty()) {
				auto chr1 = lobbyData->emotesByName.find(entry.p1chr + "1");
				auto chr2 = lobbyData->emotesByName.find(entry.p2chr + "1");
				LobbyData::Emote *p1;
				LobbyData::Emote *p2;

				if (chr1 == lobbyData->emotesByName.end()) {
					printf("Unknown emote %s1", entry.p1chr.c_str());
					p1 = &lobbyData->emotes[0];
				} else
					p1 = chr1->second;
				if (chr2 == lobbyData->emotesByName.end()) {
					printf("Unknown emote %s1", entry.p2chr.c_str());
					p2 = &lobbyData->emotes[0];
				} else
					p2 = chr2->second;
				p1->sprite.setSize({16, 16});
				p1->sprite.setPosition(entry.names.getPosition() - SokuLib::Vector2i{36, 0});
				p1->sprite.draw();
				p2->sprite.setSize({16, 16});
				p2->sprite.setPosition(entry.names.getPosition() + SokuLib::Vector2i{20 + entry.names.rect.width, 0});
				p2->sprite.draw();
			}
		}
	this->_entriesMutex.unlock();
	this->_error.draw();
}

static std::string limitStr(const std::string &str, unsigned limit)
{
	if (str.size() <= limit)
		return str;
	return str.substr(0, limit - 3) + "...";
}

void SmallHostlist::_refreshHostlist()
{
	bool locked = false;

	try {
		nlohmann::json val = nlohmann::json::parse(lobbyData->httpRequest("https://konni.delthas.fr/games"));
		bool newHost = false;

		locked = true;
		this->_entriesMutex.lock();
		for (auto &a : this->_hostEntries)
			a->deleted = true;
		for (auto &a : this->_playEntries)
			a->deleted = true;

		for (auto &j : val) {
			if (j["started"]) {
				if (!j["spectatable"])
					continue;

				auto entry = new PlayEntry();

				entry->namesStr = limitStr(j["host_name"], 10) + "|" + limitStr(j["client_name"], 10);
				entry->p1chr = j["host_character"];
				entry->p2chr = j["client_character"];
				entry->country1 = j["host_country"];
				entry->country2 = j["client_country"];
				entry->ip = j["ip"];
				entry->port = std::stoul(entry->ip.substr(entry->ip.find_last_of(':') + 1));
				entry->ip = entry->ip.substr(0, entry->ip.find_last_of(':'));

				auto it = std::find_if(this->_playEntries.begin(), this->_playEntries.end(), [entry](std::unique_ptr<PlayEntry> &a){
					return a->ip == entry->ip && a->port == entry->port;
				});

				if (it == this->_playEntries.end())
					this->_playEntries.emplace_back(entry);
				else
					it->reset(entry);
			} else {
				auto entry = new HostEntry();

				entry->nameStr = limitStr(j["host_name"], 16);
				entry->msgStr = limitStr(j["message"], 16);
				entry->country = j["host_country"];
				entry->ap = j["autopunch"];
				entry->ranked = j["ranked"];
				entry->ip = j["ip"];
				entry->port = std::stoul(entry->ip.substr(entry->ip.find_last_of(':') + 1));
				entry->ip = entry->ip.substr(0, entry->ip.find_last_of(':'));

				auto it = std::find_if(this->_hostEntries.begin(), this->_hostEntries.end(), [entry](std::unique_ptr<HostEntry> &a){
					return a->ip == entry->ip && a->port == entry->port;
				});

				if (it == this->_hostEntries.end()) {
					this->_hostEntries.emplace_back(entry);
					newHost = true;
				} else
					it->reset(entry);
			}
		}

		if (!this->_spectator && !this->_hostEntries.empty())
			for (unsigned i = 0; i <= this->_hostSelect; i++)
				this->_hostSelect -= this->_hostEntries[i]->deleted;
		if (this->_spectator && !this->_playEntries.empty())
			for (unsigned i = 0; i <= this->_hostSelect; i++)
				this->_hostSelect -= this->_playEntries[i]->deleted;

		this->_hostEntries.erase(std::remove_if(this->_hostEntries.begin(), this->_hostEntries.end(), [](const std::unique_ptr<HostEntry> &a){
			return a->deleted;
		}), this->_hostEntries.end());
		this->_playEntries.erase(std::remove_if(this->_playEntries.begin(), this->_playEntries.end(), [](const std::unique_ptr<PlayEntry> &a){
			return a->deleted;
		}), this->_playEntries.end());
		this->_entriesMutex.unlock();
		locked = false;
		if (newHost)
			playSound(49);
		this->_errorMutex.lock();
		if (this->_errorMsg)
			free(this->_errorMsg);
		this->_errorMsg = strdup("");
		this->_errorMutex.unlock();
	} catch (std::exception &e) {
		if (locked)
			this->_entriesMutex.unlock();
		this->_errorMutex.lock();
		if (this->_errorMsg)
			free(this->_errorMsg);
		this->_errorMsg = strdup(e.what());
		this->_errorMutex.unlock();
		printf("Failed to refresh hostlist: %s\n", e.what());
	}
}

SmallHostlist::Image::Image(SokuLib::DrawUtils::Sprite &sprite, SokuLib::Vector2i pos) :
	_sprite(sprite),
	_pos(pos)
{
}

void SmallHostlist::Image::update()
{
}

void SmallHostlist::Image::render()
{
	this->_sprite.setPosition(this->_pos + this->translate);
	this->_sprite.draw();
}

SmallHostlist::ScrollingImage::ScrollingImage(SokuLib::DrawUtils::Sprite &sprite, SokuLib::Vector2i startPos, SokuLib::Vector2i endPos, unsigned int animationDuration) :
	_sprite(sprite),
	_startPos(startPos),
	_endPos(endPos),
	_animationDuration(animationDuration)
{
}

void SmallHostlist::ScrollingImage::update()
{
	if (this->_animationDuration <= this->_animationCtr)
		return;
	this->_animationCtr++;
}

void SmallHostlist::ScrollingImage::render()
{
	this->_sprite.setPosition(this->_startPos + (this->_endPos - this->_startPos) * std::pow((float)this->_animationCtr / this->_animationDuration, 2) + this->translate);
	this->_sprite.draw();
}

SmallHostlist::RotatingImage::RotatingImage(SokuLib::DrawUtils::Sprite &sprite, SokuLib::Vector2i pos, float anglePerFrame) :
	_sprite(sprite),
	_pos(pos),
	_anglePerFrame(anglePerFrame)
{

}

void SmallHostlist::RotatingImage::update()
{
	this->_rotation = std::fmod(this->_rotation + this->_anglePerFrame, 2 * M_PI);
}

void SmallHostlist::RotatingImage::render()
{
	this->_sprite.setPosition(this->_pos + this->translate);
	this->_sprite.setRotation(this->_rotation);
	this->_sprite.draw();
}
