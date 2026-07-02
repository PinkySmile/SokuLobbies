#include "IpAddress.hpp"

IpAddress::IpAddress(const std::string& address) {
    this->_ipv4 = address;
}

IpAddress::IpAddress(const char* address) {
    this->_ipv4 = address;
}

IpAddress::IpAddress(u_int8_t byte0, u_int8_t byte1, u_int8_t byte2, u_int8_t byte3) {
    this->_ipv4 =
        std::to_string(byte0) + ":" +
        std::to_string(byte1) + ":" +
        std::to_string(byte2) + ":" +
        std::to_string(byte3);
}

IpAddress IpAddress::resolve(const char* domain) {
    IpAddress ip = IpAddress(reinterpret_cast<const char*>(gethostbyname(domain)->h_addr));
    return ip;
}

bool IpAddress::validIp(const char* address) {
    return inet_pton(AF_INET, address, (void*)(0));
}

bool IpAddress::operator==(IpAddress &r){
    return this->_ipv4.compare(r._ipv4);
}

std::string IpAddress::toString() const {
    return this->_ipv4;
};
