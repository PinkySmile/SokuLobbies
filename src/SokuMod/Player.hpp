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
	SokuLib::Vector2<uint32_t> pos = {0, 0};
	uint8_t battleStatus = 0;
	uint8_t machineId = 0;
	Room room;
};


#endif //SOKULOBBIES_PLAYER_HPP
