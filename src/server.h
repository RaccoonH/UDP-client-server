#pragma once

#include "network.h"
#include "packet.h"

#include <atomic>
#include <thread>
#include <unordered_map>
#include <vector>

class Server
{
public:
    Server(const IPv4 &address);
    ~Server();

    ErrorCode Start();
    void Loop(const std::chrono::milliseconds &lifetime);
    ErrorCode Stop();

private:
    ErrorCode ServerThread();

private:
    IPv4 m_addr;
    int m_socket;
    int m_eventfd;
    std::atomic_bool m_stopped;

    std::unique_ptr<std::thread> m_serverThread;
    std::unordered_map<uint64_t, UploadingFile> m_files;
};