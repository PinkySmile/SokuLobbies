//
// Created by PinkySmile on 01/10/2022.
//

#ifndef _LOBBYNOLOG
#include <iostream>
#include <mutex>
std::mutex logMutex;
#endif
#include <cstring>
#include <Exceptions.hpp>
#include "Connection.hpp"
#include "getPublicIp.hpp"


void Connection::_netLoop()
{
	char buffer[sizeof(Lobbies::Packet)];
	size_t recvSize;

	while (true) {
		try {
			recvSize = this->_socket.read(buffer, sizeof(buffer));
		} catch (std::exception &e) {
			if (!this->_socket.isOpen())
				return;
		#ifndef _LOBBYNOLOG
			logMutex.lock();
			std::cerr << e.what() << std::endl;
			logMutex.unlock();
		#endif
			this->_init = false;
			this->_connected = false;
			this->onError(e.what());
			return;
		}

		//if (status == sf::Socket::NotReady) {
		//	if (this->_timeoutClock.getElapsedTime().asSeconds() >= 5)
		//		return this->kick("Timed out");
		//	std::this_thread::sleep_for(std::chrono::milliseconds(5));
		//	continue;
		//}
		if (!this->_connected)
			return;
		if (recvSize == 0) {
			this->_init = false;
			this->_connected = false;
			this->onError("Connection closed");
			return;
		}

		size_t total = recvSize;

		do
			this->_handlePacket(*reinterpret_cast<Lobbies::Packet *>(&buffer[total - recvSize]), recvSize);
		while (recvSize != 0 && this->_connected);
	}
}

Connection::Connection(const std::string &host, unsigned short port, const Player &initParams) :
	_initParams(initParams)
{
	this->_socket.connect(host, port);
}

Connection::~Connection()
{
	this->_socket.disconnect();
	if (this->_netThread.joinable())
		this->_netThread.join();
	if (this->_posThread.joinable())
		this->_posThread.join();
}

void Connection::error(const std::string &msg)
{
	this->onError(msg);
	this->_init = false;
	this->_connected = false;
	this->_socket.disconnect();
}

void Connection::startThread()
{
	this->_netThread = std::thread{&Connection::_netLoop, this};
}

void Connection::send(const void *packet, size_t size)
{
#ifndef _LOBBYNOLOG
	logMutex.lock();
	std::cout << "[>" << inet_ntoa(this->_socket.getRemote().sin_addr) << ":" << this->_socket.getRemote().sin_port;
	std::cout << "] " << size << " bytes: " << reinterpret_cast<const Lobbies::Packet *>(packet)->toString() << std::endl;
	logMutex.unlock();
#endif
	try {
		this->_socket.send(packet, size);
	} catch (std::exception &e) {
	#ifndef _LOBBYNOLOG
		logMutex.lock();
		std::cerr << e.what() << std::endl;
		logMutex.unlock();
	#endif
	}
}

bool Connection::isInit() const
{
	return this->_init;
}

bool Connection::isConnected() const
{
	return this->_connected;
}

void Connection::_handlePacket(const Lobbies::Packet &packet, size_t &size)
{
#ifndef _LOBBYNOLOG
	logMutex.lock();
	std::cout << "[<" << inet_ntoa(this->_socket.getRemote().sin_addr) << ":" << this->_socket.getRemote().sin_port;
	std::cout << "] " << size << " bytes: " << packet.toString() << std::endl;
	logMutex.unlock();
#endif
	switch (packet.opcode) {
	case Lobbies::OPCODE_HELLO:
		return this->_handlePacket(packet.hello, size);
	case Lobbies::OPCODE_OLLEH:
		return this->_handlePacket(packet.olleh, size);
	case Lobbies::OPCODE_PLAYER_JOIN:
		return this->_handlePacket(packet.playerJoin, size);
	case Lobbies::OPCODE_PLAYER_LEAVE:
		return this->_handlePacket(packet.playerLeave, size);
	case Lobbies::OPCODE_KICKED:
		return this->_handlePacket(packet.kicked, size);
	case Lobbies::OPCODE_MOVE:
		return this->_handlePacket(packet.move, size);
	case Lobbies::OPCODE_POSITION:
		return this->_handlePacket(packet.position, size);
	case Lobbies::OPCODE_GAME_REQUEST:
		return this->_handlePacket(packet.gameRequest, size);
	case Lobbies::OPCODE_GAME_START:
		return this->_handlePacket(packet.gameStart, size);
	case Lobbies::OPCODE_PING:
		return this->_handlePacket(packet.ping, size);
	case Lobbies::OPCODE_PONG:
		return this->_handlePacket(packet.pong, size);
	case Lobbies::OPCODE_SETTINGS_UPDATE:
		return this->_handlePacket(packet.settingsUpdate, size);
	case Lobbies::OPCODE_ARCADE_ENGAGE:
		return this->_handlePacket(packet.arcadeEngage, size);
	case Lobbies::OPCODE_ARCADE_LEAVE:
		return this->_handlePacket(packet.arcadeLeave, size);
	case Lobbies::OPCODE_MESSAGE:
		return this->_handlePacket(packet.message, size);
	case Lobbies::OPCODE_IMPORTANT_MESSAGE:
		return this->_handlePacket(packet.importantMsg, size);
	default:
		return this->error("Protocol error: Invalid opcode " + std::to_string(packet.opcode));
	}
}

void Connection::_handlePacket(const Lobbies::PacketHello &, size_t &)
{
	this->error("Protocol error: OPCODE_HELLO unexpected");
}

void Connection::_handlePacket(const Lobbies::PacketOlleh &packet, size_t &size)
{
	if (size < sizeof(packet))
		return this->error("Protocol error: Invalid packet size for opcode OPCODE_OLLEH expected " + std::to_string(sizeof(packet)) + " but got " + std::to_string(size));
	size -= sizeof(packet);

	Player player = this->_initParams;

	player.id = packet.id;
	player.name = std::string(packet.realName, strnlen(packet.realName, sizeof(packet.realName)));
	this->_playerMutex.lock();
	this->_players[packet.id] = player;
	this->meMutex.lock();
	this->_me = &this->_players[packet.id];
	this->_playerMutex.unlock();
	this->meMutex.unlock();
	this->_init = true;
	this->_posThread = std::thread(&Connection::_posLoop, this);
	this->onConnect(packet);
}

void Connection::_handlePacket(const Lobbies::PacketPlayerJoin &packet, size_t &size)
{
	if (!this->_init)
		return this->error("Protocol error: Invalid handshake");
	if (size < sizeof(packet))
		return this->error("Protocol error: Invalid packet size for opcode OPCODE_PLAYER_JOIN expected " + std::to_string(sizeof(packet)) + " but got " + std::to_string(size));
	size -= sizeof(packet);

	Player player;

	player.id = packet.id;
	player.name = std::string(packet.name, strnlen(packet.name, sizeof(packet.name)));
	player.player = packet.custom;
	this->_playerMutex.lock();
	this->_players[packet.id] = player;
	this->_playerMutex.unlock();
	this->onPlayerJoin(player);
}

void Connection::_handlePacket(const Lobbies::PacketPlayerLeave &packet, size_t &size)
{
	if (!this->_init)
		return this->error("Protocol error: Invalid handshake");
	if (size < sizeof(packet))
		return this->error("Protocol error: Invalid packet size for opcode OPCODE_PLAYER_LEAVE expected " + std::to_string(sizeof(packet)) + " but got " + std::to_string(size));
	size -= sizeof(packet);
	if (packet.id == this->_me->id)
		return this->error("Protocol error: Server sent OPCODE_PLAYER_LEAVE with self id");
	this->_playerMutex.lock();
	this->_players.erase(this->_players.find(packet.id));
	this->_playerMutex.unlock();
}

void Connection::_handlePacket(const Lobbies::PacketKicked &packet, size_t &size)
{
	if (size < sizeof(packet))
		return this->error("Protocol error: Invalid packet size for opcode OPCODE_KICKED expected " + std::to_string(sizeof(packet)) + " but got " + std::to_string(size));
	size -= sizeof(packet);
	this->onImpMsg("Kicked: " + std::string(packet.message, strnlen(packet.message, sizeof(packet.message))));
	this->_init = false;
	this->_connected = false;
	this->_socket.disconnect();
}

void Connection::_handlePacket(const Lobbies::PacketMove &packet, size_t &size)
{
	if (!this->_init)
		return this->error("Protocol error: Invalid handshake");
	if (size < sizeof(packet))
		return this->error("Protocol error: Invalid packet size for opcode OPCODE_MOVE expected " + std::to_string(sizeof(packet)) + " but got " + std::to_string(size));
	size -= sizeof(packet);
	this->_playerMutex.lock();
	this->_players[packet.id].dir = packet.dir;
	this->_playerMutex.unlock();
}

void Connection::_handlePacket(const Lobbies::PacketPosition &packet, size_t &size)
{
	if (!this->_init)
		return this->error("Protocol error: Invalid handshake");
	if (size < sizeof(packet))
		return this->error("Protocol error: Invalid packet size for opcode OPCODE_POSITION expected " + std::to_string(sizeof(packet)) + " but got " + std::to_string(size));
	size -= sizeof(packet);
	this->_playerMutex.lock();
	this->_players[packet.id].pos = {packet.x, packet.y};
	this->_playerMutex.unlock();
}

void Connection::_handlePacket(const Lobbies::PacketGameRequest &packet, size_t &size)
{
	if (!this->_init)
		return this->error("Protocol error: Invalid handshake");
	if (size < sizeof(packet))
		return this->error("Protocol error: Invalid packet size for opcode OPCODE_GAME_REQUEST expected " + std::to_string(sizeof(packet)) + " but got " + std::to_string(size));
	size -= sizeof(packet);

	Lobbies::PacketGameStart game{getMyIp(), this->onHostRequest(), false};

	this->send(&game, sizeof(game));
	this->_me->battleStatus = 2;
}

void Connection::_handlePacket(const Lobbies::PacketGameStart &packet, size_t &size)
{
	if (!this->_init)
		return this->error("Protocol error: Invalid handshake");
	if (size < sizeof(packet))
		return this->error("Protocol error: Invalid packet size for opcode OPCODE_GAME_START expected " + std::to_string(sizeof(packet)) + " but got " + std::to_string(size));
	size -= sizeof(packet);
	this->onConnectRequest(packet.ip, packet.port, packet.spectator);
	this->_me->battleStatus = 2;
}

void Connection::_handlePacket(const Lobbies::PacketPing &, size_t &)
{
	this->error("Protocol error: OPCODE_PING unexpected");
}

void Connection::_handlePacket(const Lobbies::PacketPong &packet, size_t &size)
{
	if (size < sizeof(packet))
		return this->error("Protocol error: Invalid packet size for opcode OPCODE_PONG expected " + std::to_string(sizeof(packet)) + " but got " + std::to_string(size));
	size -= sizeof(packet);

	this->_info.name = std::string(packet.name, strnlen(packet.name, sizeof(packet.name)));
	this->_info.maxPlayers = packet.maxPlayers;
	this->_info.currentPlayers = packet.currentPlayers;
}

void Connection::_handlePacket(const Lobbies::PacketSettingsUpdate &packet, size_t &size)
{
	if (!this->_init)
		return this->error("Protocol error: Invalid handshake");
	if (size < sizeof(packet))
		return this->error("Protocol error: Invalid packet size for opcode OPCODE_SETTINGS_UPDATE expected " + std::to_string(sizeof(packet)) + " but got " + std::to_string(size));
	size -= sizeof(packet);
	this->_playerMutex.lock();
	this->_players[packet.id].player = packet.custom;
	this->_playerMutex.unlock();
}

void Connection::_handlePacket(const Lobbies::PacketArcadeEngage &packet, size_t &size)
{
	if (!this->_init)
		return this->error("Protocol error: Invalid handshake");
	if (size < sizeof(packet))
		return this->error("Protocol error: Invalid packet size for opcode OPCODE_ARCADE_ENGAGE expected " + std::to_string(sizeof(packet)) + " but got " + std::to_string(size));
	size -= sizeof(packet);
	this->_playerMutex.lock();
	this->_players[packet.id].battleStatus = 1;
	this->_playerMutex.unlock();
}

void Connection::_handlePacket(const Lobbies::PacketArcadeLeave &packet, size_t &size)
{
	if (!this->_init)
		return this->error("Protocol error: Invalid handshake");
	if (size < sizeof(packet))
		return this->error("Protocol error: Invalid packet size for opcode OPCODE_ARCADE_LEAVE expected " + std::to_string(sizeof(packet)) + " but got " + std::to_string(size));
	size -= sizeof(packet);
	this->_playerMutex.lock();
	this->_players[packet.id].battleStatus = 0;
	this->_playerMutex.unlock();
}

void Connection::_handlePacket(const Lobbies::PacketMessage &packet, size_t &size)
{
	if (size < sizeof(packet))
		return this->error("Protocol error: Invalid packet size for opcode OPCODE_MESSAGE expected " + std::to_string(sizeof(packet)) + " but got " + std::to_string(size));
	size -= sizeof(packet);
	this->onMsg(packet.channelId, std::string(packet.message, strnlen(packet.message, sizeof(packet.message))));
}

void Connection::_handlePacket(const Lobbies::PacketImportantMessage &packet, size_t &size)
{
	if (!this->_init)
		return this->error("Protocol error: Invalid handshake");
	if (size < sizeof(packet))
		return this->error("Protocol error: Invalid packet size for opcode OPCODE_IMPORTANT_MESSAGE expected " + std::to_string(sizeof(packet)) + " but got " + std::to_string(size));
	size -= sizeof(packet);
	this->onImpMsg(std::string(packet.message, strnlen(packet.message, sizeof(packet.message))));
}

void Connection::connect()
{
	if (this->_init)
		return;

	char uniqueId[16] = {0};
	Lobbies::PacketHello hello{uniqueId, this->_initParams.name, this->_initParams.player, this->_initParams.settings};

	this->send(&hello, sizeof(hello));
}

void Connection::disconnect()
{
	if (!this->_init)
		return;

	Lobbies::PacketPlayerLeave leave{0};

	this->send(&leave, sizeof(leave));
	this->meMutex.lock();
	this->_init = false;
	this->_me = nullptr;
	this->_players.clear();
	this->meMutex.unlock();
	if (this->_posThread.joinable())
		this->_posThread.join();
}

const Connection::LobbyInfo &Connection::getLobbyInfo() const
{
	return this->_info;
}

Player *Connection::getMe()
{
	return this->_me;
}

const Player *Connection::getMe() const
{
	return this->_me;
}

std::vector<Player> Connection::getPlayers() const
{
	std::vector<Player> players;

	this->_playerMutex.lock();
	for (auto &p : this->_players)
		players.push_back(p.second);
	this->_playerMutex.unlock();
	return players;
}

std::vector<std::string> Connection::getMessages() const
{
	this->_messagesMutex.lock();

	auto result = this->_messages;

	this->_messagesMutex.unlock();
	return result;
}

void Connection::updatePlayers(const std::vector<LobbyMenu::Avatar> &avatars)
{
	this->_playerMutex.lock();
	for (auto &p : this->_players) {
		if (p.second.dir & 1)
			p.second.pos.x += PLAYER_H_SPEED;
		if (p.second.dir & 2)
			p.second.pos.x -= PLAYER_H_SPEED;
		if (p.second.dir & 4)
			p.second.pos.y += PLAYER_V_SPEED;
		if (p.second.dir & 8)
			p.second.pos.y -= PLAYER_V_SPEED;
		p.second.animationCtr++;
		if (p.second.animationCtr > avatars[p.second.player.avatar].animationsStep) {
			p.second.currentAnimation++;
			p.second.currentAnimation %= avatars[p.second.player.avatar].nbAnimations;
			p.second.animationCtr = 0;
		}
	}
	this->_playerMutex.unlock();
}

void Connection::_posLoop()
{
	puts("IN!");
	while (this->_init) {
		puts("Lock!");
		this->meMutex.lock();
		puts("Locked");

		Lobbies::PacketPosition position{0, this->_me->pos.x, this->_me->pos.y};

		this->send(&position, sizeof(position));
		this->meMutex.unlock();
		for (int i = 0; i < 10 && this->_init; i++)
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	puts("Out!");
}
