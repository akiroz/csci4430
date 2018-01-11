all: myftpserver myftpclient

myftpserver:
	gcc -o myftpserver -lpthread myftp.c myftpserver.c

myftpclient:
	gcc -o myftpclient myftp.c myftpclient.c

clean:
	rm -f *.o myftpserver myftpclient
