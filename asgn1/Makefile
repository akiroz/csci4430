all: myftpserver myftpclient

myftpserver: myftp.h myftp.c myftpserver.c
	gcc -o myftpserver -lpthread myftp.c myftpserver.c

myftpclient: myftp.h myftp.c myftpclient.c
	gcc -o myftpclient myftp.c myftpclient.c

clean:
	rm -rf *.o *.dSYM myftpserver myftpclient
