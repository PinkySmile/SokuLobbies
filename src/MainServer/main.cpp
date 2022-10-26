//
// Created by PinkySmile on 09/10/2022.
//

#include <mutex>
#include <thread>
#include <list>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <arpa/inet.h>
#include "Socket.hpp"
#include "GuardedMutex.hpp"

std::mutex mutex;
std::list<struct Entry> entries;
in_addr myIp;

struct Entry {
	Socket s;
	std::thread _netThread = std::thread([this]{
		try {
			while (this->connected) {
				char buffer[3];
				char packet[2 + 4] = {0};
				size_t read = this->s.read(buffer, sizeof(buffer));

				if (read != 3 || buffer[0] > 1) {
					this->s.disconnect();
					this->connected = false;
					return;
				}
				this->last = time(nullptr);
				if (buffer[0] == 0) {
					std::cout << "Update entry" << std::endl;
					this->port = buffer[1] | (buffer[2] << 8);
					continue;
				}

				std::cout << "Send list" << std::endl;

				GuardedMutex m(mutex);

				m.lock();
				for (auto &entry : entries) {
					if (entry.port) {
						auto remote = entry.s.getRemote();
                                                auto ip = reinterpret_cast<char *>(&remote.sin_addr);

						if (ip[0] == 127 && ip[1] == 0 && ip[2] == 0 && ip[3] == 1)
							ip = reinterpret_cast<char *>(&myIp);
						packet[0] = entry.port & 0xFF;
						packet[1] = entry.port >> 8;
						packet[2] = ip[0];
						packet[3] = ip[1];
						packet[4] = ip[2];
						packet[5] = ip[3];
						try {
							this->s.send(&packet, sizeof(packet));
						} catch (...) {}
					}
				}
				m.unlock();
				memset(packet, 0, sizeof(packet));
				this->s.send(&packet, sizeof(packet));
			}
		} catch (std::exception &e) {
			this->connected = false;
		}
	});
	unsigned short port = 0;
	bool connected = true;
	time_t last = time(nullptr);

	Entry(Socket &s) :
		s(s)
	{
		std::cout << "New connection from " << inet_ntoa(s.getRemote().sin_addr) << std::endl;
	}

	~Entry()
	{
		std::cout << inet_ntoa(s.getRemote().sin_addr) << " disconnected" << std::endl;
		if (this->s.isOpen())
			this->s.disconnect();
		this->connected = false;
		if (this->_netThread.joinable())
			this->_netThread.join();
	}
};

void getIp()
{
	puts("Fetching public IP");

	Socket sock;
	Socket::HttpRequest request{
		/*.httpVer*/ "HTTP/1.1",
		/*.body   */ "",
		/*.method */ "GET",
		/*.host   */ "www.sfml-dev.org",
		/*.portno */ 80,
		/*.header */ {},
		/*.path   */ "/ip-provider.php",
	};
	auto response = sock.makeHttpRequest(request);

	if (inet_aton(response.body.c_str(), &myIp) == -1)
		throw std::runtime_error(response.body);
	printf("My ip is %s\n", inet_ntoa(myIp));
}

int main(int argc, char **argv)
{
	getIp();

	unsigned short port = 5254;
	Socket sock;
	std::thread thread{[]{
		while (true) {
			GuardedMutex m{mutex};

			m.lock();
			for (auto &entry : entries) {
				if (time(nullptr) - entry.last > 30) {
					entry.connected = false;
					entry.s.disconnect();
				}
			}
			entries.remove_if([](const Entry &e) { return !e.connected; });
			m.unlock();
			std::this_thread::sleep_for(std::chrono::seconds(10));
		}
	}};

	if (argc >= 2)
		port = std::stoul(argv[1]);
	sock.bind(port);
	while (true) {
		auto s = sock.accept();
		GuardedMutex m(mutex);

		m.lock();
		entries.emplace_back(s);
		m.unlock();
	}
}
