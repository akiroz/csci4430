all:
	gcc -o nat nat.c checksum.c -lnfnetlink -lnetfilter_queue -lpthread

clean:
	rm gcc -O2 -Wall -f nat
