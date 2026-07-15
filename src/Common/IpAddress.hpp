#pragma once

#include <string>
#include <arpa/inet.h>
#include <netdb.h>
#include <cstring>
#include <stdexcept>

class IpAddress {
public:
    IpAddress(const std::string& address);
    IpAddress(const char* address);
    IpAddress(u_int8_t byte0, u_int8_t byte1, u_int8_t byte2, u_int8_t byte3);
    std::string toString() const;
    
    static IpAddress resolve(const char* domain);
    static bool validIp(const char* address);

    bool operator==(IpAddress &r);
private:
    std::string _ipv4;
};