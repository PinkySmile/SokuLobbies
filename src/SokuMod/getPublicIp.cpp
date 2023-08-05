//
// Created by PinkySmile on 04/11/2020.
//

#include "getPublicIp.hpp"
#include "Exceptions.hpp"
#include "Socket.hpp"
#include "data.hpp"

static char *myIp = nullptr;
static char buffer[64];
static wchar_t buffer2[64];

const char *getMyIp()
{
	GetPrivateProfileStringW(L"Lobby", L"HostIP", L"", buffer2, sizeof(buffer2) / sizeof(*buffer2), profilePath);
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

		if (response.returnCode != 200)
			throw HTTPErrorException(response);
		myIp = strdup(response.body.c_str());
		printf("My ip is %s\n", myIp);
		return myIp;
	} catch (NetworkException &e) {
		printf("Error: %s\n", e.what());
		throw;
	}
}