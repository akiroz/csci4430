CC = gcc
CFLAGS = -O2 -Wall
LIBS = -lpthread
OBJS = mygbn.o

%.o: %.c $.h
	$(CC) $(CFLAGS) -c $< -o $@

all: myftpserver myftpclient

myftpserver: myftpserver.c $(OBJS)
	${CC} $(CFLAGS) -o $@ myftpserver.c $(OBJS) ${LIBS} 

myftpclient: myftpclient.c $(OBJS)
	${CC} $(CFLAGS) -o $@ myftpclient.c $(OBJS) ${LIBS}

clean:
	rm *.o myftpserver myftpclient
