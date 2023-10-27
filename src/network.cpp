#include "network.h"

#include <cstring>

UploadingFile::UploadingFile(uint64_t id, const sockaddr_in &socket, uint32_t totalPacketSize)
    : m_id(id), m_clientSocket(socket), m_totalPacketCount(totalPacketSize)
{
}

size_t UploadingFile::PushPacket(const Packet &packet)
{
    if (m_data.count(packet.header.seq_number)) {
        return m_data.size();
    }
    auto &packetData = m_data[packet.header.seq_number];
    packetData.insert(packetData.begin(), packet.data.begin(), packet.data.end());
    return m_data.size();
}

uint32_t UploadingFile::CRC32()
{
    uint32_t crc = 0;
    for (auto &[_, data] : m_data) {
        crc = crc32c(crc, data.data(), data.size());
    }
    return crc;
}
