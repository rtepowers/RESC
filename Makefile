all: rescSystem
rescSystem: 
	g++ rescClient.cpp -o rescClient -lcurses -lpthread
	g++ rescServer.cpp -o rescServer -lpthread
	g++ rescApiBot.cpp -o rescApiBot -lpthread

clean:
	rm -f rescClient
	rm -f rescServer
	rm -f rescApiBot
	

