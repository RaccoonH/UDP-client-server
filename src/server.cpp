#include "server.h"

#include <arpa/inet.h>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <fstream>
#include <iostream>

Server::Server(const IPv4 &address)
    : m_addr(address)
{
    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == -1) {
        throw std::runtime_error("Could not create socket");
    }

    struct sockaddr_in sockaddr;
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = address.GetPort();
    sockaddr.sin_addr.s_addr = inet_addr(address.GetIPString().c_str());

    if (bind(m_socket, (const struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0) {
        close(m_socket);
        throw std::runtime_error("Could not bind socket to address " + address.ToString());
    }

    m_eventfd = eventfd(0, 0);
    if (m_eventfd == -1) {
        close(m_socket);
        throw std::runtime_error("Could not create eventfd");
    }
    m_stopped = false;
}

Server::~Server()
{
    if (m_serverThread && m_serverThread->joinable()) {
        Stop();
    }
    close(m_socket);
    close(m_eventfd);
}

ErrorCode Server::Start()
{
    m_serverThread = std::make_unique<std::thread>(&Server::ServerThread, this);
    return OK;
}

void Server::Loop(const std::chrono::milliseconds &lifetime)
{
    struct pollfd readfds[1];
    readfds[0].fd = m_eventfd;
    readfds[0].events = POLLIN;

    if (lifetime.count() > 0) {
        poll(readfds, 1, lifetime.count());
        if (m_serverThread && m_serverThread->joinable()) {
            Stop();
        }
    }
    else {
        while (!m_stopped) {
            poll(readfds, 1, -1);
        }
    }
}

ErrorCode Server::ServerThread()
{
    m_stopped = false;

    std::vector<uint8_t> buffer(MAX_PACKET_SIZE_WITH_NO_UDP);

    struct pollfd readfds[2];
    readfds[0].fd = m_socket;
    readfds[0].events = POLLIN;
    readfds[1].fd = m_eventfd;
    readfds[1].events = POLLIN;

    while (!m_stopped) {
        auto res = poll(readfds, 2, 2000);
        if (res > 0) {
            if (!(readfds[0].revents & POLLIN)) {
                readfds[0].revents = 0;
                continue;
            }

            struct sockaddr_in clientAddr;
            socklen_t len = sizeof(clientAddr);
            auto messageSize = recvfrom(m_socket, buffer.data(), buffer.size(), 0, (struct sockaddr *)&clientAddr, &len);
            if (messageSize <= 0) {
                continue;
            }

            Packet inPacket(buffer.data(), messageSize);
            if (inPacket.header.type != PUT) {
                std::cout << "Wrong packet type!" << std::endl;
                continue;
            }
            Packet outPacket;
            outPacket.header = inPacket.header;
            outPacket.header.type = ACK;

            auto id = *reinterpret_cast<uint64_t *>(inPacket.header.id);
            if (!m_files.count(id)) {
                m_files.insert({id, UploadingFile(id, clientAddr, inPacket.header.seq_total)});
            }

            auto &file = m_files.at(id);
            outPacket.header.seq_total = file.PushPacket(inPacket);
            if (file.IsFinished()) {
                auto crc = file.CRC32();
                std::cout << "Send calculated CRC " << crc << std::endl;
                outPacket.data.insert(outPacket.data.begin(), (uint8_t *)&crc, (uint8_t *)&crc + sizeof(crc));
                m_files.erase(id);
            }

            if (!PacketToBuffer(outPacket, buffer.data(), buffer.size())) {
                std::cout << "Could not push packet to buffer" << std::endl;
                continue;
            }
            sendto(m_socket, buffer.data(), buffer.size(), 0, (struct sockaddr *)&clientAddr, len);
        }
        else if (res < 0) {
            std::cout << "poll error: " << strerror(errno) << std::endl;
            continue;
        }
    }

    return OK;
}

ErrorCode Server::Stop()
{
    m_stopped = true;
    char minValueForEvent[8] = "testval";
    write(m_eventfd, minValueForEvent, sizeof(minValueForEvent));
    if (m_serverThread && m_serverThread->joinable()) {
        m_serverThread->join();
    }
    return OK;
}