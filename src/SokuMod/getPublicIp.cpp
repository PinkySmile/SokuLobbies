//
// Created by PinkySmile on 04/11/2020.
//

#include "getPublicIp.hpp"
#include <windows.h>
#include <cstdio>
#include <mutex>
#include <string>
#include "data.hpp"
#include "LobbyData.hpp"

static std::mutex myIpv6Mutex;
static std::string myIpv6("");
static char *myIp = nullptr;
static wchar_t buffer2[256];
static char buffer[sizeof(buffer2) / sizeof(*buffer2)];

const char *getMyIp()
{
	auto bufferSize = sizeof(buffer2)/sizeof(*buffer2);
	GetPrivateProfileStringW(L"Lobby", L"HostIP", L"", buffer2, bufferSize, profilePath);
	if (*buffer2) {
		for (int i = 0; i < sizeof(buffer); i++)
			buffer[i] = buffer2[i];
		printf("Using forced ip %s\n", buffer);
		return buffer;
	}
	if (myIp)
		return myIp;

	puts("Fetching public IP");
	try {
		GetPrivateProfileStringW(L"Lobby", L"GetPublicIpServer", L"http://www.sfml-dev.org/ip-provider.php", buffer2, bufferSize, profilePath);
		// https://curl.se/libcurl/c/CURLOPT_URL.html
		WideCharToMultiByte(CP_ACP, 0, buffer2, -1, buffer, sizeof(buffer), NULL, NULL);

		auto ip = lobbyData->httpRequest(buffer, "GET", "", 16000);

		ip.erase(0, ip.find_first_not_of(" \n\r\t"));
		ip.erase(ip.find_last_not_of(" \n\r\t") + 1);
		myIp = strdup(ip.c_str());
		printf("My ip is %s\n", myIp);
		return myIp;
	} catch (std::exception &e) {
		printf("Error: %s\n", e.what());
		throw;
	}
}

std::string getMyIpv6()
{
	myIpv6Mutex.lock();
	std::string _myIpv6 = myIpv6;
	myIpv6Mutex.unlock();
	return _myIpv6;
}

bool isIpv6Available() {
	myIpv6Mutex.lock();
	bool ret = !myIpv6.empty();
	myIpv6Mutex.unlock();
	return ret;
}

void setMyIpv6(std::string &ipv6) {
	myIpv6Mutex.lock();
	myIpv6 = ipv6;
	myIpv6Mutex.unlock();
}