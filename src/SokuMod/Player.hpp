//
// Created by PinkySmile on 02/10/2022.
//

#ifndef SOKULOBBIES_PLAYER_HPP
#define SOKULOBBIES_PLAYER_HPP


#include <Packet.hpp>
#include <SokuLib.hpp>

struct Player {
	struct Room {
		std::string ip;
		unsigned short port;
	};

	uint32_t id = 0;
	std::string name;
	Lobbies::LobbySettings settings;
	Lobbies::PlayerCustomization player;
	uint8_t dir = 0;
	uint8_t animation = 0;
	SokuLib::Vector2<uint32_t> pos = {0, 0};
	Lobbies::BattleStatus battleStatus = Lobbies::BATTLE_STATUS_IDLE;
	uint8_t machineId = 0;
	uint8_t currentAnimation = 0;
	uint8_t animationCtr = 0;
	Room room;
};


#endif //SOKULOBBIES_PLAYER_HPP
