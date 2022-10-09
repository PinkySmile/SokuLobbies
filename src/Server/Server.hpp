//
// Created by PinkySmile on 01/10/2022.
//

#ifndef SOKULOBBIES_SERVER_HPP
#define SOKULOBBIES_SERVER_HPP


#include <mutex>
#include <memory>
#include <Packet.hpp>
#include "Connection.hpp"

struct BanEntry {
	char uniqueId[16];
	char profileName[33];
	char ip[22];
};

class Server {
private:
	bool _opened = true;
	unsigned short _port;
	Connection::LobbyInfo _infos;
	sf::TcpListener _listener;
	std::vector<std::shared_ptr<Connection>> _connections;
	std::mutex _connectionsMutex;
	std::mutex _machinesMutex;
	std::vector<BanEntry> _banList;
	std::thread _mainServerThread;
	std::map<uint8_t, std::vector<Connection *>> _machines;

	bool _processCommands(Connection &author, const std::string &msg);
	std::string _sanitizeName(const std::string &name, const Connection *con);
	void _prepareConnectionHandlers(Connection &connection);
	bool _startRoom(const std::vector<Connection *> &machine);
	void _registerToMainServer();

public:
	~Server();
	void run(unsigned short port, unsigned maxPlayers, const std::string &name);
	void close();
};


#endif //SOKULOBBIES_SERVER_HPP
