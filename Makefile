all: rescSystem
rescSystem: 
	g++ rescClient2.cpp -o rescClient2 -lcurses -lpthread
	g++ rescAuth.cpp -o rescAuth -lpthread
	g++ rescd.cpp -o rescd -lpthread

clean:
	rm -rf rescClient2
	rm -rf rescd
	rm -rf rescAuth
	

