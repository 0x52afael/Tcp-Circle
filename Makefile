TCP=tcpnode
UDP=udpnode
FLAGS=-Wall -Werror -std=c99
CC=gcc
EXTRA=-D_POSIX_SOURCE -D_GNU_SOURCE -lpthread 


all: tcpnode udpnode

tcpnode: tcpnode.c 
	$(CC) $(FLAGS) $(EXTRA) $(TCP).c -o $(TCP)

udpnode: udpnode.c 
	$(CC) $(FLAGS) $(UDP).c -o $(UDP)
clean:
	rm -f *.o $(TCP) $(UDP)
