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
#include <fstream>
#ifdef _WIN32
#include <shlwapi.h>
#endif
#include "Server.hpp"

Server::Server()
{
	std::ifstream stream{"slurs.txt"};
	std::string line;

	if (!stream)
		return;

	while (std::getline(stream, line))
		this->_bannedWords.push_back(line);
}

Server::~Server()
{
	this->_opened = false;
	if (this->_mainServerThread.joinable())
		this->_mainServerThread.join();
}

void Server::run(unsigned short port, unsigned maxPlayers, const std::string &name)
{
#ifndef _DEBUG
	try {
#endif
		auto socket = std::make_unique<sf::TcpSocket>();

		this->_port = port;
		this->_infos.maxPlayers = maxPlayers;
		this->_infos.currentPlayers = 0;
		this->_infos.name = name;
	#ifndef _LOBBYNOLOG
		logMutex.lock();
		std::cout << "Listening on port " << port << std::endl;
		logMutex.unlock();
	#endif
		if (this->_listener.listen(port) != sf::Socket::Done)
			return;
		this->_listener.setBlocking(false);
		this->_registerToMainServer();
		while (this->_opened) {
			if (sf::Socket::Done == this->_listener.accept(*socket)) {
				this->_connectionsMutex.lock();
			#ifndef _LOBBYNOLOG
				logMutex.lock();
				std::cout << "New conenction from " << socket->getRemoteAddress().toString() << ":" << socket->getRemotePort() << std::endl;
				logMutex.unlock();
			#endif
				this->_connections.emplace_back(new Connection(socket));
				this->_prepareConnectionHandlers(*this->_connections.back());
				this->_connectionsMutex.unlock();
				socket = std::make_unique<sf::TcpSocket>();
			} else
				std::this_thread::sleep_for(std::chrono::milliseconds(100));

			this->_connectionsMutex.lock();
			this->_connections.erase(std::remove_if(this->_connections.begin(), this->_connections.end(), [](std::shared_ptr<Connection> c) {
				return !c->isConnected();
			}), this->_connections.end());
			this->_connectionsMutex.unlock();
		}
#ifndef _DEBUG
	} catch (std::exception &e) {
		this->_listener.close();
		this->_connectionsMutex.lock();
		for (auto &c : this->_connections)
			c->kick("Internal server error: " + std::string(e.what()));
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
		c->kick("Server closed");
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
			if (c->getId() == id) {
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
			if (c->getId() != id && c->isInit())
				c->send(&move, sizeof(move));
		this->_connectionsMutex.unlock();
	};
	connection.onPosition = [this, id](uint32_t x, uint32_t y){
		Lobbies::PacketPosition pos{id, x, y};

		this->_connectionsMutex.lock();
		for (auto &c : this->_connections)
			if (c->getId() != id && c->isInit())
				c->send(&pos, sizeof(pos));
		this->_connectionsMutex.unlock();
	};
	connection.onDisconnect = [this, &connection](const std::string &reason){
		if (!connection.isInit())
			return;

		Lobbies::PacketMessage msg{0xFF00FF, 0, connection.getName() + reason};
		Lobbies::PacketPlayerLeave leave{connection.getId()};

		this->_connectionsMutex.lock();
		for (auto &c : this->_connections)
			if (c->isInit()) {
				c->send(&msg, sizeof(msg));
				if (&*c != &connection)
					c->send(&leave, sizeof(leave));
			}
		this->_infos.currentPlayers--;
		this->_connectionsMutex.unlock();
		if (connection.getBattleStatus()) {
			this->_machinesMutex.lock();

			auto &machine = this->_machines[connection.getActiveMachine()];

			// There is no need to check if the vector is of size >= 2
			// because there is no way that the connection is not in the list if the battle status is not 0
			if (machine[0] == &connection || machine[1] == &connection) {
				// This is one of the players, so we boot everyone out
				for (auto &c1 : machine) {
					Lobbies::PacketArcadeLeave packet{c1->getId()};

					this->_connectionsMutex.lock();
					for (auto &c2 : this->_connections)
						if (c2->getId() != connection.getId() && c2->isInit())
							c2->send(&packet, sizeof(packet));
					c1->setNotPlaying();
					this->_connectionsMutex.unlock();
				}
				machine.clear();
			} else
				machine.erase(std::find(machine.begin(), machine.end(), &connection));
			this->_machinesMutex.unlock();
		}
	};
	connection.onJoin = [this, &connection, id](const Lobbies::PacketHello &packet, std::string ip, std::string &name){
		if (this->_infos.maxPlayers == this->_infos.currentPlayers) {
			connection.kick("Server is full");
			return false;
		}

		auto it = std::find_if(this->_banList.begin(), this->_banList.end(), [&connection, &ip](BanEntry entry){
			return entry.ip == ip || memcmp(entry.uniqueId, connection.getUniqueId(), sizeof(entry.uniqueId)) == 0;
		});

		if (it != this->_banList.end()) {
			connection.kick("You are banned from this server");
			return false;
		}
		name = this->_sanitizeName(std::string(packet.name, strnlen(packet.name, sizeof(packet.name))), &connection);

		Lobbies::PacketOlleh res{name, id};
		Lobbies::PacketPlayerJoin join{id, name, packet.custom};
		Lobbies::PacketMessage msgPacket{0xFF00FF, 0, connection.getName() + " has joined the lobby."};

		connection.send(&res, sizeof(res));
		connection.send(&msgPacket, sizeof(msgPacket));
		this->_connectionsMutex.lock();
		for (auto &c : this->_connections)
			if (c->isInit()) {
				Lobbies::PacketPlayerJoin join2{c->getId(), c->getName(), c->getPlayer()};
				Lobbies::PacketPosition pos{c->getId(), c->getPos().x, c->getPos().y};
				Lobbies::PacketMove move{c->getId(), c->getDir()};

				c->send(&join, sizeof(join));
				c->send(&msgPacket, sizeof(msgPacket));
				connection.send(&join2, sizeof(join2));
				connection.send(&pos, sizeof(pos));
				connection.send(&move, sizeof(move));
				if (connection.getBattleStatus()) {
					Lobbies::PacketArcadeEngage engage{c->getId()};

					connection.send(&engage, sizeof(engage));
				}
			}
		this->_infos.currentPlayers++;
		this->_connectionsMutex.unlock();
		return true;
	};
	connection.onMessage = [this, &connection, id](uint8_t channel, const std::string &msg){
		if (this->_processCommands(connection, msg))
			return;

		auto realMessage = msg;

		for (size_t i = 0; i < realMessage.size(); i++) {
			if (realMessage[i] == '<')
				realMessage[i] = '{';
			if (realMessage[i] == '>')
				realMessage[i] = '}';
		}

		Lobbies::PacketMessage msgPacket{channel, id, "[" + connection.getName() + "]: " + realMessage};

		for (auto &word : this->_bannedWords) {
			auto pos = msg.find(word);

			if (pos == std::string::npos)
				continue;
			if (pos != 0 && isalpha(msg[pos - 1]))
				continue;
			if (pos != msg.size() - word.size() && isalpha(msg[pos + word.size()]))
				continue;
			printf("Message from %s has been shadow banned (%s)\n", connection.getName().c_str(), realMessage.c_str());
			connection.send(&msgPacket, sizeof(msgPacket));
			return;
		}

		this->_connectionsMutex.lock();
		for (auto &c : this->_connections)
			if (c->isInit())
				c->send(&msgPacket, sizeof(msgPacket));
		this->_connectionsMutex.unlock();
	};
	connection.onSettingsUpdate = [this, id](const Lobbies::PacketSettingsUpdate &settings){
		Lobbies::PacketSettingsUpdate packet{id, settings.custom, {}};

		this->_connectionsMutex.lock();
		for (auto &c : this->_connections)
			if (c->isInit())
				c->send(&packet, sizeof(packet));
		this->_connectionsMutex.unlock();
	};
	connection.onGameStart = [&connection, this](const Connection::Room &room){
		Lobbies::PacketGameStart packet{room.ip, room.port, false};

		this->_machinesMutex.lock();
		auto &machine = this->_machines[connection.getActiveMachine()];

		connection.setPlaying();
		for (size_t i = 0; i < machine.size(); i++)
			if (machine[i] != &connection) {
				packet.spectator = i >= 2;
				machine[i]->send(&packet, sizeof(packet));
			}
		this->_machinesMutex.unlock();
	};
	connection.onGameRequest = [&connection, this](){
		this->_onPlayerJoinArcade(connection, connection.getActiveMachine());
	};
	connection.onArcadeLeave = [&connection, this](){
		if (!connection.getBattleStatus())
			return;
		this->_machinesMutex.lock();

		Lobbies::PacketMessage msgPacket{0x00FFFF, 0, "You left arcade " + std::to_string(connection.getActiveMachine()) + "."};
		auto &machine = this->_machines[connection.getActiveMachine()];

		connection.send(&msgPacket, sizeof(msgPacket));
		// There is no need to check if the vector is of size >= 2
		// because there is no way that the connection is not in the list if the battle status is not 0
		if (machine[0] == &connection || machine[1] == &connection) {
			// This is one of the players, so we boot everyone out
			for (auto &c1 : machine) {
				Lobbies::PacketArcadeLeave packet{c1->getId()};

				this->_connectionsMutex.lock();
				for (auto &c2 : this->_connections)
					if (c2->isInit())
						c2->send(&packet, sizeof(packet));
				c1->setNotPlaying();
				this->_connectionsMutex.unlock();
			}
			machine.clear();
		} else
			machine.erase(std::find(machine.begin(), machine.end(), &connection));
		this->_machinesMutex.unlock();
	};
	connection.setId(id);
	connection.startThread();
}

std::string Server::_sanitizeName(const std::string &name, const Connection *con)
{
	unsigned nb = 0;
	std::string result = name;

	if (result.empty()) {
		result = name + std::to_string(nb);
		nb++;
	}
loop:
	this->_connectionsMutex.lock();
	for (auto &c : this->_connections)
		if (&*c != con && c->getName() == result) {
			result = name + std::to_string(nb);
			nb++;
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
	Lobbies::PacketGameRequest packet{0};

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
	if (p1settings == p2settings && p1settings == Lobbies::HOSTPREF_HOST_ONLY) {
		Lobbies::PacketImportantMessage msg{
			"<color FF0000>"
				"Error: Cannot start the game because both you and your opponent have their hosting preference set to 'Host only'."
			"</color>"
		};
		Lobbies::PacketArcadeLeave leave{machine[1]->getId()};

		machine[0]->send(&msg, sizeof(msg));
		machine[1]->send(&msg, sizeof(msg));
		machine[1]->send(&leave, sizeof(leave));
		return false;
	}
	if (p1settings >= Lobbies::HOSTPREF_NO_PREF) {
		machine[p2settings == Lobbies::HOSTPREF_HOST_ONLY]->send(&packet, sizeof(packet));
		return true;
	}
	if (p2settings >= Lobbies::HOSTPREF_NO_PREF) {
		machine[p1settings == Lobbies::HOSTPREF_CLIENT_ONLY]->send(&packet, sizeof(packet));
		return true;
	}
	machine[p1settings != Lobbies::HOSTPREF_HOST_ONLY]->send(&packet, sizeof(packet));
	return true;
}

bool Server::_processCommands(Connection &author, const std::string &msg)
{
	if (msg.empty() || msg.front() != '/')
		return false;

	auto parsed = this->_parseCommand(msg.substr(1));
	auto it = Server::_commands.find(parsed[0]);

	if (it == Server::_commands.end()) {
		Lobbies::PacketMessage m{0xFF0000, 0, "Unknown command \"" + parsed[0]+ "\"<br>Use /help for a list of command"};

		author.send(&m, sizeof(m));
		return true;
	}
	parsed.erase(parsed.begin());
	(this->*it->second.callback)(author, parsed);
	return true;
}

void Server::_registerToMainServer()
{
#ifndef _LOBBYNOLOG
	logMutex.lock();
	std::cout << "Registering lobby '" << this->_infos.name << "' to the server: " << static_cast<int>(this->_infos.maxPlayers) << " max slots" << std::endl;
	logMutex.unlock();
#endif
	this->_mainServerThread = std::thread([this]{
		sf::TcpSocket socket;
		unsigned short servPort;
		char packet[3];
		char buffer[64];

		packet[0] = 0;
		packet[1] = this->_port & 0xFF;
		packet[2] = this->_port >> 8;
#ifdef _WIN32
		GetPrivateProfileString("Lobby", "Host", "pinkysmile.fr", buffer, sizeof(buffer), "./SokuLobbies.ini");
		servPort = GetPrivateProfileInt("Lobby", "Port", 5254, "./SokuLobbies.ini");
#else
		// This is a dirty hack for the main server since it's running on linux
		// and only need to talk to the main server on the loopback
		// but it would be better to use real config files instead
		strcpy(buffer, "localhost");
		servPort = 5254;
#endif
		std::cout << "Main server is " << buffer << ":" << servPort << std::endl;
		socket.connect(buffer, servPort);
		while (this->_opened) {
			if (socket.send(&packet, sizeof(packet)) == sf::Socket::Disconnected)
				socket.connect(buffer, servPort);
			for (int i = 0; i < 100 && this->_opened; i++)
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	});
}

void Server::close()
{
	this->_opened = false;
}

void Server::_onPlayerJoinArcade(Connection &connection, unsigned int aid)
{
	if (connection.getBattleStatus())
		return;

	Lobbies::PacketMessage msgPacket{0x00FFFF, 0, "You joined arcade " + std::to_string(aid) + "."};
	Lobbies::PacketArcadeEngage engage{connection.getId()};

	connection.setActiveMachine(aid);
	this->_machinesMutex.lock();
	auto &machine = this->_machines[aid];

	connection.send(&msgPacket, sizeof(msgPacket));
	if (machine.size() >= 2) {
		if (machine[0]->getBattleStatus() == 2) {
			Lobbies::PacketGameStart packet{machine[0]->getRoomInfo().ip, machine[0]->getRoomInfo().port, true};

			connection.send(&packet, sizeof(packet));
		} else if (machine[1]->getBattleStatus() == 2) {
			Lobbies::PacketGameStart packet{machine[1]->getRoomInfo().ip, machine[1]->getRoomInfo().port, true};

			connection.send(&packet, sizeof(packet));
		}
	}
	machine.push_back(&connection);
	if (machine.size() == 2 && !this->_startRoom(machine)) {
		this->_machinesMutex.unlock();
		return;
	}
	this->_machinesMutex.unlock();

	this->_connectionsMutex.lock();
	for (auto &c : this->_connections)
		if (c->getId() != connection.getId() && c->isInit())
			c->send(&engage, sizeof(engage));
	this->_connectionsMutex.unlock();
}

std::vector<std::string> Server::_parseCommand(const std::string &msg)
{
	std::string token;
	std::vector<std::string> result;
	bool q = false;
	bool sq = false;
	bool esc = false;

	for (auto c : msg) {
		if (esc) {
			token += c;
			esc = false;
		} else if (c == '\\')
			esc = true;
		else if (c == '"' && !sq)
			q = !q;
		else if (c == '\'' && !q)
			sq = !sq;
		else if (!isspace(c) || q || sq)
			token += c;
		else {
			result.push_back(token);
			token.clear();
		}
	}
	result.push_back(token);
	return result;
}

Connection *Server::_findPlayer(uint32_t id)
{
	this->_connectionsMutex.lock();
	for (auto &c : this->_connections)
		if (c->getId() == id) {
			this->_connectionsMutex.unlock();
			if (c->isInit())
				return &*c;
			return nullptr;
		}
	this->_connectionsMutex.unlock();
	return nullptr;
}

Connection *Server::_findPlayer(const std::string &name)
{
	this->_connectionsMutex.lock();
	for (auto &c : this->_connections)
		if (c->getName() == name) {
			this->_connectionsMutex.unlock();
			if (c->isInit())
				return &*c;
			return nullptr;
		}
	this->_connectionsMutex.unlock();
	return nullptr;
}

const std::map<std::string, Server::Cmd> Server::_commands{
	{"help",    {"[command]", "Displays list of commands.<br>Example:<br>/help<br>/help help", &Server::_helpCmd}},
	{"join",    {"(player_id)|@(player_name)", "Join an arcade machine. The id must be in the range 0 to 4294967295<br>Example:<br>/join 1<br>/join @PinkySmile", &Server::_joinCmd}},
	{"list",    {"", "Displays the list of connected players.", &Server::_listCmd}},
	{"locate",  {"(player_id)|@(player_name)", "Locate a player in the field.<br>Example:<br>/locate 1<br>/locate @PinkySmile", &Server::_locateCmd}},
	{"teleport",{"(player_id)|@(player_name)", "Teleports to a player or a location<br>Example:<br>/teleport 10<br>/teleport @PinkySmile", &Server::_teleportCmd}},
};

void Server::_helpCmd(Connection &author, const std::vector<std::string> &args)
{
	std::string msg;

	if (args.empty()) {
		msg = "Available commands:";
		for (auto &cmd : Server::_commands)
			msg += "<br>/" + cmd.first;
	} else {
		auto it = Server::_commands.find(args[0]);

		if (it == Server::_commands.end()) {
			Lobbies::PacketMessage m{0xFF0000, 0, "Unknown command \"" + args[0] + "\""};

			author.send(&m, sizeof(m));
			return;
		}
		msg = "/" + it->first + " " + it->second.usage + ": " + it->second.description;
	}

	Lobbies::PacketMessage m{0xFFFF00, 0, msg};

	author.send(&m, sizeof(m));
}

void Server::_joinCmd(Connection &author, const std::vector<std::string> &args)
{
	if (args.empty()) {
		Lobbies::PacketMessage m{0xFF0000, 0, "Missing argument #1 for command /join. Use /help join for more information"};

		author.send(&m, sizeof(m));
		return;
	}

	if (args[0].front() == '@') {
		auto name = args[0].substr(1);
		auto player = this->_findPlayer(name);

		if (!player) {
			Lobbies::PacketMessage m{0xFF0000, 0, "Cannot find " + name + "."};

			author.send(&m, sizeof(m));
		} else if (!player->getBattleStatus()) {
			Lobbies::PacketMessage m{0xFF0000, 0, name + " is not at an arcade machine."};

			author.send(&m, sizeof(m));
		} else
			this->_onPlayerJoinArcade(author, player->getActiveMachine());
		return;
	}

	uint64_t id;
	try {
		id = std::stoul(args[0]);
		if (id > UINT32_MAX)
			throw std::exception();
	} catch (...) {
		Lobbies::PacketMessage m{0xFF0000, 0, args[0] + " is not a valid number"};

		author.send(&m, sizeof(m));
		return;
	}
	this->_onPlayerJoinArcade(author, id);

	Lobbies::PacketArcadeEngage engage{author.getId()};

	author.send(&engage, sizeof(engage));
}

void Server::_listCmd(Connection &author, const std::vector<std::string> &)
{
	this->_connectionsMutex.lock();

	std::string msg = "There are " + std::to_string(this->_connections.size()) + " players connected.";

	for (auto &c : this->_connections)
		if (c->isInit())
			msg += "<br>" + std::to_string(c->getId()) + ": " + c->getName();
	this->_connectionsMutex.unlock();

	Lobbies::PacketMessage m{0xFFFF00, 0, msg};

	author.send(&m, sizeof(m));
}

void Server::_locateCmd(Connection &author, const std::vector<std::string> &args)
{
	Connection *player;

	if (args[0].front() == '@') {
		auto name = args[0].substr(1);

		player = this->_findPlayer(name);
		if (!player) {
			Lobbies::PacketMessage m{0xFF0000, 0, "Cannot find " + name + "."};

			author.send(&m, sizeof(m));
			return;
		}
	} else {
		uint64_t id;

		try {
			id = std::stoul(args[0]);
			if (id > UINT32_MAX)
				throw std::exception();
		} catch (...) {
			Lobbies::PacketMessage m{0xFF0000, 0, args[0] + " is not a valid number"};

			author.send(&m, sizeof(m));
			return;
		}
		player = this->_findPlayer(id);
		if (!player) {
			Lobbies::PacketMessage m{0xFF0000, 0, "Cannot find player #" + std::to_string(id) + "."};

			author.send(&m, sizeof(m));
			return;
		}
	}

	Lobbies::PacketMessage m{0xFFFF00, 0, player->getName() + " is at x:" + std::to_string(player->getPos().x) + " y:" + std::to_string(player->getPos().y) + "."};

	author.send(&m, sizeof(m));
}

void Server::_teleportCmd(Connection &author, const std::vector<std::string> &args)
{
	Connection *player;

	if (args[0].front() == '@') {
		auto name = args[0].substr(1);

		player = this->_findPlayer(name);
		if (!player) {
			Lobbies::PacketMessage m{0xFF0000, 0, "Cannot find " + name + "."};

			author.send(&m, sizeof(m));
			return;
		}
	} else {
		uint64_t id;

		try {
			id = std::stoul(args[0]);
			if (id > UINT32_MAX)
				throw std::exception();
		} catch (...) {
			Lobbies::PacketMessage m{0xFF0000, 0, args[0] + " is not a valid number"};

			author.send(&m, sizeof(m));
			return;
		}
		player = this->_findPlayer(id);
		if (!player) {
			Lobbies::PacketMessage m{0xFF0000, 0, "Cannot find player #" + std::to_string(id) + "."};

			author.send(&m, sizeof(m));
			return;
		}
	}

	Lobbies::PacketPosition pos{author.getId(), player->getPos().x, player->getPos().y};
	Lobbies::PacketMessage m{0xFFFF00, 0, "Teleporting to " + player->getName() + " at x:" + std::to_string(player->getPos().x) + " y:" + std::to_string(player->getPos().y) + "."};

	author.send(&m, sizeof(m));
	author.send(&pos, sizeof(pos));
	author.send(&pos, sizeof(pos));
	author.send(&pos, sizeof(pos));
	author.send(&pos, sizeof(pos));
}
