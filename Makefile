all: rescSystem
rescSystem: 
	g++ rescClient2.cpp -o rescClient2 -lcurses -lpthread
	g++ rescd.cpp -o rescd -lpthread

clean:
	rm -f rescClient2
	rm -f rescd
	

