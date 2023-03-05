#include <iostream>
#include <mutex>
#include <csignal>
#include "Server.hpp"

std::mutex logMutex;
static Server *serv = nullptr;

void s(int)
{
	if (serv)
		serv->close();
	serv = nullptr;
}

void displayHelp(const char *prog)
{
	std::cout << prog << " <port> <max players> <name> [password]" << std::endl;
}

int main(int argc, const char *argv[])
{
	if (argc == 5) {
		for (const char *p = argv[4]; *p; p++)
			if (*p >= 127 || *p < ' ') {
				std::cout << "Your password may only contain english characters and symbols." << std::endl;
				return EXIT_FAILURE;
			}
		if (strlen(argv[4]) > sizeof(Lobbies::PacketHello::password)) {
			std::cout << "Your password must be shorter than " << sizeof(Lobbies::PacketHello::password) << " characters." << std::endl;
			return EXIT_FAILURE;
		}
	} else if (argc != 4) {
		displayHelp(argv[0]);
		return EXIT_FAILURE;
	}
	signal(SIGINT, s);

	Server server;

	serv = &server;
	server.run(std::stoul(argv[1]), std::stoul(argv[2]), argv[3], argv[4]);
	return EXIT_SUCCESS;
}
