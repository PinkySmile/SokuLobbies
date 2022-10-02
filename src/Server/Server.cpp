//
// Created by PinkySmile on 01/10/2022.
//

#ifndef _LOBBYNOLOG
#include <iostream>
#include <mutex>
extern std::mutex logMutex;
#endif
#include <memory>
#include <cstring>
#include "Server.hpp"

void Server::run(unsigned short port, unsigned maxPlayers, const std::string &name)
{
#ifndef _DEBUG
	try {
#endif
		auto socket = std::make_unique<sf::TcpSocket>();

		this->_infos.maxPlayers = maxPlayers;
		this->_infos.currentPlayers = 0;
		this->_infos.name = name;
		this->_listener.listen(port);
		this->_listener.setBlocking(false);
#ifndef _LOBBYNOLOG
		logMutex.lock();
		std::cout << "Listening on port " << port << std::endl;
		logMutex.unlock();
#endif
		this->_registerToMainServer();
		while (this->_opened) {
			if (sf::Socket::Done == this->_listener.accept(*socket)) {
				this->_connectionsMutex.lock();
				this->_connections.emplace_back(socket);
				this->_prepareConnectionHandlers(this->_connections.back());
				this->_connectionsMutex.unlock();
				socket = std::make_unique<sf::TcpSocket>();
			} else
				std::this_thread::sleep_for(std::chrono::milliseconds(100));

			this->_connectionsMutex.lock();
			this->_connections.erase(std::remove_if(this->_connections.begin(), this->_connections.end(), [](Connection &c) {
				return !c.isConnected();
			}), this->_connections.end());
			this->_connectionsMutex.unlock();
		}
#ifndef _DEBUG
	} catch (std::exception &e) {
		this->_listener.close();
		this->_connectionsMutex.lock();
		for (auto &c : this->_connections)
			c.kick("Internal server error: " + std::string(e.what()));
		this->_connectionsMutex.unlock();
#ifndef _LOBBYNOLOG
		logMutex.lock();
		std::cerr << "Fatal error " << e.what() << std::endl;
		logMutex.unlock();
#endif
		throw;
	}
#endif
	this->_listener.close();
	this->_connectionsMutex.lock();
	for (auto &c : this->_connections)
		c.kick("Server closed");
	this->_connectionsMutex.unlock();
#ifndef _LOBBYNOLOG
	logMutex.lock();
	std::cout << "Server closed" << std::endl;
	logMutex.unlock();
#endif
}

void Server::_prepareConnectionHandlers(Connection &connection)
{
	bool ok = false;
	unsigned id = 1;

	while (!ok) {
		ok = true;
		for (auto &c : this->_connections)
			if (c.getId() == id && c.isInit()) {
				ok = false;
				id++;
			}
	}

	connection.onPing = [this](){
		return this->_infos;
	};
	connection.onMove = [this, id](uint8_t dir){
		Lobbies::PacketMove move{id, dir};

		this->_connectionsMutex.lock();
		for (auto &c : this->_connections)
			if (c.getId() != id && c.isInit())
				c.send(&move, sizeof(move));
		this->_connectionsMutex.unlock();
	};
	connection.onPosition = [this, id](uint32_t x, uint32_t y){
		Lobbies::PacketPosition pos{id, x, y};

		this->_connectionsMutex.lock();
		for (auto &c : this->_connections)
			if (c.getId() != id && c.isInit())
				c.send(&pos, sizeof(pos));
		this->_connectionsMutex.unlock();
	};
	connection.onDisconnect = [this, &connection](const std::string &reason){
		Lobbies::PacketMessage msg{0, 0, connection.getName() + reason};

		this->_connectionsMutex.lock();
		for (auto &c : this->_connections)
			if (c.isInit())
				c.send(&msg, sizeof(msg));
		if (connection.getBattleStatus())
			connection.onArcadeLeave();
		this->_connectionsMutex.unlock();
	};
	connection.onJoin = [this, &connection, id](const Lobbies::PacketHello &packet, std::string ip, std::string &name){
		auto it = std::find_if(this->_banList.begin(), this->_banList.end(), [&connection, &ip](BanEntry entry){
			return entry.ip == ip || memcmp(entry.uniqueId, connection.getUniqueId(), sizeof(entry.uniqueId)) == 0;
		});

		if (it != this->_banList.end()) {
			connection.kick(it->profileName);
			return false;
		}
		name = this->_sanitizeName(std::string(packet.name, strnlen(packet.name, sizeof(packet.name))));

		Lobbies::PacketOlleh res{name, id};
		Lobbies::PacketPlayerJoin join{id, name, packet.custom};

		connection.send(&res, sizeof(res));
		this->_connectionsMutex.lock();
		for (auto &c : this->_connections)
			if (c.isInit()) {
				Lobbies::PacketPlayerJoin join2{c.getId(), c.getName(), c.getPlayer()};
				Lobbies::PacketPosition pos{c.getId(), c.getPos().x, c.getPos().y};
				Lobbies::PacketMove move{c.getId(), c.getDir()};

				c.send(&join, sizeof(join));
				connection.send(&join2, sizeof(join2));
				connection.send(&pos, sizeof(pos));
				connection.send(&move, sizeof(move));
				if (connection.getBattleStatus()) {
					Lobbies::PacketArcadeEngage engage{c.getId()};

					connection.send(&engage, sizeof(engage));
				}
			}
		this->_connectionsMutex.unlock();
		return true;
	};
	connection.onMessage = [this, &connection, id](uint8_t channel, const std::string &msg){
		if (this->_processCommands(connection, msg))
			return;

		Lobbies::PacketMessage msgPacket{channel, id, connection.getName() + ": " + msg};

		this->_connectionsMutex.lock();
		for (auto &c : this->_connections)
			if (c.isInit())
				c.send(&msgPacket, sizeof(msgPacket));
		this->_connectionsMutex.unlock();
	};
	connection.onSettingsUpdate = [this, id](const Lobbies::PacketSettingsUpdate &settings){
		Lobbies::PacketSettingsUpdate packet{id, settings.custom, {}};

		this->_connectionsMutex.lock();
		for (auto &c : this->_connections)
			if (c.isInit())
				c.send(&packet, sizeof(packet));
		this->_connectionsMutex.unlock();
	};
	connection.onGameStart = [&connection, this](const Connection::Room &room){
		Lobbies::PacketGameStart packet{room.ip, room.port};

		this->_machinesMutex.lock();
		auto &machine = this->_machines[connection.getActiveMachine()];

		connection.setPlaying();
		for (auto c : machine)
			if (c != &connection)
				c->send(&packet, sizeof(packet));
		this->_machinesMutex.unlock();
	};
	connection.onGameRequest = [&connection, this, id](){
		Lobbies::PacketArcadeEngage engage{id};

		this->_machinesMutex.lock();
		auto &machine = this->_machines[connection.getActiveMachine()];

		if (!machine.empty() && machine[0]->getBattleStatus() == 2) {
			Lobbies::PacketGameStart packet{machine[0]->getRoomInfo().ip, machine[0]->getRoomInfo().port};

			connection.send(&packet, sizeof(packet));
		} else if (machine.size() >= 2 && machine[1]->getBattleStatus() == 2) {
			Lobbies::PacketGameStart packet{machine[1]->getRoomInfo().ip, machine[1]->getRoomInfo().port};

			connection.send(&packet, sizeof(packet));
		}
		machine.push_back(&connection);
		if (machine.size() == 2 && !this->_startRoom(machine)) {
			this->_machinesMutex.unlock();
			return;
		}
		this->_machinesMutex.unlock();

		this->_connectionsMutex.lock();
		for (auto &c : this->_connections)
			if (c.getId() != id && c.isInit())
				c.send(&engage, sizeof(engage));
		this->_connectionsMutex.unlock();
	};
	connection.onArcadeLeave = [&connection, this](){
		if (!connection.getBattleStatus())
			return;
		this->_machinesMutex.lock();

		auto &machine = this->_machines[connection.getActiveMachine()];

		// There is no need to check if the vector is of size >= 2
		// because there is no way that the connection is not in the list if the battle status is not 0
		if (machine[0] == &connection || machine[1] == &connection) {
			// This is one of the players, so we boot everyone out
			for (auto &c1 : machine) {
				Lobbies::PacketArcadeLeave packet{c1->getId()};

				this->_machinesMutex.lock();
				for (auto &c2 : this->_connections)
					if (c2.isInit())
						c2.send(&packet, sizeof(packet));
				c1->setNotPlaying();
				this->_machinesMutex.unlock();
			}
			machine.clear();
		} else
			machine.erase(std::find(machine.begin(), machine.end(), &connection));
		this->_machinesMutex.lock();
	};
	connection.setId(id);
	connection.startThread();
}

std::string Server::_sanitizeName(const std::string &name)
{
	unsigned nb = 0;
	std::string result = name;

loop:
	this->_connectionsMutex.lock();
	for (auto &c : this->_connections)
		if (c.getName() == result) {
			nb++;
			result = name + std::to_string(nb);
			this->_connectionsMutex.unlock();
			goto loop;
		}
	this->_connectionsMutex.unlock();
	return result;
}

bool Server::_startRoom(const std::vector<Connection *> &machine)
{
	auto p1settings = machine[0]->getSettings().hostPref & Lobbies::HOSTPREF_HOST_PREF_MASK;
	auto p2settings = machine[1]->getSettings().hostPref & Lobbies::HOSTPREF_HOST_PREF_MASK;

	if (p1settings == p2settings && p1settings == Lobbies::HOSTPREF_CLIENT_ONLY) {
		Lobbies::PacketImportantMessage msg{
			"<color FF0000>"
				"Error: Cannot start the game because both you and your opponent have their hosting preference set to 'Client only'."
				"One of you needs to be able to host to start the game."
				"Either change your settings to 'No preference' or 'Host only' if you can host."
				"Otherwise, try to join someone that can host games."
			"</color>"
		};
		Lobbies::PacketArcadeLeave leave{machine[1]->getId()};

		machine[0]->send(&msg, sizeof(msg));
		machine[1]->send(&msg, sizeof(msg));
		machine[1]->send(&leave, sizeof(leave));
		return false;
	}
	return true;
}

bool Server::_processCommands(Connection &author, const std::string &msg)
{
	if (msg.empty() || msg.front() != '/')
		return false;

	Lobbies::PacketMessage m{0, 0, "Commands are not yet implemented"};

	author.send(&m, sizeof(m));
	return true;
}

void Server::_registerToMainServer()
{

}

void Server::close()
{
	this->_opened = false;
}
