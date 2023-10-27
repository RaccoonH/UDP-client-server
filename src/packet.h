#pragma once

#include <cstdint>
#include <cstring>
#include <stddef.h>
#include <stdexcept>
#include <vector>

enum PacketType
{
    ACK = 0,
    PUT = 1
};

enum ErrorCode
{
    OK = 0,
    UNKNOWN = 1,
    WRONG_CRC = 2,
    SOCKET_ERROR = 3
};

constexpr int HEADER_SIZE = 17;
constexpr int MAX_PACKET_SIZE = 1472;
constexpr int UDP_HEADER_SIZE = 8;
constexpr int MAX_PACKET_SIZE_WITH_NO_UDP = MAX_PACKET_SIZE - UDP_HEADER_SIZE;
constexpr int MAX_PAYLOAD_SIZE = MAX_PACKET_SIZE_WITH_NO_UDP - HEADER_SIZE;

struct PacketHeader
{
    PacketHeader() = default;
    PacketHeader(const uint8_t *data, size_t size)
    {
        if (size < HEADER_SIZE) {
            throw std::runtime_error("Few input data");
        }
        seq_number = *(uint32_t *)data;
        seq_total = *(uint32_t *)(data + 4);
        type = *(data + 8);
        std::memcpy(id, data + 9, 8);
    }
    uint32_t seq_number;
    uint32_t seq_total;
    uint8_t type;
    uint8_t id[8];
};

struct Packet
{
    Packet() = default;
    Packet(const uint8_t *indata, size_t size)
        : header(indata, size)
    {
        data.insert(data.begin(), indata + HEADER_SIZE, indata + size);
    }
    PacketHeader header;
    std::vector<uint8_t> data;
};

inline bool PacketHeaderToBuffer(const PacketHeader &packet, uint8_t *buffer, size_t size)
{
    if (size < HEADER_SIZE)
        return false;

    std::memcpy(buffer, &packet.seq_number, 4);
    std::memcpy(buffer + 4, &packet.seq_total, 4);
    std::memcpy(buffer + 8, &packet.type, 1);
    std::memcpy(buffer + 9, packet.id, 8);
    return true;
}

inline bool PacketToBuffer(const Packet &packet, uint8_t *buffer, size_t size)
{
    if (!PacketHeaderToBuffer(packet.header, buffer, size))
        return false;
    if (packet.data.empty())
        return true;
    if ((packet.data.size() + HEADER_SIZE) > size) {
        return false;
    }

    std::memcpy(buffer + HEADER_SIZE, packet.data.data(), packet.data.size());
    return true;
}

inline uint32_t GetPacketSeqNumFromBuffer(const uint8_t *buffer)
{
    return *(uint32_t *)buffer;
}

inline uint32_t crc32c(uint32_t crc, const unsigned char *buf, size_t len)
{
    int k;
    crc = ~crc;
    while (len--) {
        crc ^= *buf++;
        for (k = 0; k < 8; k++)
            crc = crc & 1 ? (crc >> 1) ^ 0x82f63b78 : crc >> 1;
    }
    return ~crc;
}