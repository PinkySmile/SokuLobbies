cmake_minimum_required(VERSION 3.22)
project(SokuLobbies)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/pkgs)

if (CMAKE_BUILD_TYPE STREQUAL Debug)
	if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		add_compile_options(-W -Wall -Wextra)
		if (NOT WIN32 AND ASAN)
			add_compile_options(-fsanitize=address)
			link_libraries(asan)
		endif ()
	endif ()
	add_compile_definitions(_DEBUG=1)
else ()
	add_compile_definitions(NDEBUG=1)
endif ()
if (NOLOG)
	add_compile_definitions(_LOBBYNOLOG=1)
endif()

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
	)
	target_include_directories(SokuLobbiesServer PRIVATE ${SFML_INCLUDE_DIRS})
	if (WIN32)
		target_link_libraries(SokuLobbiesServer ${SFML_NETWORK_LIBRARY} ${SFML_SYSTEM_LIBRARY} ws2_32)
	else()
		target_link_libraries(SokuLobbiesServer ${SFML_NETWORK_LIBRARY} ${SFML_SYSTEM_LIBRARY})
	endif()
endif()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	add_definitions(-DWINVER=0x0501 -D_WIN32_WINNT=0x0501)
	if (CMAKE_BUILD_TYPE STREQUAL Debug)
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebug")
	else()
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
	endif()
	add_subdirectory(src/SokuMod/SokuLib)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /Brepro")
	set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /Brepro")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /Brepro")

	add_library(
		"SokuLobbiesMod"
		MODULE
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
		src/SokuMod/StatsMenu.cpp src/SokuMod/StatsMenu.hpp src/SokuMod/LobbyData.cpp src/SokuMod/LobbyData.hpp)
	target_compile_options(SokuLobbiesMod PRIVATE /Zi)
	target_compile_definitions("SokuLobbiesMod" PRIVATE DIRECTINPUT_VERSION=0x0800 _CRT_SECURE_NO_WARNINGS)
	target_link_directories("SokuLobbiesMod" PRIVATE lib)
	target_link_libraries(
		SokuLobbiesMod
		SokuLib
		shlwapi
		ws2_32
		d3dx9
	)
	if (CMAKE_BUILD_TYPE STREQUAL Debug)
	else ()
		target_compile_definitions(SokuLobbiesMod PRIVATE _LOBBYNOLOG=1)
	endif ()
endif()
