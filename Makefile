server:
	mkdir -p build
	g++ src/main_server.cpp src/server.cpp src/network.cpp -o build/server

client:
	mkdir -p build
	g++ src/main_client.cpp src/network.cpp src/client.cpp -o build/client

run: server client
	build/server 127.0.0.1:12345 5 &
	build/client 127.0.0.1:12345 lena_color.tiff 2000 1 5000000
