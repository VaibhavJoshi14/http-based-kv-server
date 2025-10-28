all:
	g++ database.cpp -o database -I/usr/local/include -L/usr/local/lib -lcassandra -luv
	g++ server.cpp -o server `pkg-config --cflags --libs opencv4`
	g++ client.cpp -o client

clean:
	rm -f client server database