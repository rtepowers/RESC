all: rescSystem
rescSystem: 
	g++ rescClient.cpp -o rescClient -lcurses -lpthread
	g++ rescServer.cpp -o rescServer -lpthread

clean:
	rm -f rescClient
	rm -f rescServer
	

