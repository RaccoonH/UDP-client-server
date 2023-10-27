cmake_minimum_required(VERSION 3.16)

project(udp_communication)

add_executable(client
    src/main_client.cpp
    src/network.h
    src/network.cpp
    src/packet.h
    src/client.h
    src/client.cpp)

add_executable(server
    src/main_server.cpp
    src/server.cpp
    src/server.h
    src/network.h
    src/network.cpp
    src/packet.h)