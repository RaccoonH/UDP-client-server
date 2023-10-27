#include "client.h"
#include "network.h"

#include <iostream>
#include <thread>

constexpr uint64_t BITRATE_2000_KBPS = 2000 * 1000;
int main(int argc, char *argv[])
{
    if (argc < 4) {
        printf("Pass args: \nserver_address \nfilepath \ntimeout_ms \nthreads [optional, default 2]\nbitrate_one_thread_bps[optional, default 2000 kbps]\n"
               "e.g. 127.0.0.1:12345 /home/test/image.png 2000 1 2000000\n");
        return -1;
    }
    try {
        using namespace std::chrono;
        auto threads = 2;
        auto bitrate = BITRATE_2000_KBPS;
        if (argc >= 5) {
            threads = std::stoi(argv[4]);
        }
        if (argc >= 6) {
            bitrate = std::stoull(argv[5]);
        }

        IPv4 serverAddr(argv[1]);
        Client client(serverAddr, argv[2], std::chrono::milliseconds(std::stoi(argv[3])), threads, bitrate);
        auto now = steady_clock::now();
        if (auto err = client.SendFile(); err != OK) {
            std::cout << "Error: " << err << std::endl;
        }
        auto elapsedTime = duration_cast<milliseconds>(steady_clock::now() - now);
        double fileSize = (float)client.GetFileSize();
        printf("File size: %.3lf kB, Elapsed time: %ld ms\n", fileSize / 1000, elapsedTime.count());
        printf("Average bitrate: %.3lf kbps\n", fileSize * 8 / 1000 / elapsedTime.count() * 1000);
    } catch (const std::exception &e) {
        std::cout << e.what() << std::endl;
        return -1;
    }

    return 0;
}