#include "client.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <future>
#include <iostream>
#include <random>
#include <thread>

Client::Client(const IPv4 &address, const std::filesystem::path &file, const std::chrono::milliseconds &timeout, int threadsCount, uint64_t bitrate)
    : m_addr(address), m_timeout(timeout), m_bitrate(bitrate), m_result(false)
{
    if (threadsCount <= 0) {
        throw std::runtime_error("Incorrect num of threads " + std::to_string(threadsCount));
    }
    if (!std::filesystem::exists(file)) {
        throw std::runtime_error("Could not find file " + file.string());
    }
    for (int i = 0; i < threadsCount; i++) {
        auto clientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (clientSocket == -1) {
            for (auto sock : m_sockets) {
                close(sock);
            }
            throw std::runtime_error("Could not create socket");
        }
        m_sockets.push_back(clientSocket);
    }

    uint64_t crc = crc32c(0, (const unsigned char *)file.string().c_str(), file.string().size());
    m_fileID = (crc << 32) | crc32c(crc, (const unsigned char *)file.string().c_str(), file.string().size());

    m_file.open(file.string(), std::ios::in | std::ios::binary);

    m_server.sin_family = AF_INET;
    m_server.sin_port = address.GetPort();
    m_server.sin_addr.s_addr = inet_addr(address.GetIPString().c_str());
}

Client::~Client()
{
    m_file.close();
    for (auto socket : m_sockets) {
        close(socket);
    }
}

ErrorCode Client::SendFile()
{
    m_file.seekg(0, std::ios_base::end);
    uint32_t seq_total = m_file.tellg() / MAX_PAYLOAD_SIZE;
    seq_total += m_file.tellg() % MAX_PAYLOAD_SIZE == 0 ? 0 : 1;
    m_file.seekg(0, std::ios_base::beg);

    uint32_t crc = 0;
    uint32_t packet_num = 0;
    while (!m_file.eof()) {

        PacketHeader packetHeader;
        packetHeader.seq_number = packet_num;
        packetHeader.seq_total = seq_total;
        packetHeader.type = PUT;
        std::memcpy(packetHeader.id, &m_fileID, 8);

        auto &packet = m_packets.emplace_back();
        packet.resize(MAX_PACKET_SIZE_WITH_NO_UDP);
        PacketHeaderToBuffer(packetHeader, packet.data(), packet.size());
        m_file.read((char *)packet.data() + HEADER_SIZE, MAX_PAYLOAD_SIZE);

        auto bytes = m_file.gcount();
        if (bytes < MAX_PAYLOAD_SIZE) {
            packet.resize(bytes + HEADER_SIZE);
        }
        packet_num++;

        crc = crc32c(crc, packet.data() + HEADER_SIZE, packet.size() - HEADER_SIZE);
    }
    std::shuffle(m_packets.begin(), m_packets.end(), std::random_device());

    std::vector<std::future<ErrorCode>> threads;
    for (auto sock : m_sockets) {
        threads.emplace_back(std::async(&Client::ClientThread, this, seq_total, crc, sock));
    }

    for (int i = 0; i < m_sockets.size(); i++) {
        auto res = threads[i].get();
        if (res != OK) {
            printf("Thread %d returned error: %d\n", i, res);
            continue;
        }
    }
    return m_result ? OK : UNKNOWN;
}

ErrorCode Client::ClientThread(uint32_t seq_total, uint32_t crc, int clientSocket)
{
    using namespace std::chrono;

    while (true) {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_packets.empty()) {
            return OK;
        }
        auto packet = m_packets.back();
        m_packets.pop_back();
        lock.unlock();

        struct pollfd readfds[1];
        readfds[0].fd = clientSocket;
        readfds[0].events = POLLIN;

        int type_pos = 8;
        int seq_total_pos = 4;
        socklen_t len = sizeof(m_server);
        std::vector<uint8_t> recvBuffer(HEADER_SIZE + 4);
        auto sendStart = steady_clock::now();

        bool receivedAckPacket = false;
        bool sendPacket = true;
        while (!receivedAckPacket) {
            if (sendPacket) {
                sendto(clientSocket, packet.data(), packet.size(), 0, (const struct sockaddr *)&m_server, len);
                sendPacket = false;
            }

            auto res = poll(readfds, 1, m_timeout.count());
            if (res == 0) {
                std::cout << "Timeout expired, send packet again" << std::endl;
                sendPacket = true;
                continue;
            }
            if (res < 0) {
                std::cout << "poll error: " << strerror(errno) << std::endl;
                return UNKNOWN;
            }

            auto messageSize = recvfrom(clientSocket, recvBuffer.data(), recvBuffer.size(), 0, (struct sockaddr *)&m_server, &len);
            if (messageSize < HEADER_SIZE) {
                std::cout << "Wrong packet size!" << std::endl;
                break;
            }
            PacketHeader recvHeader(recvBuffer.data(), recvBuffer.size());

            if (recvHeader.type != ACK) {
                std::cout << "Wrong packet type!" << std::endl;
                break;
            }

            if (recvHeader.seq_number != GetPacketSeqNumFromBuffer(packet.data())) {
                std::cout << "Wrong packet is received, skip it" << std::endl;
                continue;
            }
            receivedAckPacket = true;

            if (recvHeader.seq_total != seq_total)
                break;
            if (messageSize < HEADER_SIZE + 4) {
                std::cout << "Wrong packet size!" << std::endl;
                break;
            }

            auto inCRC = *(uint32_t *)(recvBuffer.data() + HEADER_SIZE);
            printf("Client CRC: %u\nServer CRC: %u\n", crc, inCRC);
            if (inCRC == crc) {
                std::cout << "File has been sent successfully" << std::endl;
                m_result = true;
                return OK;
            }
            else {
                std::cout << "Wrong CRC!" << std::endl;
                return WRONG_CRC;
            }
        }
        auto elapsedTime = duration_cast<milliseconds>(steady_clock::now() - sendStart);
        auto timeByBitrate = (packet.size() * 1000 * 8) / m_bitrate;
        if (elapsedTime.count() < timeByBitrate) {
            std::this_thread::sleep_for(milliseconds(timeByBitrate - elapsedTime.count()));
        }
    }

    return UNKNOWN;
}

size_t Client::GetFileSize()
{
    if (!m_file.is_open())
        return 0;
    m_file.clear();
    m_file.seekg(0, std::ios_base::end);
    size_t length = m_file.tellg();
    m_file.seekg(0, std::ios_base::beg);

    return length;
}