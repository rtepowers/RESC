all: rescSystem
rescSystem: 
	g++ rescClient2.cpp -o rescClient2 -lcurses -lpthread
	g++ rescAuth.cpp -o rescAuth -lpthread

clean:
	rm -rf rescClient2
	rm -rf rescAuth
	

