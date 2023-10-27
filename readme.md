# UDP client/server file transfer
A small application with client and server on UDP.

## What does it do?
Client divides a file into small chunks and sends it to server in different order.  
Server receives them and calculates CRC of all file and send final CRC.  
Client checks own CRC and server's CRC.  

## Build
You can use cmake or make.      
### Requirements:
1. c++17
You should use GCC >= 11.4.0 (at least it was tested only on GCC 11.4.0)  

## How to run
At First start server with next args: **ip_address lifetime_sec (0 means infinity life)**  
Further start client with next args: **server_address filepath timeout_ms threads [optional, default 2] bitrate_one_thread_bps[optional, default 2000 kbps]**  
You can just start ```make run```, to see how it works  