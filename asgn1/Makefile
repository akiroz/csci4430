arch := $(shell uname -m)
kernel := $(shell uname -s)

all: myftpserver myftpclient

myftpserver: myftp.h myftp.c myftpserver.c
	gcc -o myftpserver-$(arch)-$(kernel) -std=gnu99 -lpthread myftp.c myftpserver.c

myftpclient: myftp.h myftp.c myftpclient.c
	gcc -o myftpclient-$(arch)-$(kernel) -std=gnu99 myftp.c myftpclient.c

clean:
	rm -rf *.o *.dSYM myftpserver-$(arch)-$(kernel) myftpclient-$(arch)-$(kernel)
