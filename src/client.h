#pragma once

#include "network.h"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <netinet/in.h>

class Client
{
public:
    Client(const IPv4 &serverAddr, const std::filesystem::path &file, const std::chrono::milliseconds &timeout, int threadsCount, uint64_t bitrate);
    ~Client();

    ErrorCode SendFile();
    size_t GetFileSize();

private:
    ErrorCode ClientThread(uint32_t seq_total, uint32_t crc, int clientSocket);

private:
    IPv4 m_addr;
    std::vector<int> m_sockets;
    uint64_t m_bitrate;
    struct sockaddr_in m_server;
    std::chrono::milliseconds m_timeout;

    std::ifstream m_file;
    uint64_t m_fileID;

    std::mutex m_mutex;
    std::vector<std::vector<uint8_t>> m_packets;
    std::atomic_bool m_result;
};