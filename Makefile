all: rescSystem
rescSystem: 
	g++ rescClient.cpp -o rescClient -lcurses -lpthread
	g++ rescServer.cpp -o rescServer -lpthread
	g++ rescTracker.cpp -o rescTracker -lpthread

clean:
	rm -rf rescClient
	rm -rf rescServer
	rm -rf rescTracker
	

