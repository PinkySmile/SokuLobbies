//
// Created by PinkySmile on 01/10/2022.
//

#ifndef SOKULOBBIES_CONNECTION_HPP
#define SOKULOBBIES_CONNECTION_HPP


#include <thread>
#include <mutex>
#include <functional>
#include <Packet.hpp>
#include <Vector2.hpp> //From SokuLib
#include "Player.hpp"
#include "Socket.hpp"

#define PLAYER_H_SPEED 2
#define PLAYER_V_SPEED 2

class Connection {
public:
	struct LobbyInfo {
		std::string name;
		uint8_t maxPlayers;
		uint8_t currentPlayers;
	};

private:
	mutable std::mutex _messagesMutex;
	mutable std::mutex _playerMutex;
	std::thread _netThread;
	std::thread _posThread;
	bool _connected = true;
	bool _init = false;
	char _uniqueId[16];
	std::string _name;
	Socket _socket;
	LobbyInfo _info;
	const Player &_initParams;
	Player *_me = nullptr;
	std::map<uint32_t, uint32_t> _machines;
	std::map<uint32_t, Player> _players;
	std::vector<std::string> _messages; //TODO: properly handle channels

	void _netLoop();
	void _posLoop();
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
	std::function<void (const std::string &ip, unsigned short port, bool spectate)> onConnectRequest;
	std::function<void (const std::string &msg)> onError;
	std::function<void (const std::string &msg)> onImpMsg;
	std::function<void (int32_t channel, const std::string &msg)> onMsg;
	std::function<unsigned short ()> onHostRequest;
	std::mutex meMutex;

	Connection(const std::string &host, unsigned short port, const Player &initParams);
	~Connection();
	void startThread();
	void error(const std::string &msg);
	void connect();
	void disconnect();
	void send(const void *packet, size_t size);
	bool isInit() const;
	bool isConnected() const;
	const LobbyInfo &getLobbyInfo() const;
	Player *getMe();
	const Player *getMe() const;
	std::vector<Player> getPlayers() const;
	std::vector<std::string> getMessages() const;
	void updatePlayers();
};


#endif //SOKULOBBIES_CONNECTION_HPP
