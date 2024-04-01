#ifndef _IPV6MAP_H_
#define _IPV6MAP_H_
#include <winsock2.h>
#include <windows.h>
#include <in6addr.h>
#include <libloaderapi.h>
#include <ws2ipdef.h>

/**
 * Type of function `sockaddr6to4`. The function can be got from `getSockaddr6to4` if it returns non-zero.
 * `sockaddr6to4` write the IPv6-mapped IPv4 address into out4, and return zero if and only if there is no error.
 * When this function called, a map will be created if the IPv6 address hasn't been mapped, or the
 * mapped IPv4 address will be got from the existing map if the IPv6 address has been mapped.
 * Once a IPv6 address is mapped successfully, we can access it by access the IPv6-mapped IPv4 address.
 * `[::]` will be mapped to `0.0.0.0`, and other IPv6 address will be mapped to `127.127.0.0/16`.
 * The map is stateful, and it is permanent until the process stops. So don't map too many IPv6 addresses.
 * But it is OK to call `sockaddr6to4` on one IPv6 address many times.
 */
typedef int (*sockaddr6to4_t)(const struct sockaddr_in6 *in6, struct sockaddr_in *out4);

/**
 * Type of function `sockaddr4to6`. The function can be got from `getSockaddr4to6` if it returns non-zero.
 * `sockaddr4to6` write the IPv4-mapped IPv6 address into out6, and return zero if and only if there is no error.
 * `0.0.0.0` is mapped to `[::]`, `127.127.0.0/16` is mapped to IPv6 address (failed and return
 * 1 if the address hasn't mapped to a IPv6 address), and other IPv4 address is mapped to IPv4-
 * mapped IPv6 address defined by RFC 6890, i.e. `::ffff:IPv4Address` (such as ::ffff:1.2.3.4).
 */
typedef int (*sockaddr4to6_t)(const struct sockaddr_in *in4, struct sockaddr_in6 *out6);

#define __IPV6MAP_FILENAMES(type, prefix) type{prefix##"IPv6Map.dat", prefix##"IPv6Map.dll"}
#define IPV6MAP_FILENAMES __IPV6MAP_FILENAMES((char *), )
#define IPV6MAP_WFILENAMES __IPV6MAP_FILENAMES((wchar_t *), L)

/**
 * return the handle of loaded IPv6Map DLL. return `NULL` if it hasn't been loaded.
 */
inline HMODULE getIPv6MapDll() {
	char *filenames[] = __IPV6MAP_FILENAMES(,);
	for (int i = 0; i < sizeof(filenames) / sizeof(filenames[0]); i++) {
		HMODULE ipv6map_dll = GetModuleHandle(filenames[i]);
		if (ipv6map_dll && GetProcAddress(ipv6map_dll, "IPv6MapVersion"))
			return ipv6map_dll;
	}
	return NULL;
}

/**
 * return version string of loaded IPv6Map DLL. return `NULL` if it hasn't been loaded.
 */
inline const char *getIPv6MapVersion() {
	HMODULE ipv6map_dll = getIPv6MapDll();
	return ipv6map_dll ? (const char *)GetProcAddress(ipv6map_dll, "IPv6MapVersion") : NULL;
}

/**
 * return pointer to `sockaddr6to4` of loaded IPv6Map DLL. return `NULL` if it hasn't been loaded.
 */
inline sockaddr6to4_t getSockaddr6to4() {
	HMODULE ipv6map_dll = getIPv6MapDll();
	return ipv6map_dll ? (sockaddr6to4_t)GetProcAddress(ipv6map_dll, "sockaddr6to4") : NULL;
}

/**
 *  return pointer to `sockaddr4to6` of loaded IPv6Map DLL. return `NULL` if it hasn't been loaded.
 */
inline sockaddr4to6_t getSockaddr4to6() {
	HMODULE ipv6map_dll = getIPv6MapDll();
	return ipv6map_dll ? (sockaddr4to6_t)GetProcAddress(ipv6map_dll, "sockaddr4to6") : NULL;
}

#endif
