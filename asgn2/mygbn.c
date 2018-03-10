#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>                     /* close(). */
#include <time.h>                       /* Timer. Not sure if signals are better than this or not. */
#include <sys/time.h>
#include "mygbn.h"

/*    ~~~~~~~~~~~~~~~~~~~~~~~SENDER/CLIENT~~~~~~~~~~~~~~~~~~~~~~~      */

int base, nextseqnum, mN, mTimeout;    //, trigger;

//pthreads in sender for receiving AckPackets & triggering retransmission
pthread_t clientThreadTimer, clientThreadRecv;
pthread_mutex_t mutex_timer = PTHREAD_MUTEX_INITIALIZER; /* Ensures no two I/Os (timeout retransmit may intercept!) to buffer at the same time... */
/* ...checked by clientThreadTimer and clientThreadSend. Critical region: Write packet to buffer+send buffer. */
pthread_cond_t signal = PTHREAD_COND_INITIALIZER;

//pointer for data packets
struct MYGBN_Packet *sndpkt;                /* TODO: Should not be this value! */

struct sndpkt first;

//global variables for server address and client address (also address length)
struct sockaddr_in server_addr, client_addr;
socklen_t server_addrlen, client_addrlen;

// TODO: pthread, UDP bind() etc
void mygbn_init_sender(struct mygbn_sender *mygbn_sender, char *ip, int port, int N, int timeout) {
	//define global variables
    base = 0;
    nextseqnum = 1;    //spec assumes sender initializes seqNum as 1
    mN = N;
    sndpkt = malloc(N * sizeof(struct MYGBN_Packet));
	mTimeout = timeout;
    //trigger = mTimeout;

	
    mygbn_sender->sd = socket(AF_INET, SOCK_DGRAM, 0);    //set up socket

    //no need to set reusable port for client
    /*
	int val = 1;
    if (setsockopt(mygbn_sender->sd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(int)) == -1) {
        perror("setsockopt() failed");
        exit(1);
    } 
	*/

	//client does not require bind()
    /*
	memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);
    server_addrlen = sizeof(server_addr);
	*/
}

/* ~~~~~~~~~~~~~~~~~~~~~~threads below~~~~~~~~~~~~~~~~~~~~~~ */

void *timer_thread(void *arg) {
    fprintf(stderr, "timer_thread started.\n");
    struct mygbn_sender *mygbn_sender = (struct mygbn_sender *) arg;
	struct timespec ts;
	struct timeval tp;

    for (;;) {    //sorry I used the TA's method (cond.c) to count the time, feel free to change
		pthread_mutex_lock(&mutex_timer);    //start locking the door
		
		gettimeofday(&tp, NULL);

		ts.tv_sec = tp.tv_sec;
		ts.tv_nsec = tp.tv_usec * 1000;
		ts.tv_sec += mTimeout;     // set wait deadline

		int rc;
		rc = pthread_cond_timedwait(&signal, &mutex_timer, &ts);    //wait for signal from other threads

		if (rc == ETIMEDOUT) {    //when timeout, retransmits up to N unacknowledged DataPackets
			int i;
            if ((base % mN) < (nextseqnum-1) % mN) {
                for (i=base % mN; i <= mN-1; i++) {
                    sendto(mygbn_sender->sd, &sndpkt[i], sizeof(struct MYGBN_Packet), 0, (struct sockaddr *) &server_addr,
                    server_addrlen);
                }
            } else if ((base % mN) > (nextseqnum-1) % mN) {
                for (i=base % mN; i <= mN-1; i++) {
                    sendto(mygbn_sender->sd, &sndpkt[i], sizeof(struct MYGBN_Packet), 0, (struct sockaddr *) &server_addr,
                    server_addrlen); 
                } 
                for (i=0; i <= (nextseqnum-1) % mN; i++) {
                    sendto(mygbn_sender->sd, &sndpkt[i], sizeof(struct MYGBN_Packet), 0, (struct sockaddr *) &server_addr,
                    server_addrlen);
                }
            }
            /*
			for (i = base; i <= nextseqnum - 1; ++i) {
                sendto(mygbn_sender->sd, &sndpkt[i], sizeof(struct MYGBN_Packet), 0, (struct sockaddr *) &server_addr,
				server_addrlen);
            }
			*/
		} else {    //when called by signal, then resets the timer
			gettimeofday(&tp, NULL);

			ts.tv_sec = tp.tv_sec;
			ts.tv_nsec = tp.tv_usec * 1000;
			ts.tv_sec += mTimeout;     // set wait deadline
		}
		pthread_mutex_unlock(&mutex_timer);    //unlocks the door
        
		// old method of counting time
		/*
		clock_t msec = 0, before = clock();
        do {
            pthread_mutex_unlock(&mutex_timer); //stop
            clock_t difference = clock() - before;
            msec = difference * 1000 / CLOCKS_PER_SEC;
            pthread_mutex_lock(&mutex_timer);   //start
        } while (msec < trigger);


        if (trigger == -1) {        // reset timer
            trigger = mTimeout;
        } else {
            
            pthread_mutex_unlock(&mutex_timer);
        }
		*/
    }
}

void *rcv_thread(void *arg) {
	//receive AckPackets
    fprintf(stderr, "rcv_thread started.\n");
    struct mygbn_sender *mygbn_sender = (struct mygbn_sender *) arg;
    for (;;) {
        struct MYGBN_Packet rcvpkt;

        if (recvfrom(mygbn_sender->sd, &rcvpkt, sizeof(rcvpkt), 0, (struct sockaddr *) &server_addr, &server_addrlen) >
            0) {
            if (rcvpkt.type == GBN_ACK && rcvpkt.seqNum == (base+1)) {    //when AckPacket is received & it's the oldest unACKed pkt
                base = rcvpkt.seqNum + 1;
                if ((base+1) != nextseqnum) {    //if there are still some unACKed packets in the window, reset the timer
                    pthread_mutex_lock(&mutex_timer);
					pthread_cond_signal(&signal);
					pthread_mutex_unlock(&mutex_timer);  
                }
            }
        }
    }
}

int mygbn_send(struct mygbn_sender *mygbn_sender, unsigned char *buf, int len) {
    // Check seqnum, move base, stop mutex_timer, (re)start mutex_timer
    int err;
    int total_bytes_read = 0;

	/*
    if (nextseqnum >= base + mN) {
        return -1;
    }
	*/

	//create receive packets thread
	err = pthread_create(&clientThreadRecv, NULL, rcv_thread, mygbn_sender);
    if (err != 0) {
        perror("Error: pthread_create");
        exit(1);
    }
	
    while (total_bytes_read < len) {    //send data until the last part of payload is sent
        //pthread_mutex_lock(&mutex_timer);
        if (base == 0) {    // Create timer thread for 1st time to make sure the receiver thread holds the lock.
            err = pthread_create(&clientThreadTimer, NULL, timer_thread, mygbn_sender);
            if (err != 0) {
                perror("Error: pthread_create");
                exit(1);
            }
        }
		if ((nextseqnum - base + 1) < 5){    //create more packets only when the window is not full
			int bytes_read = 0;
			sndpkt[nextseqnum % mN] = new_gbn_pkg(GBN_DATA);     // make_pkt(nextseqnum,data); , i.e. create DataPackets
			if (len - total_bytes_read >= 512) bytes_read = 512;
			else bytes_read = len - total_bytes_read;
			strncpy((char *) sndpkt[nextseqnum % mN].payload, (const char *) buf, (size_t) bytes_read);
			sndpkt[nextseqnum % mN].payload[bytes_read] = '\0';
			sndpkt[nextseqnum % mN].length = bytes_read;
			sndpkt[nextseqnum % mN].seqNum = nextseqnum;

			//send DataPacket to server, 
			sendto(mygbn_sender->sd, &sndpkt[nextseqnum % mN], sizeof(struct MYGBN_Packet), 0, (struct sockaddr *) &server_addr,
				server_addrlen);
			total_bytes_read += bytes_read;
			if (base == nextseqnum) {    //reset the timer
				//trigger = -1;
				pthread_mutex_lock(&mutex_timer);
				pthread_cond_signal(&signal);
				pthread_mutex_unlock(&mutex_timer);
			}
        nextseqnum++;
		}
    }
    return total_bytes_read;
}

void mygbn_close_sender(struct mygbn_sender *mygbn_sender) {
    struct timespec ts;
    struct timeval tp;
    struct MYGBN_Packet sndpckt = new_gbn_pkg(GBN_END);
	int failedTransmission = 0;

	while (1){
		pthread_mutex_lock(&mutex_timer);
    
		gettimeofday(&tp, NULL);

		ts.tv_sec = tp.tv_sec;
		ts.tv_nsec = tp.tv_usec * 1000;
		ts.tv_sec += mTimeout;     // set wait deadline

		sendto(mygbn_sender->sd, &sndpckt, sizeof(struct MYGBN_Packet), 0, (struct sockaddr *) &server_addr,
                    server_addrlen);    //send EndPacket for 1st time
	
		int rc;
		printf("mygbn_close_sender::wait for the signal or timeout!\n");
		rc = pthread_cond_timedwait(&signal, &mutex_timer, &ts);    //wait for signal from recvPacket thread

		if (rc == ETIMEDOUT) {    //if timeout
			printf("mygbn_close_sender::timing thread timeout!\n");
			failedTransmission++;
			if (failedTransmission < 3){    //if less than 3 transmission failure, retransmit EndPacket & reset timer
				sendto(mygbn_sender->sd, &sndpckt, sizeof(struct MYGBN_Packet), 0, (struct sockaddr *) &server_addr,
						server_addrlen);
				pthread_mutex_lock(&mutex_timer);
				pthread_cond_signal(&signal);
				pthread_mutex_unlock(&mutex_timer);
			} else {
				fprintf(stderr, "Retransmitted 3 times. Close socket anyway.\n");
				close(mygbn_sender->sd);
				exit(1);
			}
		}
	}
	exit(0);
}

/* ~~~~~~~~~~~~~~~~~~~~~~threads above~~~~~~~~~~~~~~~~~~~~~~ */

/*    ~~~~~~~~~~~~~~~~~~~~~~~~~~RECEIVER/SERVER~~~~~~~~~~~~~~~~~~~~~~~~~~      */

unsigned int cumulativeack, expectedseqnum;    //cumulativeack = last ack # received

//discard packet: read the seqnum of packet, if < expectedseqnum, clear buffer
//else send ACK. if packet not corrupt, if filename is null, set filename open file
//else wb that file

void mygbn_init_receiver(struct mygbn_receiver *mygbn_receiver, int port) {
    cumulativeack = 0;
    expectedseqnum = 1;

	//initialize socket
    mygbn_receiver->sd = socket(AF_INET, SOCK_DGRAM, 0);

    //reusable port
    int val = 1;
    if (setsockopt(mygbn_receiver->sd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(int)) == -1) {
        perror("setsockopt() failed");
        exit(1);
    }

	//bind port to socket
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);
    server_addrlen = sizeof(server_addr);

    if (bind(mygbn_receiver->sd, (struct sockaddr *) &server_addr, server_addrlen) < 0) {
        perror("bind() failed");
        exit(1);
    }
}

int mygbn_recv(struct mygbn_receiver *mygbn_receiver, unsigned char *buf, int len) {
    int size;
    struct MYGBN_Packet recvpk;
	
	//if the received packet has size larger than 0
    if ((size = (int) recvfrom(mygbn_receiver->sd, &recvpk, sizeof(recvpk), 0, (struct sockaddr *) &client_addr,
                               &client_addrlen)) > 0) {
		//if the packet is DataPacket
        if (recvpk.type == GBN_DATA) {
            struct MYGBN_Packet sndpckt = new_gbn_pkg(GBN_ACK);    //create an AckPacket
			//if the pkt received is the expected pkt, e.g. expect pkt #19 and the pkt received is #19
            if (recvpk.seqNum == expectedseqnum) {
                size_t payload_length = recvpk.length - 12;
                buf = malloc(payload_length);
                strncpy((char *) buf, (const char *) recvpk.payload, payload_length);    //copy payload in pkt into buffer
                buf[payload_length] = '\0';
                cumulativeack = expectedseqnum;    //adjust the numbers
                sndpckt.seqNum = cumulativeack;    //set seqnum attribute in AckPkt and send the packet
                sendto(mygbn_receiver->sd, &sndpckt, sizeof(sndpckt), 0, (struct sockaddr *) &client_addr,
                       client_addrlen);
                expectedseqnum++;
                return (int) payload_length;
            } else {
                //Discard packet if not the expected pkt
                sndpckt.seqNum = cumulativeack;
                sendto(mygbn_receiver->sd, &sndpckt, sizeof(sndpckt), 0, (struct sockaddr *) &client_addr,
                       client_addrlen);
                return -1;
            }
        } else if (recvpk.type == GBN_END) {    //if received EndPacket, send back AckPacket
            struct MYGBN_Packet sndpckt = new_gbn_pkg(GBN_ACK);
            sndpckt.seqNum = cumulativeack;
            sendto(mygbn_receiver->sd, &sndpkt, sizeof(sndpckt), 0, (struct sockaddr *) &client_addr, client_addrlen);
            cumulativeack = 0;    //set cumulative ack to 0
            expectedseqnum = 1;
            return -1;
        }
    } else return -1;
	return -1;
}

void mygbn_close_receiver(struct mygbn_receiver *mygbn_receiver) {
	//close the socket
    close(mygbn_receiver->sd);
    exit(0);
}

struct MYGBN_Packet new_gbn_pkg(unsigned char type) {
    struct MYGBN_Packet pkg = {
            .protocol = {'g', 'b', 'n'},
            .type     = type,
            .length   = 12                              //sizeof(struct MYGBN_Packet)
    };
    return pkg;
}