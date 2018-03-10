//
// Created by softfeta on 3/2/18.
//

#ifndef ASGN2_MYGBN_H
#define ASGN2_MYGBN_H

#define MAX_PAYLOAD_SIZE 512

#define GBN_DATA        0xA0
#define GBN_ACK            0xA1
#define GBN_END            0xA2

struct MYGBN_Packet {
    unsigned char protocol[3];                  /* protocol string (3 bytes) "gbn" */
    unsigned char type;                         /* type (1 byte) */
    unsigned int seqNum;                        /* sequence number (4 bytes) */
    unsigned int length;                        /* length(header+payload) (4 bytes) */
    unsigned char payload[MAX_PAYLOAD_SIZE];    /* payload data */
};

struct mygbn_sender {
    int sd; // GBN sender socket
    pthread_t clientThreadTimer;
    pthread_t clientThreadRecv;
    // ... other member variables
};

struct sndpkt {
	struct MYGBN_Packet *current;
	struct MYGBN_Packet *next;
};

void mygbn_init_sender(struct mygbn_sender *mygbn_sender, char *ip, int port, int N, int timeout);

int mygbn_send(struct mygbn_sender *mygbn_sender, unsigned char *buf, int len);

void mygbn_close_sender(struct mygbn_sender *mygbn_sender);

struct mygbn_receiver {
    int sd; // GBN receiver socket
    // ... other member variables
};

void mygbn_init_receiver(struct mygbn_receiver *mygbn_receiver, int port);

int mygbn_recv(struct mygbn_receiver *mygbn_receiver, unsigned char *buf, int len);

void mygbn_close_receiver(struct mygbn_receiver *mygbn_receiver);



struct MYGBN_Packet new_gbn_pkg(unsigned char type);

void *timer_thread(void *arg);

void *rcv_thread(void *arg);

void *rcv_thread_server(void *arg);

#endif
