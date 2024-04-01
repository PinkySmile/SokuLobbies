#include <atomic>
#include <windows.h>
#include <cstddef>
extern bool hasIpv6Map();
extern bool mapIpv6toIpv4(const char * ipv6, char * ipv4, size_t len);
extern void getIpv6Loop(std::atomic_bool& _open);