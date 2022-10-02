#
# A CMake module to find SFML
#
# Once done this module will define:
#  SFML_FOUND - system has SFML
#  SFML_GRAPHICS_LIBRARY
#  SFML_AUDIO_LIBRARY
#  SFML_NETWORK_LIBRARY
#  SFML_SYSTEM_LIBRARY
#  SFML_WINDOW_LIBRARY
#  - Link these to use SFML
#

IF (NOT SFML_INCLUDE_DIRS OR NOT SFML_GRAPHICS_LIBRARY OR NOT SFML_AUDIO_LIBRARY OR NOT SFML_NETWORK_LIBRARY OR NOT SFML_SYSTEM_LIBRARY OR NOT SFML_WINDOW_LIBRARY)
        IF (MSVC)      # Visual Studio
                SET(CMAKE_FIND_LIBRARY_PREFIXES ";lib")
                SET(CMAKE_FIND_LIBRARY_SUFFIXES ".lib;.dll.lib;.dll.a;.a")
        ELSEIF (MINGW) # Windows
                SET(CMAKE_FIND_LIBRARY_PREFIXES ";lib")
                SET(CMAKE_FIND_LIBRARY_SUFFIXES ".dll.a;.dll;.a;.lib")
        ELSE (MSVC)    # Linux and MacOS
                SET(CMAKE_FIND_LIBRARY_PREFIXES "lib")
                SET(CMAKE_FIND_LIBRARY_SUFFIXES ".so;.dylib;.a")
        ENDIF(MSVC)

        FIND_LIBRARY(SFML_GRAPHICS_LIBRARY
                NAMES
                sfml-graphics
                PATHS
                ${SFML_DIR}/lib/              # SFML root directory (if provided)
                ${SFML_DIR}                   # SFML root directory (if provided)
                /usr/lib64/                   # Default Fedora28 library path
                /usr/lib/                     # Some more Linux library path
                /usr/lib/x86_64-linux-gnu/    # Some more Linux library path
                /usr/local/lib/               # Some more Linux library path
                /usr/local/lib64/             # Some more Linux library path
        )

        FIND_LIBRARY(SFML_AUDIO_LIBRARY
                NAMES
                sfml-audio
                PATHS
                ${SFML_DIR}/lib/              # SFML root directory (if provided)
                ${SFML_DIR}                   # SFML root directory (if provided)
                /usr/lib64/                   # Default Fedora28 library path
                /usr/lib/                     # Some more Linux library path
                /usr/lib/x86_64-linux-gnu/    # Some more Linux library path
                /usr/local/lib/               # Some more Linux library path
                /usr/local/lib64/             # Some more Linux library path
        )

        FIND_LIBRARY(SFML_NETWORK_LIBRARY
                NAMES
                sfml-network
                PATHS
                ${SFML_DIR}/lib/              # SFML root directory (if provided)
                ${SFML_DIR}                   # SFML root directory (if provided)
                /usr/lib64/                   # Default Fedora28 library path
                /usr/lib/                     # Some more Linux library path
                /usr/lib/x86_64-linux-gnu/    # Some more Linux library path
                /usr/local/lib/               # Some more Linux library path
                /usr/local/lib64/             # Some more Linux library path
        )

        FIND_LIBRARY(SFML_SYSTEM_LIBRARY
                NAMES
                sfml-system
                PATHS
                ${SFML_DIR}/lib/              # SFML root directory (if provided)
                ${SFML_DIR}                   # SFML root directory (if provided)
                /usr/lib64/                   # Default Fedora28 library path
                /usr/lib/                     # Some more Linux library path
                /usr/lib/x86_64-linux-gnu/    # Some more Linux library path
                /usr/local/lib/               # Some more Linux library path
                /usr/local/lib64/             # Some more Linux library path
        )

        FIND_LIBRARY(SFML_WINDOW_LIBRARY
                NAMES
                sfml-window
                PATHS
                ${SFML_DIR}/lib/              # SFML root directory (if provided)
                ${SFML_DIR}                   # SFML root directory (if provided)
                /usr/lib64/                   # Default Fedora28 library path
                /usr/lib/                     # Some more Linux library path
                /usr/lib/x86_64-linux-gnu/    # Some more Linux library path
                /usr/local/lib/               # Some more Linux library path
                /usr/local/lib64/             # Some more Linux library path
        )

        FIND_PATH(SFML_INCLUDE_DIRS
                NAMES
                SFML/Graphics.hpp
                PATHS
                ${SFML_DIR}/include/             # SFML root directory (if provided)
                ${SFML_DIR}                      # SFML root directory (if provided)
                /usr/include/                    # Default Fedora28 system include path
                /usr/local/include/              # Default Fedora28 local include path
                ${CMAKE_MODULE_PATH}/include/    # Expected to contain the path to this file for Windows10
        )
ENDIF (NOT SFML_INCLUDE_DIRS OR NOT SFML_GRAPHICS_LIBRARY OR NOT SFML_AUDIO_LIBRARY OR NOT SFML_NETWORK_LIBRARY OR NOT SFML_SYSTEM_LIBRARY OR NOT SFML_WINDOW_LIBRARY)

IF (SFML_INCLUDE_DIRS AND SFML_GRAPHICS_LIBRARY AND SFML_AUDIO_LIBRARY AND SFML_NETWORK_LIBRARY AND SFML_SYSTEM_LIBRARY AND SFML_WINDOW_LIBRARY)
        SET(SFML_FOUND TRUE)
ELSE ()
        SET(SFML_FOUND FALSE)
ENDIF ()

IF (SFML_FIND_REQUIRED AND NOT SFML_FOUND)
        MESSAGE(FATAL_ERROR
                "  SFML not found.\n"
                "      Fill CMake variable SFML_DIR to the provided directory.\n"
                )
ENDIF (SFML_FIND_REQUIRED AND NOT SFML_FOUND)
