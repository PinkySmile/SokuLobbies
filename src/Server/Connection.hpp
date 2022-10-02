//
// Created by PinkySmile on 01/10/2022.
//

#ifndef SOKULOBBIES_CONNECTION_HPP
#define SOKULOBBIES_CONNECTION_HPP


#include <Packet.hpp>
#include <SFML/Network.hpp>
#include <thread>
#include <functional>

class Connection {
public:
	struct LobbyInfo {
		std::string name;
		uint8_t maxPlayers;
		uint8_t currentPlayers;
	};
	struct Room {
		std::string ip;
		unsigned short port;
	};

private:
	bool _connected = true;
	bool _init = false;
	uint32_t _id = 0;
	char _uniqueId[16];
	std::string _name;
	std::string _realName;
	std::unique_ptr<sf::TcpSocket> _socket;
	Lobbies::LobbySettings _settings;
	Lobbies::PlayerCustomization _player;
	std::thread _netThread;
	uint8_t _dir = 0;
	sf::Vector2<uint32_t> _pos = {0, 0};
	uint8_t _battleStatus = 0;
	uint8_t _machineId = 0;
	sf::Clock _timeoutClock;
	Room _room;

	void _netLoop();
	void _handlePacket(const Lobbies::Packet &packet, size_t size);
	void _handlePacket(const Lobbies::PacketHello &packet, size_t size);
	void _handlePacket(const Lobbies::PacketOlleh &packet, size_t size);
	void _handlePacket(const Lobbies::PacketPlayerJoin &packet, size_t size);
	void _handlePacket(const Lobbies::PacketPlayerLeave &packet, size_t size);
	void _handlePacket(const Lobbies::PacketKicked &packet, size_t size);
	void _handlePacket(const Lobbies::PacketMove &packet, size_t size);
	void _handlePacket(const Lobbies::PacketPosition &packet, size_t size);
	void _handlePacket(const Lobbies::PacketGameRequest &packet, size_t size);
	void _handlePacket(const Lobbies::PacketGameStart &packet, size_t size);
	void _handlePacket(const Lobbies::PacketPing &packet, size_t size);
	void _handlePacket(const Lobbies::PacketPong &packet, size_t size);
	void _handlePacket(const Lobbies::PacketSettingsUpdate &packet, size_t size);
	void _handlePacket(const Lobbies::PacketArcadeEngage &packet, size_t size);
	void _handlePacket(const Lobbies::PacketArcadeLeave &packet, size_t size);
	void _handlePacket(const Lobbies::PacketMessage &packet, size_t size);
	void _handlePacket(const Lobbies::PacketImportantMessage &packet, size_t size);

public:
	std::function<LobbyInfo ()> onPing;
	std::function<void (uint8_t dir)> onMove;
	std::function<void (uint32_t x, uint32_t y)> onPosition;
	std::function<void (const std::string &reason)> onDisconnect;
	std::function<bool (const Lobbies::PacketHello &hello, std::string ip, std::string &name)> onJoin;
	std::function<void (uint8_t channel, const std::string &msg)> onMessage;
	std::function<void (const Lobbies::PacketSettingsUpdate &settings)> onSettingsUpdate;
	std::function<void (const Room &port)> onGameStart;
	std::function<void ()> onGameRequest;
	std::function<void ()> onArcadeLeave;

	Connection(std::unique_ptr<sf::TcpSocket> &socket);
	~Connection();
	void kick(const std::string &msg);
	void startThread();
	void setId(uint32_t id);
	void send(const void *packet, size_t size);
	uint32_t getId() const;
	bool isInit() const;
	const char *getUniqueId() const;
	std::string getName() const;
	std::string getRealName() const;
	sf::Vector2<uint32_t> getPos() const;
	uint8_t getDir() const;
	uint8_t getBattleStatus() const;
	void setPlaying();
	void setNotPlaying();
	uint8_t getActiveMachine() const;
	const Room &getRoomInfo() const;
	Lobbies::LobbySettings getSettings() const;
	Lobbies::PlayerCustomization getPlayer() const;
	bool isConnected() const;
};


#endif //SOKULOBBIES_CONNECTION_HPP
