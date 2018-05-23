CC=gcc


p2p: p2p.o
	$(CC) p2p.o -o p2p  -lpthread
  

p2p.o: p2p.c
	$(CC) p2p.c -c -o p2p.o
  
  
p2p_s: p2p_s.o
	$(CC) p2p_s.o -o p2p_s  -lpthread
p2p_s.o: p2p_s.c
	$(CC) p2p_s.c -c -o p2p_s.o
	
p2p_c: p2p_c.o
	$(CC) p2p_c.o -o p2p_c  -lpthread
p2p_c.o: p2p_c.c
	$(CC) p2p_c.c -c -o p2p_c.o

clean:
	rm -f *.o p2p
