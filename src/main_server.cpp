#include "network.h"
#include "server.h"

#include <cstdlib>
#include <iostream>
#include <signal.h>
#include <stdio.h>
#include <thread>
#include <unistd.h>

static std::unique_ptr<Server> server;

void SignalHandler(int s)
{
    std::cout << "Caught signal" << std::endl;
    if (server)
        server->Stop();
    exit(1);
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        std::cout << "Pass args: ip_address lifetime_sec (0 means infinity life) (e.g. 127.0.0.1:12345 10)" << std::endl;
        return -1;
    }

    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = SignalHandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

    try {
        IPv4 serverAddr(argv[1]);
        server = std::make_unique<Server>(serverAddr);
        server->Start();
        server->Loop(std::chrono::seconds(std::atoi(argv[2])));
    } catch (const std::exception &e) {
        std::cout << e.what() << std::endl;
        return -1;
    }

    return 0;
}