cmake_minimum_required(VERSION 3.15)
project(SokuLobbies)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/pkgs)

if (CMAKE_BUILD_TYPE STREQUAL Debug)
	if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		add_compile_options(-W -Wall -Wextra)
	endif ()
	add_compile_definitions(_DEBUG=1)
else ()
	add_compile_definitions(NDEBUG=1)
endif ()
if (NOLOG)
	add_compile_definitions(_LOBBYNOLOG=1)
endif()
if (ASAN)
	if (WIN32)
		if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
			add_compile_options(/fsanitize=address)
			add_link_options(/fsanitize=address)
		endif()
	else ()
		add_compile_options(-fsanitize=address)
		add_link_options(-fsanitize=address)
	endif ()
endif ()

include_directories(src/Common)
find_package(SFML)

add_executable(
	SokuLobbiesMainServer
	src/Common/Socket.hpp
	src/Common/Socket.cpp
	src/MainServer/main.cpp
	src/Common/GuardedMutex.cpp
	src/Common/GuardedMutex.hpp
	src/Common/Utils.cpp
	src/Common/Utils.hpp
)
if (WIN32)
	target_link_libraries(SokuLobbiesMainServer ws2_32)
endif()

if (SFML_FOUND)
	add_executable(
		SokuLobbiesServer
		src/Server/main.cpp
		src/Common/Packet.hpp
		src/Common/Packet.cpp
		src/Common/Socket.hpp
		src/Common/Socket.cpp
		src/Server/Connection.cpp
		src/Server/Connection.hpp
		src/Server/Server.cpp
		src/Server/Server.hpp
		src/Common/GuardedMutex.cpp
		src/Common/GuardedMutex.hpp
		src/Common/Utils.cpp
		src/Common/Utils.hpp
		src/Common/encodingConverter.cpp
		src/Common/encodingConverter.hpp
		src/Common/shiftJISConvTable.inl
		src/Common/shiftJISReverseConvTable.inl
	)
	if (MAIN_SERVER_HOST)
		target_compile_definitions(SokuLobbiesServer PRIVATE MAIN_SERVER_HOST="${MAIN_SERVER_HOST}")
	endif ()
	target_include_directories(SokuLobbiesServer PRIVATE ${SFML_INCLUDE_DIRS})
	if (WIN32)
		target_link_libraries(SokuLobbiesServer ${SFML_NETWORK_LIBRARY} ${SFML_SYSTEM_LIBRARY} ws2_32)
	else()
		target_link_libraries(SokuLobbiesServer ${SFML_NETWORK_LIBRARY} ${SFML_SYSTEM_LIBRARY})
	endif()
endif()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	add_definitions(-DWINVER=0x0600 -D_WIN32_WINNT=0x0600)
	if (CMAKE_BUILD_TYPE STREQUAL Debug)
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebug")
	else()
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
	endif()
	add_subdirectory(src/SokuMod/SokuLib)

	# mbedTLS
	option(ENABLE_PROGRAMS "" OFF)
	option(INSTALL_MBEDTLS_HEADERS "" OFF)
	option(MBEDTLS_FATAL_WARNINGS "" OFF)
	add_subdirectory(src/SokuMod/mbedtls EXCLUDE_FROM_ALL)

	add_library(mbedtls_shim INTERFACE)
	target_link_libraries(mbedtls_shim INTERFACE mbedtls mbedx509 mbedcrypto)

	# workaround for curl: link against our local mbedtls rather than search for it
	function(find_package)
		if(ARGV0 STREQUAL "MbedTLS")
			set(MBEDTLS_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/src/SokuMod/mbedtls/include" CACHE STRING "")
			set(MBEDTLS_LIBRARIES "" CACHE STRING "")
			mark_as_advanced(MBEDTLS_INCLUDE_DIRS MBEDTLS_LIBRARIES)
		else()
			_find_package(${ARGV})
		endif()
	endfunction()

	# workaround for curl: don't try installing any export files
	function(install)
		if(ARGV0 STREQUAL "EXPORT")
		else()
			_install(${ARGV})
		endif()
	endfunction()


	# cURL
	option(BUILD_CURL_EXE "" OFF)
	option(BUILD_SHARED_LIBS "" OFF)
	option(BUILD_TESTING "" OFF)
	option(CURL_STATIC_CRT "" ON)
	option(HTTP_ONLY "" ON)
	option(CURL_DISABLE_CRYPTO_AUTH "" ON)
	option(ENABLE_MANUAL "" OFF)
	option(CURL_USE_OPENSSL "" OFF)
	option(CURL_USE_MBEDTLS "" ON)
	option(CURL_DISABLE_LDAP "" ON)
	option(CURL_ZLIB "" OFF)
	option(CMAKE_USE_LIBSSH2 "" OFF)
	option(ENABLE_UNIX_SOCKETS "" OFF)
	option(CURL_CA_BUNDLE "" none)
	option(CURL_CA_PATH "" none)
	option(CURL_WINDOWS_SSPI "" OFF)
	add_subdirectory(src/SokuMod/curl EXCLUDE_FROM_ALL)

	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /Brepro")
	set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /Brepro")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /Brepro")

	add_library(
		"SokuLobbiesMod"
		MODULE
		src/SokuMod/version.rc
		src/SokuMod/main.cpp
		src/Common/Packet.hpp
		src/Common/Packet.cpp
		src/Common/Socket.hpp
		src/Common/Socket.cpp
		src/SokuMod/LobbyMenu.cpp
		src/SokuMod/LobbyMenu.hpp
		src/SokuMod/Connection.cpp
		src/SokuMod/Connection.hpp
		src/SokuMod/InputBox.cpp
		src/SokuMod/InputBox.cpp
		src/SokuMod/InLobbyMenu.cpp
		src/SokuMod/InLobbyMenu.cpp
		src/SokuMod/getPublicIp.hpp
		src/SokuMod/getPublicIp.cpp
		src/SokuMod/Player.cpp
		src/SokuMod/Player.hpp
		src/Common/GuardedMutex.cpp
		src/Common/GuardedMutex.hpp
		src/Common/Utils.cpp
		src/Common/Utils.hpp
		src/SokuMod/data.hpp
		src/SokuMod/AchievementsMenu.cpp
		src/SokuMod/AchievementsMenu.hpp
		src/SokuMod/StatsMenu.cpp
		src/SokuMod/StatsMenu.hpp
		src/SokuMod/LobbyData.cpp
		src/SokuMod/LobbyData.hpp
		src/SokuMod/SmallHostlist.cpp
		src/SokuMod/SmallHostlist.hpp
		src/SokuMod/BackgroundHost.cpp
		src/SokuMod/createUTFTexture.cpp
		src/SokuMod/createUTFTexture.hpp
		src/SokuMod/ipv6map.cpp
		src/Common/encodingConverter.cpp
		src/Common/encodingConverter.hpp
		src/Common/shiftJISConvTable.inl
		src/Common/shiftJISReverseConvTable.inl
	)
	target_compile_options(SokuLobbiesMod PRIVATE /Zi)
	target_compile_definitions("SokuLobbiesMod" PRIVATE DIRECTINPUT_VERSION=0x0800 _CRT_SECURE_NO_WARNINGS)
	target_link_libraries(
		SokuLobbiesMod
		SokuLib
		shlwapi
		ws2_32
		d3dx9
		imm32
		version
		CURL::libcurl
		mbedtls_shim
	)
	target_link_directories(SokuLobbiesMod PRIVATE lib)
	target_include_directories(SokuLobbiesMod PRIVATE lib/include)
	target_compile_definitions(SokuLobbiesMod PRIVATE _LOBBYNOLOG=1)
endif()
