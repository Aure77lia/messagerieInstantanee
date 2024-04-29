#server:	server.o 
#	gcc -o server server.o -lm
server.o: server.c
	gcc -Wall -c server.c
clean:
	rm *.o server
