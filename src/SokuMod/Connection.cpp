//
// Created by PinkySmile on 01/10/2022.
//

#ifndef _LOBBYNOLOG
#include <iostream>
#include <mutex>
std::mutex logMutex;
#endif
#include <cstring>
#include <algorithm>
#include <chrono>
#include <vector>
#include <functional>
#include <Exceptions.hpp>
#include "Connection.hpp"
#include "getPublicIp.hpp"
#include "ipv6map_extern.hpp"

#define print(...) do { if (this->_init) printf(__VA_ARGS__); } while(0)

extern unsigned char soku2Major;
extern unsigned char soku2Minor;
extern char soku2Letter;
extern bool soku2Force;

static long long steadyClockMilliseconds()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()
	).count();
}

void Connection::_netLoop()
{
	char buffer[sizeof(Lobbies::Packet) * 6];
	size_t recvSizeAdded;
	size_t recvSize = 0;

	while (true) {
		if (!this->_connected)
			return;

		if (!this->_hasConnected) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

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

		print("Handle packet. %zu bytes remain\n", recvSize);
		while (this->_handlePacket(*reinterpret_cast<Lobbies::Packet *>(&buffer[total - recvSize]), recvSize) && recvSize != 0 && this->_connected)
			print("Next packet. %zu bytes remain\n", recvSize);
		print("No more packets. %zu bytes remain\n", recvSize);
		memmove(buffer, &buffer[total - recvSize], recvSize);
	}
}

Connection::Connection(const std::string &host, unsigned short port, const Player &initParams) :
	_host(host),
	_port(port),
	_initParams(initParams)
{
}

Connection::~Connection()
{
	this->_init = false;
	this->_connected = false;
	{
		std::lock_guard<std::mutex> meMutexGuard(this->meMutex);
		std::lock_guard<std::mutex> playerMutexGuard(this->_playerMutex);
		std::lock_guard<std::mutex> functionMutexGuard(this->functionMutex);
		if (this->onDisconnect)
			this->onDisconnect();
	}
	// Closing the socket first interrupts a pending connect/recv so joining cannot
	// block the game thread until the operating system network timeout expires.
	this->_socket.disconnect();
	if (this->_connectThread.joinable())
		this->_connectThread.join();
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
	this->_connectThread = std::thread([this](){
		try {
			this->_socket.connect(this->_host, this->_port);
			this->_hasConnected = true;
		} catch (std::exception &e) {
			std::lock_guard<std::mutex> meMutexGuard(this->meMutex);
			std::lock_guard<std::mutex> playerMutexGuard(this->_playerMutex);
			this->error(e.what());
		}
	});
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

bool Connection::hasConnected() const
{
	return this->_hasConnected;
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
	case Lobbies::OPCODE_BATTLE_STATUS_UPDATE:
		return this->_handlePacket(packet.battleStatusUpdate, size);
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
	if (size < sizeof(packet)) {
		print("Lobbies::PacketOlleh: size < sizeof(packet): %zu < %zu\n", size, sizeof(packet));
		return false;
	}
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
	if (size < sizeof(packet)) {
		print("Lobbies::PacketPlayerJoin: size < sizeof(packet): %zu < %zu\n", size, sizeof(packet));
		return false;
	}
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
	if (size < sizeof(packet)) {
		print("Lobbies::PacketPlayerLeave: size < sizeof(packet): %zu < %zu\n", size, sizeof(packet));
		return false;
	}
	size -= sizeof(packet);
	if (packet.id == this->_me->id)
		return this->error("Protocol error: Server sent OPCODE_PLAYER_LEAVE with self id"), false;
	auto it = this->_players.find(packet.id);
	if (it == this->_players.end())
		return true;
	if (this->onPlayerLeave)
		this->onPlayerLeave(it->second);
	this->_players.erase(it);
	return true;
}

bool Connection::_handlePacket(const Lobbies::PacketKicked &packet, size_t &size)
{
	if (size < sizeof(packet)) {
		print("Lobbies::PacketKicked: size < sizeof(packet): %zu < %zu\n", size, sizeof(packet));
		return false;
	}
	size -= sizeof(packet);

	std::string reason(packet.message, strnlen(packet.message, sizeof(packet.message)));
	constexpr char inactivePrefix[] = "Kicked for being inactive for ";
	constexpr char inactiveSuffix[] = " minutes.";
	bool inactiveAutoKick = false;

	if (
		reason.size() > sizeof(inactivePrefix) - 1 + sizeof(inactiveSuffix) - 1 &&
		reason.compare(0, sizeof(inactivePrefix) - 1, inactivePrefix) == 0 &&
		reason.compare(reason.size() - (sizeof(inactiveSuffix) - 1), sizeof(inactiveSuffix) - 1, inactiveSuffix) == 0
	) {
		auto minutesBegin = reason.begin() + sizeof(inactivePrefix) - 1;
		auto minutesEnd = reason.end() - (sizeof(inactiveSuffix) - 1);

		inactiveAutoKick = std::all_of(minutesBegin, minutesEnd, [](unsigned char c) {
			return c >= '0' && c <= '9';
		});
	}
	auto nowMs = steadyClockMilliseconds();
	auto lastSpectatingAtMs = this->_lastSpectatingAtMs.load();
	bool recentlySpectating = lastSpectatingAtMs > 0 && nowMs - lastSpectatingAtMs <= 10000;
	bool inSpectatingScene =
		SokuLib::sceneId == SokuLib::SCENE_LOADINGWATCH ||
		SokuLib::sceneId == SokuLib::SCENE_BATTLEWATCH ||
		SokuLib::newSceneId == SokuLib::SCENE_LOADINGWATCH ||
		SokuLib::newSceneId == SokuLib::SCENE_BATTLEWATCH;
	bool spectatorContext = this->_spectatingArcade.load() || this->_spectatingScene.load() || inSpectatingScene || recentlySpectating;
	if (!(spectatorContext && inactiveAutoKick) && this->onImpMsg)
		this->onImpMsg("Kicked: " + reason);
	this->_init = false;
	this->_connected = false;
	this->_socket.disconnect();
	return true;
}

bool Connection::_handlePacket(const Lobbies::PacketMove &packet, size_t &size)
{
	if (!this->_init)
		return this->error("Protocol error: Invalid handshake"), false;
	if (size < sizeof(packet)) {
		print("Lobbies::PacketMove: size < sizeof(packet): %zu < %zu\n", size, sizeof(packet));
		return false;
	}
	size -= sizeof(packet);
	auto player = this->_players.find(packet.id);
	if (player != this->_players.end())
		player->second.dir = packet.dir;
	return true;
}

bool Connection::_handlePacket(const Lobbies::PacketPosition &packet, size_t &size)
{
	if (!this->_init)
		return this->error("Protocol error: Invalid handshake"), false;
	if (size < sizeof(packet)) {
		print("Lobbies::PacketPosition: size < sizeof(packet): %zu < %zu\n", size, sizeof(packet));
		return false;
	}
	size -= sizeof(packet);
	auto player = this->_players.find(packet.id);
	if (player != this->_players.end()) {
		player->second.pos = {packet.x, packet.y};
		player->second.battleStatus = packet.status;
		player->second.dir = packet.dir;
	}
	return true;
}

bool Connection::_handlePacket(const Lobbies::PacketGameRequest &packet, size_t &size)
{
	if (!this->_init)
		return this->error("Protocol error: Invalid handshake"), false;
	if (size < sizeof(packet)) {
		print("Lobbies::PacketGameRequest: size < sizeof(packet): %zu < %zu\n", size, sizeof(packet));
		return false;
	}
	size -= sizeof(packet);
	return this->sendGameInfo();
}

bool Connection::_handlePacket(const Lobbies::PacketGameStart &packet, size_t &size)
{
	if (!this->_init)
		return this->error("Protocol error: Invalid handshake"), false;
	if (size < sizeof(packet)) {
		print("Lobbies::PacketGameStart: size < sizeof(packet): %zu < %zu\n", size, sizeof(packet));
		return false;
	}
	size -= sizeof(packet);
	this->_spectatingArcade = packet.spectator;
	if (packet.spectator)
		this->_lastSpectatingAtMs = steadyClockMilliseconds();
	if (this->onConnectRequest){
		const char * ip = packet.ip;
		unsigned short port = packet.port;
		char ipv6[sizeof(packet.ipv6)+1];
		char ipv6MappedIpv4[16];
		if (isIpv6Available() && packet.port6){
			strncpy(ipv6, packet.ipv6, sizeof(packet.ipv6));
			ipv6[sizeof(ipv6)-1] = '\0';
			print("p1 ipv6 is: %s\n", ipv6);
			if (mapIpv6toIpv4(ipv6, ipv6MappedIpv4, sizeof(ipv6MappedIpv4))){
				ip = ipv6MappedIpv4;
				port = packet.port6;
			}
		}
		print("p1: %s:%u\n", ip, port);
		this->onConnectRequest(ip, port, packet.spectator);
	}
	return true;
}

bool Connection::_handlePacket(const Lobbies::PacketPing &, size_t &)
{
	return this->error("Protocol error: OPCODE_PING unexpected"), false;
}

bool Connection::_handlePacket(const Lobbies::PacketPong &packet, size_t &size)
{
	if (size < sizeof(packet)) {
		print("Lobbies::PacketPong: size < sizeof(packet): %zu < %zu\n", size, sizeof(packet));
		return false;
	}
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
	if (size < sizeof(packet)) {
		print("Lobbies::PacketSettingsUpdate: size < sizeof(packet): %zu < %zu\n", size, sizeof(packet));
		return false;
	}
	size -= sizeof(packet);
	auto player = this->_players.find(packet.id);
	if (player != this->_players.end())
		player->second.player = packet.custom;
	return true;
}

bool Connection::_handlePacket(const Lobbies::PacketArcadeEngage &packet, size_t &size)
{
	if (!this->_init)
		return this->error("Protocol error: Invalid handshake"), false;
	if (size < sizeof(packet)) {
		print("Lobbies::PacketArcadeEngage: size < sizeof(packet): %zu < %zu\n", size, sizeof(packet));
		return false;
	}
	size -= sizeof(packet);
	auto player = this->_players.find(packet.id);
	if (player == this->_players.end())
		return true;
	player->second.machineId = packet.machineId;
	if (this->onArcadeEngage)
		this->onArcadeEngage(player->second, packet.machineId);
	return true;
}

bool Connection::_handlePacket(const Lobbies::PacketArcadeLeave &packet, size_t &size)
{
	if (!this->_init)
		return this->error("Protocol error: Invalid handshake"), false;
	if (size < sizeof(packet)) {
		print("Lobbies::PacketArcadeLeave: size < sizeof(packet): %zu < %zu\n", size, sizeof(packet));
		return false;
	}
	size -= sizeof(packet);
	if (this->_me && packet.id == this->_me->id)
		this->_spectatingArcade = false;
	auto player = this->_players.find(packet.id);
	if (player == this->_players.end())
		return true;
	if (this->onArcadeLeave)
		this->onArcadeLeave(player->second, player->second.machineId);
	player->second.machineId = 0;
	return true;
}

void Connection::setSpectatingScene(bool spectating)
{
	this->_spectatingScene = spectating;
	if (spectating)
		this->_lastSpectatingAtMs = steadyClockMilliseconds();
}

bool Connection::_handlePacket(const Lobbies::PacketMessage &packet, size_t &size)
{
	if (size < sizeof(packet)) {
		print("Lobbies::PacketMessage: size < sizeof(packet): %zu < %zu\n", size, sizeof(packet));
		return false;
	}
	size -= sizeof(packet);
	if (this->onMsg)
		this->onMsg(packet.channelId, packet.playerId, std::string(packet.message, strnlen(packet.message, sizeof(packet.message))));
	return true;
}

bool Connection::_handlePacket(const Lobbies::PacketImportantMessage &packet, size_t &size)
{
	if (!this->_init)
		return this->error("Protocol error: Invalid handshake"), false;
	if (size < sizeof(packet)) {
		print("Lobbies::PacketImportantMessage: size < sizeof(packet): %zu < %zu\n", size, sizeof(packet));
		return false;
	}
	size -= sizeof(packet);
	if (this->onImpMsg)
		this->onImpMsg(std::string(packet.message, strnlen(packet.message, sizeof(packet.message))));
	return true;
}

bool Connection::_handlePacket(const Lobbies::PacketBattleStatusUpdate &packet, size_t &size)
{
	if (!this->_init)
		return this->error("Protocol error: Invalid handshake"), false;
	if (size < sizeof(packet)) {
		print("Lobbies::PacketBattleStatusUpdate: size < sizeof(packet): %zu < %zu\n", size, sizeof(packet));
		return false;
	}
	size -= sizeof(packet);
	auto player = this->_players.find(packet.playerId);
	if (player != this->_players.end())
		player->second.battleStatus = packet.newStatus;
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
		if (p.first && !p.second.name.empty())
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
		if (this->_init) {
			this->meMutex.lock();
			Lobbies::PacketPosition position{0, this->_me->pos.x, this->_me->pos.y, this->_me->dir, this->_me->battleStatus};
			this->meMutex.unlock();

			this->send(&position, sizeof(position));
		}
		for (int i = 0; i < 10 && this->_init; i++)
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

void Connection::setPassword(const std::string &pwd)
{
	this->_pwd = pwd;
}

bool Connection::sendGameInfo()
{
	const char *ip;
	std::string ipv6;
	try {
		ip = getMyIp();
		if (isIpv6Available())
			ipv6 = getMyIpv6();
	} catch (std::exception &e) {
		this->error(std::string("Failed to get public IP: ") + e.what());
		return false;
	}
	unsigned short port = this->onHostRequest();
	unsigned short port6 = ipv6.empty() ? 0 : port;
	// if (ipv6)
	// 	print("My ipv6: %s\n", ipv6);
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

	Lobbies::PacketGameStart game{dup, port, port6 ? ipv6 : "", port6, false};

	free(dup);
	this->send(&game, sizeof(game));
	return true;
}
