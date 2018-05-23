CC=gcc


p2p: p2p.o
	$(CC) p2p.o -o p2p  -lpthread
  

p2p.o: p2p.c
	$(CC) p2p.c -c -o p2p.o
  

clean:
	rm -f *.o p2p
