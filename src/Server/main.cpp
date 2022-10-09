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
	std::cout << prog << " <port> <max players> <name>" << std::endl;
}

int main(int argc, const char *argv[])
{
	if (argc != 4) {
		displayHelp(argv[0]);
		return EXIT_FAILURE;
	}
	signal(SIGINT, s);

	Server server;

	serv = &server;
	server.run(std::stoul(argv[1]), std::stoul(argv[2]), argv[3]);
	return EXIT_SUCCESS;
}
