# Running SokuLobbies on Linux

## Original text by [Ham](https://github.com/MrsHammies), edited by Daichungus

The source code can be found in the
[SokuLobbies repo](https://github.com/PinkySmile/SokuLobbies).

## Dependencies

- A C++ Compiler

  e.g. `gcc++`

- CMake

  The minimum required version of CMake is `3.15`.

- SFML

### Debian/Ubuntu

```sh
sudo apt-get install g++ cmake libsfml-dev
```

### Fedora

```sh
sudo dnf install gcc-c++ cmake SFML-devel
```

## Building

First, generate the CMake files for compiling.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DMAIN_SERVER_HOST=pinkysmile.fr
```

Next, compile the SokuLobbies server program.

```sh
cmake --build build --target SokuLobbiesServer
```

The program will be located within the `build/` directory.

## Usage

```sh
./SokuLobbiesServer <port> <max players> <name of lobby> [password (blank if none)]
```

An example output is shown below:

```sh
you@example:~/SokuLobbies/build$ ./SokuLobbiesServer 10800 42 test P@s5M0Rd
Password is 'P@s5M0Rd'
Listening on port 10800
> Registering lobby 'test' to the server: 42 max slots
Main server is pinkysmile.fr:5254
```

Any new connections or disconnections will be logged.

Use `/help` within the program for a full list of admin commands.

## Persistent background running

You can use `screen` for persistent running of `SokuLobbiesServer`
in the background, even if the current terminal session is closed.
