//
// Created by PinkySmile on 01/10/2022.
//

#ifndef _LOBBYNOLOG
#include <iostream>
#include <mutex>
extern std::mutex logMutex;
#endif
#include <cstring>
#include "Connection.hpp"


void Connection::_netLoop()
{
	char buffer[sizeof(Lobbies::Packet)];
	size_t recvSize;

	this->_timeoutClock.restart();
	this->_socket->setBlocking(false);
	do {
		auto status = this->_socket->receive(buffer, sizeof(buffer), recvSize);

		if (status == sf::Socket::NotReady) {
			if (this->_timeoutClock.getElapsedTime().asSeconds() >= 5)
				return this->kick("Timed out");
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
			continue;
		}
		if (!this->_connected) {
		#ifndef _LOBBYNOLOG
			logMutex.lock();
			std::cout << this->_socket->getRemoteAddress().toString() << ":" << this->_socket->getRemotePort();
			if (this->_id)
				std::cout << " player id " << this->_id;
			std::cout << " disconnected" << std::endl;
			logMutex.unlock();
		#endif
			return;
		}
		if (status == sf::Socket::Disconnected) {
			this->onDisconnect(" has disconnected");
			this->_init = false;
			this->_connected = false;
		#ifndef _LOBBYNOLOG
			logMutex.lock();
			std::cout << this->_socket->getRemoteAddress().toString() << ":" << this->_socket->getRemotePort();
			if (this->_id)
				std::cout << " player id " << this->_id;
			std::cout << " disconnected" << std::endl;
			logMutex.unlock();
		#endif
			return;
		}
		if (status == sf::Socket::Error) {
			this->kick("Socket error");
		#ifndef _LOBBYNOLOG
			logMutex.lock();
			std::cout << this->_socket->getRemoteAddress().toString() << ":" << this->_socket->getRemotePort();
			if (this->_id)
				std::cout << " player id " << this->_id;
			std::cout << " disconnected" << std::endl;
			logMutex.unlock();
		#endif
			return;
		}

		size_t total = recvSize;

		do
			this->_handlePacket(*reinterpret_cast<Lobbies::Packet *>(&buffer[total - recvSize]), recvSize);
		while (recvSize != 0 && this->_connected);
	} while (true);
}

Connection::Connection(std::unique_ptr<sf::TcpSocket> &socket) :
	_socket(std::move(socket))
{
}

Connection::~Connection()
{
	if (this->_netThread.joinable())
		this->_netThread.join();
}

void Connection::kick(const std::string &msg)
{
	Lobbies::PacketKicked packet{msg};

	this->onDisconnect(" has been kicked: " + msg);
	this->_init = false;
	this->_connected = false;
	this->send(&packet, sizeof(packet));
	this->_socket->disconnect();
}

void Connection::startThread()
{
	this->_netThread = std::thread{&Connection::_netLoop, this};
}

void Connection::setId(uint32_t id)
{
	this->_id = id;
}

void Connection::send(const void *packet, size_t size)
{
	size_t sent;

#ifndef _LOBBYNOLOG
	logMutex.lock();
	std::cout << "[>" << this->_socket->getRemoteAddress().toString() << ":" << this->_socket->getRemotePort();
	if (this->_id)
		std::cout << " player id " << this->_id;
	std::cout << "] " << size << " bytes: " << reinterpret_cast<const Lobbies::Packet *>(packet)->toString() << std::endl;
	logMutex.unlock();
#endif
	this->_socket->send(packet, size, sent);
}

uint32_t Connection::getId() const
{
	return this->_id;
}

bool Connection::isInit() const
{
	return this->_init;
}

const char *Connection::getUniqueId() const
{
	return this->_uniqueId;
}

std::string Connection::getName() const
{
	return this->_name;
}

std::string Connection::getRealName() const
{
	return this->_realName;
}

sf::Vector2<uint32_t> Connection::getPos() const
{
	return this->_pos;
}

uint8_t Connection::getDir() const
{
	return this->_dir;
}

uint8_t Connection::getBattleStatus() const
{
	return this->_battleStatus;
}

void Connection::setPlaying()
{
	this->_battleStatus = 2;
}

uint8_t Connection::getActiveMachine() const
{
	return this->_machineId;
}

const Connection::Room &Connection::getRoomInfo() const
{
	return this->_room;
}

void Connection::setNotPlaying()
{
	this->_battleStatus = 0;
}

Lobbies::LobbySettings Connection::getSettings() const
{
	return this->_settings;
}

Lobbies::PlayerCustomization Connection::getPlayer() const
{
	return this->_player;
}

bool Connection::isConnected() const
{
	return this->_connected;
}

void Connection::_handlePacket(const Lobbies::Packet &packet, size_t &size)
{
#ifndef _LOBBYNOLOG
	logMutex.lock();
	std::cout << "[<" << this->_socket->getRemoteAddress().toString() << ":" << this->_socket->getRemotePort();
	if (this->_id)
		std::cout << " player id " << this->_id;
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
		return this->kick("Protocol error: Invalid opcode " + std::to_string(packet.opcode));
	}
}

void Connection::_handlePacket(const Lobbies::PacketHello &packet, size_t &size)
{
	if (this->_init)
		return;
	if (size < sizeof(packet))
		return this->kick("Protocol error: Invalid packet size for opcode OPCODE_HELLO expected " + std::to_string(sizeof(packet)) + " but got " + std::to_string(size));
	size -= sizeof(packet);
	if (packet.modVersion > MOD_VERSION)
		return this->kick("Outdated server!");
	if (packet.modVersion < MOD_VERSION)
		return this->kick("You are running an old version of SokuLobbies! Please update your mod and try again.");
	memcpy(this->_uniqueId, packet.uniqueId, sizeof(packet.uniqueId));
	if (!this->onJoin(
		packet,
		this->_socket->getRemoteAddress().toString() + ":" + std::to_string(this->_socket->getRemotePort()),
		this->_name
	)) {
		this->_socket->disconnect();
		this->_connected = false;
		return;
	}
	this->_init = true;
	this->_settings = packet.settings;
	this->_player = packet.custom;
	this->_realName = std::string(packet.name, strnlen(packet.name, sizeof(packet.name)));
	this->_timeoutClock.restart();
}

void Connection::_handlePacket(const Lobbies::PacketOlleh &, size_t &)
{
	this->kick("Protocol error: OPCODE_OLLEH unexpected");
}

void Connection::_handlePacket(const Lobbies::PacketPlayerJoin &, size_t &)
{
	this->kick("Protocol error: OPCODE_PLAYER_JOIN unexpected");
}

void Connection::_handlePacket(const Lobbies::PacketPlayerLeave &packet, size_t &size)
{
	if (!this->_init)
		return;
	if (size < sizeof(packet))
		return this->kick("Protocol error: Invalid packet size for opcode OPCODE_HELLO expected " + std::to_string(sizeof(packet)) + " but got " + std::to_string(size));
	size -= sizeof(packet);
	this->onDisconnect(" has disconnected");
	this->_init = false;
}

void Connection::_handlePacket(const Lobbies::PacketKicked &, size_t &)
{
	this->kick("Protocol error: OPCODE_KICKED unexpected");
}

void Connection::_handlePacket(const Lobbies::PacketMove &packet, size_t &size)
{
	if (!this->_init)
		return this->kick("Protocol error: Invalid handshake");
	if (size < sizeof(packet))
		return this->kick("Protocol error: Invalid packet size for opcode OPCODE_MOVE expected " + std::to_string(sizeof(packet)) + " but got " + std::to_string(size));
	size -= sizeof(packet);
	this->_dir = packet.dir;
	this->onMove(packet.dir);
}

void Connection::_handlePacket(const Lobbies::PacketPosition &packet, size_t &size)
{
	if (!this->_init)
		return this->kick("Protocol error: Invalid handshake");
	if (size < sizeof(packet))
		return this->kick("Protocol error: Invalid packet size for opcode OPCODE_POSITION expected " + std::to_string(sizeof(packet)) + " but got " + std::to_string(size));
	size -= sizeof(packet);
	this->_pos = {packet.x, packet.y};
	this->onPosition(packet.x, packet.y);
	this->_timeoutClock.restart();
}

void Connection::_handlePacket(const Lobbies::PacketGameRequest &packet, size_t &size)
{
	if (!this->_init)
		return this->kick("Protocol error: Invalid handshake");
	if (size < sizeof(packet))
		return this->kick("Protocol error: Invalid packet size for opcode OPCODE_GAME_REQUEST expected " + std::to_string(sizeof(packet)) + " but got " + std::to_string(size));
	size -= sizeof(packet);
	this->_machineId = packet.consoleId;
	this->onGameRequest();
	this->_battleStatus = 1;
}

void Connection::_handlePacket(const Lobbies::PacketGameStart &packet, size_t &size)
{
	if (!this->_init)
		return this->kick("Protocol error: Invalid handshake");
	if (size < sizeof(packet))
		return this->kick("Protocol error: Invalid packet size for opcode OPCODE_GAME_START expected " + std::to_string(sizeof(packet)) + " but got " + std::to_string(size));
	size -= sizeof(packet);
	this->_room.ip = std::string(packet.ip, strnlen(packet.ip, sizeof(packet.ip)));
	this->_room.port = packet.port;
	this->onGameStart(this->_room);
}

void Connection::_handlePacket(const Lobbies::PacketPing &packet, size_t &size)
{
	if (size < sizeof(packet))
		return this->kick("Protocol error: Invalid packet size for opcode OPCODE_PING expected " + std::to_string(sizeof(packet)) + " but got " + std::to_string(size));
	size -= sizeof(packet);

	auto lobby = this->onPing();
	Lobbies::PacketPong pong{lobby.name, lobby.maxPlayers, lobby.currentPlayers};

	this->send(&pong, sizeof(pong));
	this->_timeoutClock.restart();
}

void Connection::_handlePacket(const Lobbies::PacketPong &, size_t &)
{
	this->kick("Protocol error: OPCODE_PONG unexpected");
}

void Connection::_handlePacket(const Lobbies::PacketSettingsUpdate &packet, size_t &size)
{
	if (!this->_init)
		return this->kick("Protocol error: Invalid handshake");
	if (size < sizeof(packet))
		return this->kick("Protocol error: Invalid packet size for opcode OPCODE_SETTINGS_UPDATE expected " + std::to_string(sizeof(packet)) + " but got " + std::to_string(size));
	size -= sizeof(packet);
	this->_settings = packet.settings;
	this->_player = packet.custom;
	this->onSettingsUpdate(packet);
}

void Connection::_handlePacket(const Lobbies::PacketArcadeEngage &, size_t &)
{
	this->kick("Protocol error: OPCODE_ARCADE_ENGAGE unexpected");
}

void Connection::_handlePacket(const Lobbies::PacketArcadeLeave &packet, size_t &size)
{
	if (!this->_init)
		return this->kick("Protocol error: Invalid handshake");
	if (size < sizeof(packet))
		return this->kick("Protocol error: Invalid packet size for opcode OPCODE_ARCADE_LEAVE expected " + std::to_string(sizeof(packet)) + " but got " + std::to_string(size));
	size -= sizeof(packet);
	this->onArcadeLeave();
	this->_battleStatus = 0;
	this->_machineId = 0;
}

void Connection::_handlePacket(const Lobbies::PacketMessage &packet, size_t &size)
{
	if (!this->_init)
		return this->kick("Protocol error: Invalid handshake");
	if (size < sizeof(packet))
		return this->kick("Protocol error: Invalid packet size for opcode OPCODE_MESSAGE expected " + std::to_string(sizeof(packet)) + " but got " + std::to_string(size));
	size -= sizeof(packet);
	this->onMessage(packet.channelId, packet.message);
}

void Connection::_handlePacket(const Lobbies::PacketImportantMessage &, size_t &)
{
	this->kick("Protocol error: OPCODE_IMPORTANT_MESSAGE unexpected");
}
