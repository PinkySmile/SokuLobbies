//
// Created by PinkySmile on 01/10/2022.
//

#include <cstring>
#include "Packet.hpp"

namespace Lobbies
{
	PacketHello::PacketHello(char uniqueId[16], const std::string &name, const PlayerCustomization &custom, const LobbySettings &settings) :
		opcode(OPCODE_HELLO),
		custom(custom),
		settings(settings)
	{
		memcpy(this->uniqueId, uniqueId, sizeof(this->uniqueId));
		strncpy(this->name, name.c_str(), sizeof(this->name));
	}

	std::string PacketHello::toString() const
	{
		return "Packet HELLO: Player '" + std::string(this->name, strnlen(this->name, sizeof(this->name))) + "' "
		       "modVersion: " + std::to_string(this->modVersion) + " "
		       "title: " + std::to_string(this->custom.title) + " "
		       "avatar: " + std::to_string(this->custom.avatar) + " "
		       "head: " + std::to_string(this->custom.head) + " "
		       "body: " + std::to_string(this->custom.body) + " "
		       "back: " + std::to_string(this->custom.back) + " "
		       "env: " + std::to_string(this->custom.env) + " "
		       "feet: " + std::to_string(this->custom.feet) + " "
		       "hostPref: " + std::to_string(this->settings.hostPref);
	}

	PacketOlleh::PacketOlleh(const std::string &realName, uint32_t id) :
		opcode(OPCODE_OLLEH),
		id(id)
	{
		strncpy(this->realName, realName.c_str(), sizeof(this->realName));
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
		       "title: " + std::to_string(this->custom.title) + " "
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

	PacketPosition::PacketPosition(uint32_t id, uint32_t x, uint32_t y) :
		opcode(OPCODE_POSITION),
		id(id),
		x(x),
		y(y)
	{
	}

	std::string PacketPosition::toString() const
	{
		return "Packet POSITION: Player id " + std::to_string(this->id) + " x: " + std::to_string(this->x)+ " y: " + std::to_string(this->y);
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

	PacketGameStart::PacketGameStart(const std::string &ip, uint16_t port, bool spectator) :
		opcode(OPCODE_GAME_START),
		spectator(spectator),
		port(port)
	{
		strncpy(this->ip, ip.c_str(), sizeof(this->ip));
	}

	std::string PacketGameStart::toString() const
	{
		return "Packet GAME_START: Connect address " + std::string(this->ip, strnlen(this->ip, sizeof(this->ip))) + ":" + std::to_string(this->port) +
		       (this->spectator ? " as spectator" : " as player");
	}

	PacketPing::PacketPing() :
		opcode(OPCODE_PING)
	{
	}

	std::string PacketPing::toString() const
	{
		return "Packet PING";
	}

	PacketPong::PacketPong(const std::string &roomName, uint8_t maxPlayers, uint8_t currentPlayers) :
		opcode(OPCODE_PONG),
		maxPlayers(maxPlayers),
		currentPlayers(currentPlayers)
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
		       "title: " + std::to_string(this->custom.title) + " "
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
		return "Packet ARCADE_ENGAGE: id" + std::to_string(this->id);
	}

	PacketArcadeLeave::PacketArcadeLeave(uint32_t id) :
		opcode(OPCODE_ARCADE_LEAVE),
		id(id)
	{
	}

	std::string PacketArcadeLeave::toString() const
	{
		return "Packet ARCADE_LEAVE: id" + std::to_string(this->id);
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
		default:
			return "Packet INVALID_OPCODE " + std::to_string(this->opcode);
		}
	}

	Packet::Packet() :
		opcode(OPCODE_INVALID)
	{
	}
}