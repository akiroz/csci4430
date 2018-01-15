arch := $(shell uname -m)
kernel := $(shell uname -s)

cflags = -std=gnu99 

ifeq ($(kernel),SunOS)
	cflags += -lsocket -lnsl
endif

all: myftpserver myftpclient

myftpserver: myftp.h myftp.c myftpserver.c
	gcc $(cflags) -lpthread -o myftpserver-$(arch)-$(kernel) myftp.c myftpserver.c

myftpclient: myftp.h myftp.c myftpclient.c
	gcc $(cflags) -o myftpclient-$(arch)-$(kernel) myftp.c myftpclient.c

clean:
	rm -rf *.o *.dSYM myftpserver-$(arch)-$(kernel) myftpclient-$(arch)-$(kernel)
