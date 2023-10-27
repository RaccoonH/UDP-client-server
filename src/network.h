#pragma once

#include "packet.h"

#include <map>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <vector>

class IPv4
{
public:
    IPv4(const std::string &addr)
    {
        auto res = sscanf(addr.c_str(), "%hhu.%hhu.%hhu.%hhu:%hu", &m_ip[0], &m_ip[1], &m_ip[2], &m_ip[3], &m_port);
        if (res != 5) {
            throw std::runtime_error("Wrong address: " + addr);
        }
    }

    uint16_t GetPort() const
    {
        return m_port;
    }

    std::string GetIPString() const
    {
        return std::to_string(m_ip[0]) + "." + std::to_string(m_ip[1]) + "." + std::to_string(m_ip[2]) + "." + std::to_string(m_ip[3]);
    }

    std::string ToString() const
    {
        return std::to_string(m_ip[0]) + "." + std::to_string(m_ip[1]) + "." + std::to_string(m_ip[2]) + "." + std::to_string(m_ip[3]) + ":" + std::to_string(m_port);
    }

private:
    uint8_t m_ip[4];
    uint16_t m_port;
};

class UploadingFile
{
public:
    UploadingFile(uint64_t id, const sockaddr_in &socket, uint32_t totalPacketSize);
    size_t PushPacket(const Packet &packet);
    bool IsFinished() { return m_data.size() == m_totalPacketCount; }
    uint32_t CRC32();

private:
    uint64_t m_id;
    sockaddr_in m_clientSocket;
    uint32_t m_totalPacketCount;
    std::map<uint32_t, std::vector<uint8_t>> m_data;
};