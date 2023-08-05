#include <iostream>
#include <mutex>
#include <csignal>
#include <cstring>
#include "Server.hpp"

std::mutex logMutex;
static Server *serv = nullptr;

void s(int)
{
	if (serv)
		serv->close();
	serv = nullptr;
}

#ifdef _UTF16
#include <encodingConverter.hpp>

void displayHelp(const wchar_t *prog)
{
	std::cout << prog << " <port> <max players> <name> [password]" << std::endl;
}

int wmain(int argc, const wchar_t *argv[])
{
	if (argc == 5) {
		for (const wchar_t *p = argv[4]; *p; p++)
			if (*p >= 127 || *p < ' ') {
				std::cout << "Your password may only contain english characters and symbols." << std::endl;
				return EXIT_FAILURE;
			}
		if (wcslen(argv[4]) > sizeof(Lobbies::PacketHello::password)) {
			std::cout << "Your password must be shorter than " << sizeof(Lobbies::PacketHello::password) << " characters." << std::endl;
			return EXIT_FAILURE;
		}
		std::cout << "Password is '" << argv[4] << "'" << std::endl;
	} else if (argc != 4) {
		displayHelp(argv[0]);
		return EXIT_FAILURE;
	}
	signal(SIGINT, s);

	Server server;
	char pwd[sizeof(Lobbies::PacketHello::password) + 1];

	if (argv[4]) {
		memset(pwd, 0, sizeof(pwd));
		for (auto ptr = pwd; *argv[4]; ptr++, argv[4]++)
			*pwd = *argv[4];
	}
	serv = &server;
	server.run(
		std::stoul(argv[1]),
		std::stoul(argv[2]),
		convertEncoding<wchar_t, char, UTF16Decode, UTF8Encode>(argv[3]),
		argv[4] ? pwd : nullptr
	);
	return EXIT_SUCCESS;
}
#else
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
		std::cout << "Password is '" << argv[4] << "'" << std::endl;
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
#endif
