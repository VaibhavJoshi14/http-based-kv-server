all:
	g++ src/database.cpp -o database -I/usr/local/include -L/usr/local/lib -lcassandra -luv
	g++ src/server.cpp -o server `pkg-config --cflags --libs opencv4`
	g++ src/client.cpp -o client
	g++ src/loadgenerator.cpp -o ldgen

clean:
	rm -f client server database ldgen
