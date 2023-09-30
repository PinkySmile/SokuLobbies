//
// Created by PinkySmile on 04/11/2020.
//

#ifndef HISOUTENSOKUDISCORDINTEGRATION_GETPUBLICIP_HPP
#define HISOUTENSOKUDISCORDINTEGRATION_GETPUBLICIP_HPP
#include <string>

const char *getMyIp();
std::string getMyIpv6();
bool isIpv6Available();
void setMyIpv6(std::string &ipv6);

#endif // HISOUTENSOKUDISCORDINTEGRATION_GETPUBLICIP_HPP
