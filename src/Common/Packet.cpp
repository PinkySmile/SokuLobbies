//
// Created by PinkySmile on 01/10/2022.
//

#include <cstdint>
#include <cstring>
#include "Packet.hpp"

#ifdef _WIN32
#include <windows.h>
#include <random>
#endif

namespace Lobbies
{
	PacketHello::PacketHello(const Soku2VersionInfo &soku2Info, unsigned char versionString[16], const std::string &name, const PlayerCustomization &custom, const LobbySettings &settings) :
		opcode(OPCODE_HELLO),
		soku2Info(soku2Info),
		custom(custom),
		settings(settings)
	{
#ifdef _WIN32
		HKEY key;

		if (!RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Touhou\\Soku", 0, NULL, 0, KEY_READ | KEY_WRITE | KEY_WOW64_64KEY, NULL, &key, NULL)) {
			DWORD size = sizeof(this->uniqueId);

			if (RegQueryValueExW(key, L"SokuReplaysID", NULL, NULL, (byte *)&this->uniqueId, &size)) {
				std::random_device rd;
				std::mt19937_64 gen(rd());
				std::uniform_int_distribution<unsigned long long> dis;

				this->uniqueId = dis(gen) >> 1; // let's only keep 63-bits, positive values to avoid any sign quirk...
				RegSetValueExW(key, L"SokuReplaysID", 0, REG_QWORD, (BYTE*)&this->uniqueId, size);
			}
			RegCloseKey(key);
		}
#endif
		memcpy(this->versionString, versionString, sizeof(this->versionString));
		strncpy(this->name, name.c_str(), sizeof(this->name));
	}

	PacketHello::PacketHello(const Soku2VersionInfo &soku2Info, unsigned char versionString[16], const std::string &name, const PlayerCustomization &custom, const LobbySettings &settings, const std::string &pwd) :
		PacketHello(soku2Info, versionString, name, custom, settings)
	{
		strncpy(this->password, pwd.c_str(), sizeof(this->password));
	}

	std::string PacketHello::toString() const
	{
		return "Packet HELLO: Player '" + std::string(this->name, strnlen(this->name, sizeof(this->name))) + "' "
		       "modVersion: " + std::to_string(this->modVersion) + " "
		       "_title: " + std::to_string(this->custom.title) + " "
		       "avatar: " + std::to_string(this->custom.avatar) + " "
		       "head: " + std::to_string(this->custom.head) + " "
		       "body: " + std::to_string(this->custom.body) + " "
		       "back: " + std::to_string(this->custom.back) + " "
		       "env: " + std::to_string(this->custom.env) + " "
		       "feet: " + std::to_string(this->custom.feet) + " "
		       "hostPref: " + std::to_string(this->settings.hostPref);
	}

	PacketOlleh::PacketOlleh(const std::string &roomName, const std::string &realName, uint32_t id) :
		opcode(OPCODE_OLLEH),
		id(id)
	{
		strncpy(this->realName, realName.c_str(), sizeof(this->realName));
		strncpy(this->name, roomName.c_str(), sizeof(this->name));
	}

	std::string PacketOlleh::toString() const
	{
		return "Packet OLLEH: Real name '" + std::string(this->realName, strnlen(this->realName, sizeof(this->realName))) + "' id " + std::to_string(this->id);
	}

	PacketPlayerJoin::PacketPlayerJoin(uint32_t id, const std::string &name, PlayerCustomization custom) :
		opcode(OPCODE_PLAYER_JOIN),
		id(id),
		custom(custom)
	{
		strncpy(this->name, name.c_str(), sizeof(this->name));
	}

	std::string PacketPlayerJoin::toString() const
	{
		return "Packet PLAYER_JOINED: Player '" + std::string(this->name, strnlen(this->name, sizeof(this->name))) + "' (id " + std::to_string(this->id) + ")"
		       "_title: " + std::to_string(this->custom.title) + " "
		       "avatar: " + std::to_string(this->custom.avatar) + " "
		       "head: " + std::to_string(this->custom.head) + " "
		       "body: " + std::to_string(this->custom.body) + " "
		       "back: " + std::to_string(this->custom.back) + " "
		       "env: " + std::to_string(this->custom.env) + " "
		       "feet: " + std::to_string(this->custom.feet);
	}

	PacketPlayerLeave::PacketPlayerLeave(uint32_t id) :
		opcode(OPCODE_PLAYER_LEAVE),
		id(id)
	{
	}

	std::string PacketPlayerLeave::toString() const
	{
		return "Packet PLAYER_LEAVE: Player id " + std::to_string(this->id);
	}

	PacketKicked::PacketKicked(const std::string &msg) :
		opcode(OPCODE_KICKED)
	{
		strncpy(this->message, msg.c_str(), sizeof(this->message));
	}

	std::string PacketKicked::toString() const
	{
		return "Packet KICKED: Player '" + std::string(this->message, strnlen(this->message, sizeof(this->message))) + "'";
	}

	PacketMove::PacketMove(uint32_t id, uint8_t dir) :
		opcode(OPCODE_MOVE),
		id(id),
		dir(dir)
	{
	}

	std::string PacketMove::toString() const
	{
		return "Packet MOVE: Player id " + std::to_string(this->id) + " dir: " + std::to_string(this->dir);
	}

	PacketPosition::PacketPosition(uint32_t id, uint32_t x, uint32_t y, uint8_t dir, BattleStatus status) :
		opcode(OPCODE_POSITION),
		id(id),
		x(x),
		y(y),
		dir(dir),
		status(status)
	{
	}

	std::string PacketPosition::toString() const
	{
		return "Packet POSITION: "
		       "Player id " + std::to_string(this->id) + " "
		       "x: " + std::to_string(this->x) + " "
		       "y: " + std::to_string(this->y) + " "
		       "dir: " + std::to_string(this->dir) + " "
		       "status: " + std::to_string(this->status);
	}

	PacketGameRequest::PacketGameRequest(uint32_t consoleId) :
		opcode(OPCODE_GAME_REQUEST),
		consoleId(consoleId)
	{
	}

	std::string PacketGameRequest::toString() const
	{
		return "Packet GAME_REQUEST: Console id " + std::to_string(this->consoleId);
	}

	PacketGameStart::PacketGameStart(const std::string &ip, uint16_t port, const std::string &ipv6, uint16_t port6, bool spectator) :
		opcode(OPCODE_GAME_START),
		spectator(spectator),
		port(port),
		port6(port6)
	{
		strncpy(this->ip, ip.c_str(), sizeof(this->ip));
		strncpy(this->ipv6, ipv6.c_str(), sizeof(this->ipv6));
	}

	std::string PacketGameStart::toString() const
	{
		return "Packet GAME_START: "
		       "Connect address v4: " + std::string(this->ip, strnlen(this->ip, sizeof(this->ip))) + ":" + std::to_string(this->port) +
		       (this->port6 ? " Connect address v6: " + std::string(this->ipv6, strnlen(this->ipv6, sizeof(this->ipv6)))  + " on port " + std::to_string(this->port6) : "") +
		       " Spectator: " + (this->spectator ? "true" : "false");
	}

	PacketPing::PacketPing() :
		opcode(OPCODE_PING)
	{
	}

	std::string PacketPing::toString() const
	{
		return "Packet PING";
	}

	PacketPong::PacketPong(const std::string &roomName, uint8_t maxPlayers, uint8_t currentPlayers, const char *password) :
		opcode(OPCODE_PONG),
		maxPlayers(maxPlayers),
		currentPlayers(currentPlayers),
		requiresPwd(password != nullptr)
	{
		strncpy(this->name, roomName.c_str(), sizeof(this->name));
	}

	std::string PacketPong::toString() const
	{
		return "Packet PONG: Room " + std::string(this->name, strnlen(this->name, sizeof(this->name))) + " with " + std::to_string(this->currentPlayers) + "/" + std::to_string(this->maxPlayers) + " players";
	}

	PacketSettingsUpdate::PacketSettingsUpdate(uint32_t id, const PlayerCustomization &custom, const LobbySettings settings) :
		opcode(OPCODE_SETTINGS_UPDATE),
		id(id),
		custom(custom),
		settings(settings)
	{
	}

	std::string PacketSettingsUpdate::toString() const
	{
		return "Packet SETTINGS_UPDATE: Player " + std::to_string(this->id) + " "
		       "_title: " + std::to_string(this->custom.title) + " "
		       "avatar: " + std::to_string(this->custom.avatar) + " "
		       "head: " + std::to_string(this->custom.head) + " "
		       "body: " + std::to_string(this->custom.body) + " "
		       "back: " + std::to_string(this->custom.back) + " "
		       "env: " + std::to_string(this->custom.env) + " "
		       "feet: " + std::to_string(this->custom.feet) + " "
		       "hostPref: " + std::to_string(this->settings.hostPref);
	}

	PacketMessage::PacketMessage(int32_t channelId, uint32_t playerId, const std::string &message) :
		opcode(OPCODE_MESSAGE),
		channelId(channelId),
		playerId(playerId)
	{
		strncpy(this->message, message.c_str(), sizeof(this->message));
	}

	std::string PacketMessage::toString() const
	{
		return "Packet MESSAGE: Channel " + std::to_string(this->channelId) + " from player " + std::to_string(this->playerId) + ": " +
		       std::string(this->message, strnlen(this->message, sizeof(this->message)));
	}

	PacketArcadeEngage::PacketArcadeEngage(uint32_t id, uint32_t machineId) :
		opcode(OPCODE_ARCADE_ENGAGE),
		id(id),
		machineId(machineId)
	{
	}

	std::string PacketArcadeEngage::toString() const
	{
		return "Packet ARCADE_ENGAGE: id " + std::to_string(this->id);
	}

	PacketArcadeLeave::PacketArcadeLeave(uint32_t id) :
		opcode(OPCODE_ARCADE_LEAVE),
		id(id)
	{
	}

	std::string PacketArcadeLeave::toString() const
	{
		return "Packet ARCADE_LEAVE: id " + std::to_string(this->id);
	}

	PacketImportantMessage::PacketImportantMessage(const std::string &msg) :
		opcode(OPCODE_IMPORTANT_MESSAGE)
	{
		strncpy(this->message, msg.c_str(), sizeof(this->message));
	}

	std::string PacketImportantMessage::toString() const
	{
		return "Packet IMPORTANT_MESSAGE: " + std::string(this->message, strnlen(this->message, sizeof(this->message)));
	}

	PacketBattleStatusUpdate::PacketBattleStatusUpdate(uint32_t playerId, BattleStatus newStatus) :
		opcode(OPCODE_BATTLE_STATUS_UPDATE),
		playerId(playerId),
		newStatus(newStatus)
	{
	}

	std::string PacketBattleStatusUpdate::toString() const
	{
		return "Packet BATTLE_STATUS_UPDATE: playerId " + std::to_string(this->playerId) + " newStatus " + std::to_string(this->newStatus);
	}

	std::string Packet::toString() const
	{
		switch (this->opcode) {
		case OPCODE_HELLO:
			return this->hello.toString();
		case OPCODE_OLLEH:
			return this->olleh.toString();
		case OPCODE_PLAYER_JOIN:
			return this->playerJoin.toString();
		case OPCODE_PLAYER_LEAVE:
			return this->playerLeave.toString();
		case OPCODE_KICKED:
			return this->kicked.toString();
		case OPCODE_MOVE:
			return this->move.toString();
		case OPCODE_POSITION:
			return this->position.toString();
		case OPCODE_GAME_REQUEST:
			return this->gameRequest.toString();
		case OPCODE_GAME_START:
			return this->gameStart.toString();
		case OPCODE_PING:
			return this->ping.toString();
		case OPCODE_PONG:
			return this->pong.toString();
		case OPCODE_SETTINGS_UPDATE:
			return this->settingsUpdate.toString();
		case OPCODE_MESSAGE:
			return this->message.toString();
		case OPCODE_ARCADE_ENGAGE:
			return this->arcadeEngage.toString();
		case OPCODE_ARCADE_LEAVE:
			return this->arcadeLeave.toString();
		case OPCODE_IMPORTANT_MESSAGE:
			return this->importantMsg.toString();
		case OPCODE_BATTLE_STATUS_UPDATE:
			return this->battleStatusUpdate.toString();
		default:
			return "Packet INVALID_OPCODE " + std::to_string(this->opcode);
		}
	}

	Packet::Packet() :
		opcode(OPCODE_INVALID)
	{
	}
}