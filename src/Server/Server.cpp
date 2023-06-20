//
// Created by PinkySmile on 01/10/2022.
//

#include <iostream>
#include <future>
#ifndef _LOBBYNOLOG
#include <mutex>
extern std::mutex logMutex;
#endif
#include <memory>
#include <cstring>
#include <fstream>
#ifdef _WIN32
#include <shlwapi.h>
#endif
#include <future>
#include "Server.hpp"
#include "Utils.hpp"


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

static std::future<std::string> readLine()
{
	std::promise<std::string> promise;
	auto future = promise.get_future();
	std::thread thread([](std::promise<std::string> p){
		std::string line;

		std::cout << "> ";
		std::cout.flush();
		std::getline(std::cin, line);
		p.set_value(line);
	}, std::move(promise));

	thread.detach();
	return future;
}

void Server::run(unsigned short port, unsigned maxPlayers, const std::string &name, const char *password)
{
#ifndef _DEBUG
	try {
#endif
		auto socket = std::make_unique<sf::TcpSocket>();
		auto future = readLine();

		this->_password = password;
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
				std::cout << "New connection from " << socket->getRemoteAddress().toString() << ":" << socket->getRemotePort() << std::endl;
				logMutex.unlock();
			#endif
				this->_connections.emplace_back(new Connection(socket, this->_password));
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
			if (future.wait_for(std::chrono::milliseconds(100)) == std::future_status::ready) {
				this->_processCommands(nullptr, future.get());
				future = readLine();
			}
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

		Lobbies::PacketMessage msg{0xFFFF00, 0, connection.getName() + reason};
		Lobbies::PacketPlayerLeave leave{connection.getId()};

		std::cout << connection.getName() << reason << std::endl;
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
			return entry.ip == connection.getIp().toString();// || memcmp(entry.uniqueId, connection.getUniqueId(), sizeof(entry.uniqueId)) == 0;
		});

		if (it != this->_banList.end()) {
			std::cout << connection.getName() << " (" << connection.getIp() << ") tried to join but is banned (" << it->profileName << " " << it->ip << " " << it->reason << ")." << std::endl;
			connection.kick("You are banned from this server: " + std::string(it->reason, strnlen(it->reason, sizeof(it->reason))));
			return false;
		}
		name = this->_sanitizeName(std::string(packet.name, strnlen(packet.name, sizeof(packet.name))), &connection);

		Lobbies::PacketOlleh res{this->_infos.name, name, id};
		Lobbies::PacketPlayerJoin join{id, name, packet.custom};
		Lobbies::PacketMessage msgPacket{0xFFFF00, 0, connection.getName() + " has joined the lobby."};

		std::cout << connection.getName() << " has joined the lobby." << std::endl;
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
				if (c->getBattleStatus()) {
					Lobbies::PacketArcadeEngage engage{c->getId(), c->getActiveMachine()};

					connection.send(&engage, sizeof(engage));
				}
			}
		this->_infos.currentPlayers++;
		this->_connectionsMutex.unlock();
		return true;
	};
	connection.onMessage = [this, &connection, id](uint8_t channel, const std::string &msg){
		std::cout << "<" << connection.getName() << ">: " << msg << std::endl;
		if (!msg.empty() && msg.front() == '/')
			return this->_processCommands(&connection, msg);

		auto realMessage = "[" + connection.getName() + "]: " + msg;
		Lobbies::PacketMessage msgPacket{channel, id, realMessage};

		for (auto &word : this->_bannedWords) {
			auto pos = msg.find(word);

			if (pos == std::string::npos)
				continue;
			if (pos != 0 && isalpha(msg[pos - 1]))
				continue;
			if (pos != msg.size() - word.size() && isalpha(msg[pos + word.size()]))
				continue;
			std::cout << "Message from " << connection.getName() << " has been shadow banned (" << msg.substr(pos, pos + word.size()) << ")" << std::endl;
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

		Lobbies::PacketArcadeLeave leave{connection.getId()};

		this->_connectionsMutex.lock();
		for (auto &c : this->_connections)
			if (c->getId() != connection.getId() && c->isInit())
				c->send(&leave, sizeof(leave));
		this->_connectionsMutex.unlock();
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

void Server::_processCommands(Connection *author, const std::string &msg)
{
	if (msg.empty())
		return;

	auto parsed = this->_parseCommand(msg.front() == '/' ? msg.substr(1) : msg);
	auto it = Server::_commands.find(parsed.front());

	if (it != Server::_commands.end()) {
		parsed.erase(parsed.begin());
		return (this->*it->second.callback)(author, parsed);
	}
	if (!author || author->getIp() == sf::IpAddress::LocalHost) {
		auto ita = Server::_adminCommands.find(parsed.front());

		if (ita != Server::_adminCommands.end()) {
			parsed.erase(parsed.begin());
			return (this->*ita->second.callback)(author, parsed);
		}
	}
	sendSystemMessageTo(author, "Unknown command \"" + parsed[0]+ "\"\nUse /help for a list of command", 0xFF0000);
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
		// FIXME: This is a dirty hack for the main server since it's running on linux
		//        and only need to talk to the main server on the loopback
		//        but it would be better to use real config files instead
		strcpy(buffer, MAIN_SERVER_HOST);
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
	Lobbies::PacketArcadeEngage engage{connection.getId(), aid};

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
		if (c->isInit())
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
	{"help",    {"[command]", "Displays list of commands.\nExample:\n/help\n/help help", &Server::_helpCmd}},
	{"join",    {"(player_name)", "Join an arcade machine. The id must be in the range 0 to 4294967295\nExample:\n/join 1\n/join @PinkySmile", &Server::_joinCmd}},
	{"list",    {"", "Displays the list of connected players.", &Server::_listCmd}},
	{"locate",  {"(player_name)", "Locate a player in the field.\nExample:\n/locate 1\n/locate @PinkySmile", &Server::_locateCmd}},
	{"msg",     {"(player_name) (message)", "Sends a message privately\nExample:\n/msg @PinkySmile Hello!", &Server::_msgCmd}},
};

const std::map<std::string, Server::Cmd> Server::_adminCommands{
	{"ban",    {"(player_name) (reason)", "Bans a player.\nExample:\n/ban @PinkySmile\n/ban 1", &Server::_banCmd}},
	{"banip",  {"(ip) (reason)", "Bans a player.\nExample:\n/ban @PinkySmile\n/ban 1", &Server::_banipCmd}},
	{"kick",   {"(player_name) (reason)", "Kicks a player.\nExample:\n/ban @PinkySmile\n/ban 1", &Server::_kickCmd}},
	{"say",    {"(message)", "Sends a message as the server.\nExample:\n/say Hello!", &Server::_sayCmd}},
	{"warn",   {"(message)", "Sends an important message to everyone.\nExample:\n/warn The server will close in 5 minutes.", &Server::_warnCmd}},
};

void Server::sendSystemMessageTo(Connection *recipient, const std::string &msg, unsigned color)
{
	if (recipient) {
		Lobbies::PacketMessage m{static_cast<int>(color), 0, msg};

		recipient->send(&m, sizeof(m));
	} else
		std::cout << msg << std::endl;
}

void Server::_helpCmd(Connection *author, const std::vector<std::string> &args)
{
	if (!args.empty()) {
		auto it = Server::_commands.find(args[0]);

		if (it != Server::_commands.end())
			return sendSystemMessageTo(author, "/" + it->first + " " + it->second.usage + ": " + it->second.description, 0xFFFF00);
		if (!author || author->getIp() == sf::IpAddress::LocalHost) {
			auto ita = Server::_adminCommands.find(args[0]);

			if (ita != Server::_adminCommands.end())
				return sendSystemMessageTo(author, "/" + ita->first + " " + ita->second.usage + ": " + ita->second.description, 0xFFFF00);
		}
		return sendSystemMessageTo(author, "Unknown command \"" + args[0] + "\"", 0xFF0000);
	}

	std::string msg;

	msg += "Available commands:";
	for (auto &cmd : Server::_commands) {
		auto tmp = "/" + cmd.first + " " + cmd.second.usage;

		if (msg.size() + tmp.size() < sizeof(Lobbies::PacketMessage::message))
			msg += "\n" + tmp;
		else {
			sendSystemMessageTo(author, msg, 0xFFFF00);
			msg = tmp;
		}
	}
	if (!author || author->getIp() == sf::IpAddress::LocalHost)
		for (auto &cmd : Server::_adminCommands) {
			auto tmp = "/" + cmd.first + " " + cmd.second.usage;

			if (msg.size() + tmp.size() < sizeof(Lobbies::PacketMessage::message))
				msg += "\n" + tmp;
			else {
				sendSystemMessageTo(author, msg, 0xFFFF00);
				msg = tmp;
			}
		}
	sendSystemMessageTo(author, msg, 0xFFFF00);
}

void Server::_joinCmd(Connection *author, const std::vector<std::string> &args)
{
	if (args.empty())
		return sendSystemMessageTo(author, "Missing argument #1 for command /join. Use /help join for more information", 0xFF0000);
	if (!author)
		return sendSystemMessageTo(author, "Can only be used in a lobby", 0xFF0000);

	auto name = args.front();
	auto player = this->_findPlayer(name);

	if (!player)
		sendSystemMessageTo(author, "Cannot find " + name + ".", 0xFF0000);
	else if (!player->getBattleStatus())
		sendSystemMessageTo(author, name + " is not at an arcade machine.", 0xFF0000);
	else
		this->_onPlayerJoinArcade(*author, player->getActiveMachine());
}

void Server::_listCmd(Connection *author, const std::vector<std::string> &)
{
	this->_connectionsMutex.lock();

	std::vector<Connection *> players;

	for (auto &c : this->_connections)
		if (c->isInit())
			players.push_back(&*c);
	std::sort(players.begin(), players.end(), [](Connection *a, Connection *b){
		return a->getId() < b->getId();
	});

	std::string msg = "There are " + std::to_string(players.size()) + " players connected.";

	for (auto &c : players)
		msg += "\n" + std::to_string(c->getId()) + ": " + c->getName();
	this->_connectionsMutex.unlock();
	sendSystemMessageTo(author, msg, 0xFFFF00);
}

void Server::_locateCmd(Connection *author, const std::vector<std::string> &args)
{
	if (args.empty())
		return sendSystemMessageTo(author, "Missing argument #1 for command /locate. Use /help locate for more information", 0xFF0000);

	auto name = args.front();
	auto player = this->_findPlayer(name);

	if (!player)
		return sendSystemMessageTo(author, "Cannot find " + name + ".", 0xFF0000);
	sendSystemMessageTo(author, player->getName() + " is at x:" + std::to_string(player->getPos().x) + " y:" + std::to_string(player->getPos().y) + ".", 0xFFFF00);
}

void Server::_teleportCmd(Connection *author, const std::vector<std::string> &args)
{
	if (!author)
		return sendSystemMessageTo(author, "Can only be used in a lobby", 0xFF0000);
	if (args.empty())
		return sendSystemMessageTo(author, "Missing argument #1 for command /teleport. Use /help teleport for more information", 0xFF0000);

	auto name = args.front();
	auto player = this->_findPlayer(name);

	if (!player)
		return sendSystemMessageTo(author, "Cannot find " + name + ".", 0xFF0000);

	Lobbies::PacketPosition pos{author->getId(), player->getPos().x, player->getPos().y};

	sendSystemMessageTo(author, "Teleporting to " + player->getName() + " at x:" + std::to_string(player->getPos().x) + " y:" + std::to_string(player->getPos().y) + ".", 0xFFFF00);
	author->send(&pos, sizeof(pos));
	author->send(&pos, sizeof(pos));
	author->send(&pos, sizeof(pos));
	author->send(&pos, sizeof(pos));
}

void Server::_msgCmd(Connection *author, const std::vector<std::string> &args)
{
	if (!author)
		return sendSystemMessageTo(author, "Can only be used in a lobby", 0xFF0000);
	if (args.empty())
		return sendSystemMessageTo(author, "Missing argument #1 for command /teleport. Use /help teleport for more information", 0xFF0000);

	auto name = args.front();
	auto player = this->_findPlayer(name);
	auto msg = join(args.begin() + 1, args.end(), ' ');

	if (!player)
		return sendSystemMessageTo(author, "Cannot find " + name + ".", 0xFF0000);

	auto realMessage1 = "[from " + (author ? author->getName() : std::string("*CONSOLE*")) + "]: " + msg;
	auto realMessage2 = "[to " + player->getName() + "]: " + msg;

	for (char &i : realMessage1) {
		if (i == '<')
			i = '{';
		if (i == '>')
			i = '}';
	}
	for (char &i : realMessage2) {
		if (i == '<')
			i = '{';
		if (i == '>')
			i = '}';
	}

	Lobbies::PacketMessage msgPacket1{-1, author ? author->getId() : 0, realMessage1};
	Lobbies::PacketMessage msgPacket2{-1, author ? author->getId() : 0, realMessage2};

	std::cout << '[' << (author ? author->getName() : std::string("*CONSOLE*")) << " -> " << player->getName() << "]: " << msg << std::endl;
	if (author)
		author->send(&msgPacket2, sizeof(msgPacket2));
	for (auto &word : this->_bannedWords) {
		auto pos = msg.find(word);

		if (pos == std::string::npos)
			continue;
		if (pos != 0 && isalpha(msg[pos - 1]))
			continue;
		if (pos != msg.size() - word.size() && isalpha(msg[pos + word.size()]))
			continue;
		std::cout << "Private message from " << (author ? author->getName() : std::string("*CONSOLE*")) << " to " << player->getName() << " has been shadow banned (" << msg.substr(pos, pos + word.size()) << ")" << std::endl;
		return;
	}
	player->send(&msgPacket1, sizeof(msgPacket1));
}

void Server::_banCmd(Connection *author, const std::vector<std::string> &args)
{
	if (args.empty())
		return sendSystemMessageTo(author, "Missing argument #1 for command /banip. Use /help banip for more information", 0xFF0000);

	auto reason = args.size() == 1 ? "Banned by an operator" : join(args.begin() + 1, args.end(), ' ');
	Connection *player = this->_findPlayer(args[0]);

	if (!player)
		return sendSystemMessageTo(author, "Cannot find " + args[0] + ".", 0xFF0000);
	this->_banList.emplace_back();

	auto &entry = this->_banList.back();

	memset(&entry, 0, sizeof(entry));
	sendSystemMessageTo(author, "Banned " + player->getName(), 0xFFFF00);
	strncpy(entry.ip, player->getIp().toString().c_str(), sizeof(entry.ip));
	strncpy(entry.profileName, player->getRealName().c_str(), sizeof(entry.profileName));
	strncpy(entry.reason, reason.c_str(), sizeof(entry.reason));
	player->kick(reason);
}

void Server::_banipCmd(Connection *author, const std::vector<std::string> &args)
{
	if (args.empty())
		return sendSystemMessageTo(author, "Missing argument #1 for command /banip. Use /help banip for more information", 0xFF0000);

	auto ip = sf::IpAddress(args.front());
	auto reason = args.size() == 1 ? "Banned by an operator" : join(args.begin() + 1, args.end(), ' ');
	Connection *player = nullptr;
	std::string name;

	if (ip == sf::IpAddress::None)
		return sendSystemMessageTo(author, "Invalid ip provided", 0xFF0000);

	auto it = std::find_if(this->_banList.begin(), this->_banList.end(), [ip](BanEntry &entry){
		return ip.toString() == entry.ip;
	});

	if (it == this->_banList.end()) {
		this->_banList.emplace_back();
		auto &entry = this->_banList.back();
		sendSystemMessageTo(author, "Banned " + ip.toString(), 0xFFFF00);
		memset(&entry, 0, sizeof(entry));
		strncpy(entry.ip, ip.toString().c_str(), sizeof(entry.ip));
		strncpy(entry.profileName, ip.toString().c_str(), sizeof(entry.profileName));
		strncpy(entry.reason, reason.c_str(), sizeof(entry.reason));
	} else {
		sendSystemMessageTo(author, "Updated entry for " + std::string(it->profileName), 0xFFFF00);
		strncpy(it->reason, reason.c_str(), sizeof(it->reason));
	}

	std::vector<Connection *> toKick;

	this->_connectionsMutex.lock();
	for (auto &c : this->_connections)
		if (c->getIp() == ip && c->isInit())
			toKick.push_back(&*c);
	this->_connectionsMutex.unlock();
	for (auto c : toKick)
		c->kick(reason);
}

void Server::_kickCmd(Connection *author, const std::vector<std::string> &args)
{
	if (args.empty())
		return sendSystemMessageTo(author, "Missing argument #1 for command /banip. Use /help banip for more information", 0xFF0000);

	auto reason = args.size() == 1 ? "Kicked by an operator" : join(args.begin() + 1, args.end(), ' ');
	Connection *player = this->_findPlayer(args[0]);

	if (!player)
		return sendSystemMessageTo(author, "Cannot find " + args[0] + ".", 0xFF0000);
	player->kick(reason);
}

void Server::_sayCmd(Connection *author, const std::vector<std::string> &args)
{
	auto realMessage = "[*CONSOLE*]: " + join(args.begin(), args.end(), ' ');

	for (char &i : realMessage) {
		if (i == '<')
			i = '{';
		if (i == '>')
			i = '}';
	}

	Lobbies::PacketMessage msgPacket{-1, 0, realMessage};

	std::cout << realMessage << std::endl;
	for (auto &word : this->_bannedWords) {
		auto pos = realMessage.find(word);

		if (pos == std::string::npos)
			continue;
		if (pos != 0 && isalpha(realMessage[pos - 1]))
			continue;
		if (pos != realMessage.size() - word.size() && isalpha(realMessage[pos + word.size()]))
			continue;
		std::cout << "Message from " << (author ? author->getName() : std::string("*CONSOLE*")) << " has been shadow banned (" << realMessage.substr(pos, pos + word.size()) << ")" << std::endl;
		if (author)
			author->send(&msgPacket, sizeof(msgPacket));
		return;
	}

	this->_connectionsMutex.lock();
	for (auto &c : this->_connections)
		if (c->isInit())
			c->send(&msgPacket, sizeof(msgPacket));
	this->_connectionsMutex.unlock();
}

void Server::_warnCmd(Connection *author, const std::vector<std::string> &args)
{
	auto realMessage = join(args.begin(), args.end(), ' ');

	for (char &i : realMessage) {
		if (i == '<')
			i = '{';
		if (i == '>')
			i = '}';
	}

	Lobbies::PacketMessage msgPacket{0xFFFF00, 0, realMessage};
	Lobbies::PacketImportantMessage msgPacket2{realMessage};

	std::cout << realMessage << std::endl;
	this->_connectionsMutex.lock();
	for (auto &c : this->_connections)
		if (c->isInit()) {
			c->send(&msgPacket, sizeof(msgPacket));
			c->send(&msgPacket2, sizeof(msgPacket2));
		}
	this->_connectionsMutex.unlock();
}
