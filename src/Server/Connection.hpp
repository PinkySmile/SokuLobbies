//
// Created by PinkySmile on 01/10/2022.
//

#ifndef SOKULOBBIES_CONNECTION_HPP
#define SOKULOBBIES_CONNECTION_HPP


#include <optional>
#include <Packet.hpp>
#include <SFML/Network.hpp>
#include <thread>
#include <functional>

class Connection {
public:
	struct LobbyInfo {
		std::string name;
		uint8_t maxPlayers = 0;
		uint8_t currentPlayers = 0;
	};
	struct Room {
		std::string ip;
		unsigned short port;
		std::string ipv6;
		unsigned short port6;
	};

private:
	const char *_password;
	bool _connected = true;
	bool _init = false;
	uint32_t _id = 0;
	Lobbies::Soku2VersionInfo _soku2Infos;
	unsigned char _versionString[16];
	unsigned long long _uniqueId;
	std::string _name;
	std::string _realName;
	std::unique_ptr<sf::TcpSocket> _socket;
	Lobbies::LobbySettings _settings;
	Lobbies::PlayerCustomization _player;
	std::thread _netThread;
	uint8_t _dir = 0;
	sf::Vector2<uint32_t> _pos = {0, 0};
	bool _posChanged = true;
	Lobbies::BattleStatus _battleStatus = Lobbies::BATTLE_STATUS_IDLE;
	std::optional<uint8_t> _machineId;
	sf::Clock _timeoutClock;
	Room _room;

	void _netLoop();
	bool _handlePacket(const Lobbies::Packet &packet, size_t &size);
	bool _handlePacket(const Lobbies::PacketHello &packet, size_t &size);
	bool _handlePacket(const Lobbies::PacketOlleh &packet, size_t &size);
	bool _handlePacket(const Lobbies::PacketPlayerJoin &packet, size_t &size);
	bool _handlePacket(const Lobbies::PacketPlayerLeave &packet, size_t &size);
	bool _handlePacket(const Lobbies::PacketKicked &packet, size_t &size);
	bool _handlePacket(const Lobbies::PacketMove &packet, size_t &size);
	bool _handlePacket(const Lobbies::PacketPosition &packet, size_t &size);
	bool _handlePacket(const Lobbies::PacketGameRequest &packet, size_t &size);
	bool _handlePacket(const Lobbies::PacketGameStart &packet, size_t &size);
	bool _handlePacket(const Lobbies::PacketPing &packet, size_t &size);
	bool _handlePacket(const Lobbies::PacketPong &packet, size_t &size);
	bool _handlePacket(const Lobbies::PacketSettingsUpdate &packet, size_t &size);
	bool _handlePacket(const Lobbies::PacketArcadeEngage &packet, size_t &size);
	bool _handlePacket(const Lobbies::PacketArcadeLeave &packet, size_t &size);
	bool _handlePacket(const Lobbies::PacketMessage &packet, size_t &size);
	bool _handlePacket(const Lobbies::PacketImportantMessage &packet, size_t &size);
	bool _handlePacket(const Lobbies::PacketBattleStatusUpdate &packet, size_t &size);

public:
	std::function<LobbyInfo ()> onPing;
	std::function<void (uint8_t dir, bool changed)> onMove;
	std::function<void (uint32_t x, uint32_t y, uint8_t dir, Lobbies::BattleStatus status, bool)> onPosition;
	std::function<void (const std::string &reason)> onDisconnect;
	std::function<bool (const Lobbies::PacketHello &hello, std::string ip, std::string &name)> onJoin;
	std::function<void (uint8_t channel, const std::string &msg)> onMessage;
	std::function<void (Lobbies::BattleStatus status)> onBattleStatus;
	std::function<void (const Lobbies::PacketSettingsUpdate &settings)> onSettingsUpdate;
	std::function<void (const Room &port)> onGameStart;
	std::function<bool (uint32_t aid)> onGameRequest;
	std::function<void ()> onArcadeLeave;

	Connection(std::unique_ptr<sf::TcpSocket> &socket, const char *password);
	~Connection();
	sf::IpAddress getIp() const;
	void kick(const std::string &msg);
	void startThread();
	void setId(uint32_t id);
	void send(const void *packet, size_t size);
	uint32_t getId() const;
	bool isInit() const;
	const unsigned char *getVersionString() const;
	unsigned long long getUniqueId() const;
	std::string getName() const;
	std::string getRealName() const;
	sf::Vector2<uint32_t> getPos() const;
	uint8_t getDir() const;
	Lobbies::BattleStatus getBattleStatus() const;
	void setPlaying(bool spec);
	void setNotPlaying();
	void setActiveMachine(uint8_t id);
	std::optional<uint8_t> getActiveMachine() const;
	const Room &getRoomInfo() const;
	Lobbies::Soku2VersionInfo getSoku2Version() const;
	Lobbies::LobbySettings getSettings() const;
	Lobbies::PlayerCustomization getPlayer() const;
	bool isConnected() const;
};


#endif //SOKULOBBIES_CONNECTION_HPP
