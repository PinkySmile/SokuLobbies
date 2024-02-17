//
// Created by PinkySmile on 01/10/2022.
//

#ifndef _LOBBYNOLOG
#include <iostream>
#include <mutex>
std::mutex logMutex;
#endif
#include <cstring>
#include <vector>
#include <functional>
#include <Exceptions.hpp>
#include "Connection.hpp"
#include "getPublicIp.hpp"


extern unsigned char soku2Major;
extern unsigned char soku2Minor;
extern char soku2Letter;
extern bool soku2Force;

void Connection::_netLoop()
{
	char buffer[sizeof(Lobbies::Packet) * 6];
	size_t recvSizeAdded = 0;
	size_t recvSize = 0;

	while (true) {
		try {
			recvSize += (recvSizeAdded = this->_socket.read(buffer + recvSize, sizeof(buffer) - recvSize));
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
			{
				std::lock_guard<std::mutex> meMutexGuard(this->meMutex);
				std::lock_guard<std::mutex> playerMutexGuard(this->_playerMutex);
				std::lock_guard<std::mutex> functionMutexGuard(this->functionMutex);
				this->onError(e.what());
				if (this->onDisconnect)
					this->onDisconnect();
			}
			return;
		}

		if (!this->_connected)
			return;
		if (recvSizeAdded == 0) {
			this->_init = false;
			this->_connected = false;
			{
				std::lock_guard<std::mutex> meMutexGuard(this->meMutex);
				std::lock_guard<std::mutex> playerMutexGuard(this->_playerMutex);
				std::lock_guard<std::mutex> functionMutexGuard(this->functionMutex);
				this->onError("Connection closed");
				if (this->onDisconnect)
					this->onDisconnect();
			}
			return;
		}

		size_t total = recvSize;

		while (this->_handlePacket(*reinterpret_cast<Lobbies::Packet *>(&buffer[total - recvSize]), recvSize) && recvSize != 0 && this->_connected);
		memmove(buffer, &buffer[total - recvSize], recvSize);
	}
}

Connection::Connection(const std::string &host, unsigned short port, const Player &initParams) :
	_initParams(initParams)
{
	this->_socket.connect(host, port);
}

Connection::~Connection()
{
	this->_init = false;
	{
		std::lock_guard<std::mutex> meMutexGuard(this->meMutex);
		std::lock_guard<std::mutex> playerMutexGuard(this->_playerMutex);
		std::lock_guard<std::mutex> functionMutexGuard(this->functionMutex);
		if (this->onDisconnect)
			this->onDisconnect();
	}
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

bool Connection::_handlePacket(const Lobbies::Packet &packet, size_t &size)
{
#ifndef _LOBBYNOLOG
	logMutex.lock();
	std::cout << "[<" << inet_ntoa(this->_socket.getRemote().sin_addr) << ":" << this->_socket.getRemote().sin_port;
	std::cout << "] " << size << " bytes: " << packet.toString() << std::endl;
	logMutex.unlock();
#endif
	std::lock_guard<std::mutex> meMutexGuard(this->meMutex);
	std::lock_guard<std::mutex> playerMutexGuard(this->_playerMutex);
	std::lock_guard<std::mutex> functionMutexGuard(this->functionMutex);
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
		return this->error("Protocol error: Invalid opcode " + std::to_string(packet.opcode)), false;
	}
}

bool Connection::_handlePacket(const Lobbies::PacketHello &, size_t &)
{
	return this->error("Protocol error: OPCODE_HELLO unexpected"), false;
}

bool Connection::_handlePacket(const Lobbies::PacketOlleh &packet, size_t &size)
{
	if (size < sizeof(packet))
		return false;
	size -= sizeof(packet);

	Player player = this->_initParams;

	player.id = packet.id;
	player.name = std::string(packet.realName, strnlen(packet.realName, sizeof(packet.realName)));
	this->_players[packet.id] = player;
	this->_me = &this->_players[packet.id];
	this->_init = true;
	this->_posThread = std::thread(&Connection::_posLoop, this);
	if (this->onConnect)
		this->onConnect(packet);
	return true;
}

bool Connection::_handlePacket(const Lobbies::PacketPlayerJoin &packet, size_t &size)
{
	if (!this->_init)
		return this->error("Protocol error: Invalid handshake"), false;
	if (size < sizeof(packet))
		return false;
	size -= sizeof(packet);

	Player player;

	player.id = packet.id;
	player.name = std::string(packet.name, strnlen(packet.name, sizeof(packet.name)));
	player.player = packet.custom;
	this->_players[packet.id] = player;
	if (this->onPlayerJoin)
		this->onPlayerJoin(player);
	return true;
}

bool Connection::_handlePacket(const Lobbies::PacketPlayerLeave &packet, size_t &size)
{
	if (!this->_init)
		return this->error("Protocol error: Invalid handshake"), false;
	if (size < sizeof(packet))
		return false;
	size -= sizeof(packet);
	if (packet.id == this->_me->id)
		return this->error("Protocol error: Server sent OPCODE_PLAYER_LEAVE with self id"), false;
	this->_players.erase(this->_players.find(packet.id));
	return true;
}

bool Connection::_handlePacket(const Lobbies::PacketKicked &packet, size_t &size)
{
	if (size < sizeof(packet))
		return false;
	size -= sizeof(packet);
	if (this->onImpMsg)
		this->onImpMsg("Kicked: " + std::string(packet.message, strnlen(packet.message, sizeof(packet.message))));
	this->_init = false;
	this->_connected = false;
	this->_socket.disconnect();
	return true;
}

bool Connection::_handlePacket(const Lobbies::PacketMove &packet, size_t &size)
{
	if (!this->_init)
		return this->error("Protocol error: Invalid handshake"), false;
	if (size < sizeof(packet))
		return false;
	size -= sizeof(packet);
	this->_players[packet.id].dir = packet.dir;
	return true;
}

bool Connection::_handlePacket(const Lobbies::PacketPosition &packet, size_t &size)
{
	if (!this->_init)
		return this->error("Protocol error: Invalid handshake"), false;
	if (size < sizeof(packet))
		return false;
	size -= sizeof(packet);
	this->_players[packet.id].pos = {packet.x, packet.y};
	return true;
}

bool Connection::_handlePacket(const Lobbies::PacketGameRequest &packet, size_t &size)
{
	if (!this->_init)
		return this->error("Protocol error: Invalid handshake"), false;
	if (size < sizeof(packet))
		return false;
	size -= sizeof(packet);

	const char* ip;
	try {
		ip = getMyIp();
	} catch (std::exception &e) {
		this->error(std::string("Failed to get public IP: ") + e.what());
		return false;
	}
	unsigned short port = this->onHostRequest();
	auto dup = strdup(ip);
	char *pos = strchr(dup, ':');

	if (pos) {
		try {
			port = std::stoul(pos + 1);
		} catch (std::exception &e) {
			puts(e.what());
		}
		*pos = 0;
	}

	Lobbies::PacketGameStart game{dup, port, false};

	free(dup);
	this->send(&game, sizeof(game));
	this->_me->battleStatus = 2;
	return true;
}

bool Connection::_handlePacket(const Lobbies::PacketGameStart &packet, size_t &size)
{
	if (!this->_init)
		return this->error("Protocol error: Invalid handshake"), false;
	if (size < sizeof(packet))
		return false;
	size -= sizeof(packet);
	if (this->onConnectRequest)
		this->onConnectRequest(packet.ip, packet.port, packet.spectator);
	this->_me->battleStatus = 2;
	return true;
}

bool Connection::_handlePacket(const Lobbies::PacketPing &, size_t &)
{
	return this->error("Protocol error: OPCODE_PING unexpected"), false;
}

bool Connection::_handlePacket(const Lobbies::PacketPong &packet, size_t &size)
{
	if (size < sizeof(packet))
		return false;
	size -= sizeof(packet);

	std::lock_guard<std::mutex> infoMutexLock(this->_infoMutex);
	this->_info.name = std::string(packet.name, strnlen(packet.name, sizeof(packet.name)));
	this->_info.maxPlayers = packet.maxPlayers;
	this->_info.currentPlayers = packet.currentPlayers;
	this->_info.hasPwd = packet.requiresPwd;
	return true;
}

bool Connection::_handlePacket(const Lobbies::PacketSettingsUpdate &packet, size_t &size)
{
	if (!this->_init)
		return this->error("Protocol error: Invalid handshake"), false;
	if (size < sizeof(packet))
		return false;
	size -= sizeof(packet);
	this->_players[packet.id].player = packet.custom;
	return true;
}

bool Connection::_handlePacket(const Lobbies::PacketArcadeEngage &packet, size_t &size)
{
	if (!this->_init)
		return this->error("Protocol error: Invalid handshake"), false;
	if (size < sizeof(packet))
		return false;
	size -= sizeof(packet);
	this->_players[packet.id].battleStatus = 1;
	this->_players[packet.id].machineId = packet.machineId;
	if (this->onArcadeEngage)
		this->onArcadeEngage(this->_players[packet.id], packet.machineId);
	return true;
}

bool Connection::_handlePacket(const Lobbies::PacketArcadeLeave &packet, size_t &size)
{
	if (!this->_init)
		return this->error("Protocol error: Invalid handshake"), false;
	if (size < sizeof(packet))
		return false;
	size -= sizeof(packet);
	this->_players[packet.id].battleStatus = 0;
	if (this->onArcadeLeave)
		this->onArcadeLeave(this->_players[packet.id], this->_players[packet.id].machineId);
	return true;
}

bool Connection::_handlePacket(const Lobbies::PacketMessage &packet, size_t &size)
{
	if (size < sizeof(packet))
		return false;
	size -= sizeof(packet);
	if (this->onMsg)
		this->onMsg(packet.channelId, packet.playerId, std::string(packet.message, strnlen(packet.message, sizeof(packet.message))));
	return true;
}

bool Connection::_handlePacket(const Lobbies::PacketImportantMessage &packet, size_t &size)
{
	if (!this->_init)
		return this->error("Protocol error: Invalid handshake"), false;
	if (size < sizeof(packet))
		return false;
	size -= sizeof(packet);
	if (this->onImpMsg)
		this->onImpMsg(std::string(packet.message, strnlen(packet.message, sizeof(packet.message))));
	return true;
}

void Connection::connect()
{
	if (this->_init)
		return;

	unsigned char version[16];
	constexpr unsigned char rollNoSWRVersion[16] = {
		0x6F, 0x53, 0xD5, 0x29,
		0xFA, 0xC9, 0x60, 0x18,
		0x85, 0x9C, 0x21, 0xE2,
		0x71, 0x36, 0x70, 0x9F
	};
	Lobbies::Soku2VersionInfo soku2Info{soku2Major, soku2Minor, soku2Letter, soku2Force};

	// SokuRoll doesn't change the version string for non SWR linked so we replace it with something custom
	if (memcmp((unsigned char *)0x858B80, SokuLib::Soku110acRollSWRAllChars, 16) == 0 && SokuLib::SWRUnlinked)
		memcpy(version, rollNoSWRVersion, 16);
	else if (SokuLib::SWRUnlinked)
		memcpy(version, (unsigned char *)0x858B90, 16);
	else
		memcpy(version, (unsigned char *)0x858B80, 16);
	// Giuroll doesn't change the version string for non SWR linked either so we mirror the first byte
	if (
		memcmp((unsigned char *)0x858B81, SokuLib::Soku110acNoRollSWRAllChars + 1, 15) == 0 &&
		*(unsigned char *)0x858B80 != *SokuLib::Soku110acNoRollSWRAllChars
	)
		version[0] = *(unsigned char *)0x858B80;

	if (this->_pwd) {
		Lobbies::PacketHello hello{soku2Info, version, this->_initParams.name, this->_initParams.player, this->_initParams.settings, *this->_pwd};

		this->send(&hello, sizeof(hello));
	} else {
		Lobbies::PacketHello hello{soku2Info, version, this->_initParams.name, this->_initParams.player, this->_initParams.settings};

		this->send(&hello, sizeof(hello));
	}
}

void Connection::disconnect()
{
	if (!this->_init)
		return;

	Lobbies::PacketPlayerLeave leave{0};

	this->send(&leave, sizeof(leave));
	{
		std::lock_guard<std::mutex> meMutexGuard(this->meMutex);
		std::lock_guard<std::mutex> playerMutexGuard(this->_playerMutex);
		std::lock_guard<std::mutex> functionMutexGuard(this->functionMutex);
		this->_init = false;
		this->_me = nullptr;
		this->_players.clear();
		if (this->onDisconnect)
			this->onDisconnect();
	}
	if (this->_posThread.joinable())
		this->_posThread.join();
}

const Connection::LobbyInfo Connection::getLobbyInfo() const
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

	std::lock_guard<std::mutex> playerMutexGuard(this->_playerMutex);
	for (auto &p : this->_players)
		players.push_back(p.second);
	return players;
}

std::vector<std::string> Connection::getMessages() const
{
	this->_messagesMutex.lock();

	auto result = this->_messages;

	this->_messagesMutex.unlock();
	return result;
}

static void playerBasicAnimation(Player &player, const LobbyData::Avatar &avatar)
{
	char scale = 1 + ((player.dir & 0b100000) != 0);

	if (player.dir & 1)
		player.pos.x += PLAYER_H_SPEED * scale;
	if (player.dir & 2)
		player.pos.x -= PLAYER_H_SPEED * scale;
	player.animationCtr++;
	if (player.animationCtr > avatar.animationsStep) {
		player.currentAnimation++;
		player.currentAnimation %= avatar.nbAnimations;
		player.animationCtr = 0;
	}
	player.animation = (player.dir & 0b00011) != 0;
}

static void playerSuwakoAnimation(Player &player, const LobbyData::Avatar &avatar)
{
	char scale = 1 + ((player.dir & 0b100000) != 0);

	if (player.dir & 1)
		player.pos.x += PLAYER_H_SPEED * scale;
	if (player.dir & 2)
		player.pos.x -= PLAYER_H_SPEED * scale;
	player.animationCtr++;
	if (player.animationCtr > avatar.animationsStep) {
		player.currentAnimation++;
		player.currentAnimation %= avatar.nbAnimations;
		player.animationCtr = 0;
	}
	player.animation = (player.dir & 0b00011) != 0;
}

std::vector<std::function<void (Player &, const LobbyData::Avatar &)>> Connection::_playerUpdateHandles{
	playerBasicAnimation,
	playerSuwakoAnimation
};

void Connection::updatePlayers(const std::vector<LobbyData::Avatar> &avatars)
{
	this->_playerMutex.lock();
	for (auto &p : this->_players) {
		if (p.second.player.avatar >= avatars.size()) {
			char scale = 1 + ((p.second.dir & 0b100000) != 0);

			if (p.second.dir & 1)
				p.second.pos.x += PLAYER_H_SPEED * scale;
			if (p.second.dir & 2)
				p.second.pos.x -= PLAYER_H_SPEED * scale;
			p.second.animation = (p.second.dir & 0b00011) != 0;
			continue;
		}

		auto &avatar = avatars[p.second.player.avatar];

		Connection::_playerUpdateHandles[avatar.animationStyle](p.second, avatar);
	}
	this->_playerMutex.unlock();
}

void Connection::_posLoop()
{
	while (this->_init) {
		this->meMutex.lock();
		if (this->_init) {
			Lobbies::PacketPosition position{0, this->_me->pos.x, this->_me->pos.y};
			this->send(&position, sizeof(position));
		}
		this->meMutex.unlock();
		for (int i = 0; i < 10 && this->_init; i++)
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

void Connection::setPassword(const std::string &pwd)
{
	this->_pwd = pwd;
}
