#include "ipv6map.h"
#include "ipv6map_extern.hpp"
#include "getPublicIp.hpp"
#include <chrono>
#include <thread>
#include <ws2tcpip.h>

#define IPV6_ADDR(a, b, c, d, e, f, g, h) \
	{ ntohs((a)), ntohs((b)), ntohs((c)), ntohs((d)), ntohs((e)), ntohs((f)), ntohs((g)), ntohs((h)) }
#define DEFAULT_RELAY_IP IPV6_ADDR(0x2408, 0x400d, 0x1009, 0x3100, 0xf090, 0xee91, 0x3089, 0x2982)

static sockaddr6to4_t sockaddr6to4;

bool hasIpv6Map(){
    if (sockaddr6to4)
        return true;
    return (sockaddr6to4 = getSockaddr6to4());
}

bool mapIpv6toIpv4(const char * ipv6, char * ipv4, size_t len){
    in6_addr sin6_addr;
    if (inet_pton(AF_INET6, ipv6, &sin6_addr) != 1) {
        return false;
    }
    sockaddr_in ip_;
    return sockaddr6to4(&sockaddr_in6{AF_INET6,0,0,sin6_addr,0}, &ip_) == 0 && inet_ntop(AF_INET, &ip_.sin_addr, ipv4, len);
}

void getIpv6Loop(std::atomic_bool& _open) {
    SOCKET s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET)
        return;
    u_long mode = 1;
    if (ioctlsocket(s, FIONBIO, &mode) != 0)
        return;
    if (bind(s, (sockaddr *)&sockaddr_in6{AF_INET6,0,0,in6addr_any,0}, sizeof(sockaddr_in6)) != 0)
        return;
    uint16_t relay_[8] = DEFAULT_RELAY_IP;
    sockaddr_in6 relay = {AF_INET6,htons(12321),0,*(in6_addr *)relay_,0};
    sockaddr_in6 from;
    const char requireIp[2] = {'6', 6};
    char buf[2+16+2];
    bool gotten = false;
    int timeoutCount = 0;
    printf("Getting IPv6 address\n");
    while(_open){
        if (sendto(s, requireIp, (int)sizeof(requireIp), 0, (const sockaddr *)&relay, (int)sizeof(relay)) == SOCKET_ERROR){
            printf("Error when getting IPv6 address: %u\n", WSAGetLastError());
        }
        int time = (gotten && !timeoutCount) ? 3000 : 500;
        timeoutCount += time;
        for (int i = 0; i<= time / 100; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            int fromlen = sizeof(from);
            if (recvfrom(s, buf, sizeof(buf), 0, (sockaddr *) &from, &fromlen) == sizeof(buf)) {
                if (buf[0] = '6' && buf[1] == 7 && fromlen == sizeof(from) && memcmp(&from.sin6_addr, &relay.sin6_addr, sizeof(in6_addr)) == 0){
                    char myIpv6[46];
                    if (inet_ntop(AF_INET6, buf + 2, myIpv6, sizeof(myIpv6))) {
                        gotten = true;
                        setMyIpv6(std::string(myIpv6));
                        timeoutCount = 0;
                    }
                }
            }
            if (!_open)
                break;
        }
        if (timeoutCount >= 3000 && gotten && _open) {
            gotten = false;
            setMyIpv6(std::string());
        }
    }
    closesocket(s);
}