all: myftpserver myftpclient

myftpserver: myftp.h myftp.c myftpserver.c
	gcc -o myftpserver -g -lpthread myftp.c myftpserver.c

myftpclient: myftp.h myftp.c myftpclient.c
	gcc -o myftpclient -g myftp.c myftpclient.c

clean:
	rm -f *.o *.dSYM myftpserver myftpclient
