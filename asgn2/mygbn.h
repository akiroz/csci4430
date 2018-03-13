/*
 * mygbn.h
 */

#ifndef __mygbn_h__
#define __mygbn_h__

#include <pthread.h>
#include <netdb.h>

#define MAX_PAYLOAD_SIZE 512

#define GBN_DATA_PACKET 0xA0
#define GBN_ACK_PACKET 0xA1
#define GBN_END_PACKET 0xA2

#define GBN_HEADER_SIZE 12

struct MYGBN_Packet {
  unsigned char protocol[3];                  /* protocol string (3 bytes) "gbn" */
  unsigned char type;                         /* type (1 byte) */
  unsigned int seqNum;                        /* sequence number (4 bytes) */
  unsigned int length;                        /* length(header+payload) (4 bytes) */
  unsigned char payload[MAX_PAYLOAD_SIZE];    /* payload data */
} __attribute__((packed));

struct mygbn_queue {
  void* payload;
  unsigned int length;
};

struct mygbn_sender {
  int sd; // GBN sender socket
  int N;
  int timeout;
  unsigned int base;
  unsigned int next;
  int sent_len;
  struct mygbn_queue *queue;
  struct addrinfo *srv;
  pthread_mutex_t lock;
  pthread_cond_t queue_cond;
  pthread_cond_t send_cond;
  pthread_cond_t ack_cond;
  pthread_t timeout_thread;
  pthread_t ack_thread;
};

void mygbn_init_sender(struct mygbn_sender* mygbn_sender, char* ip, int port, int N, int timeout);
int mygbn_send(struct mygbn_sender* mygbn_sender, unsigned char* buf, int len);
void mygbn_close_sender(struct mygbn_sender* mygbn_sender);

struct mygbn_receiver {
  int sd; // GBN receiver socket
  unsigned int ackNum;
  struct MYGBN_Packet pkt;
  unsigned int payload_offset;
};

void mygbn_init_receiver(struct mygbn_receiver* mygbn_receiver, int port);
int mygbn_recv(struct mygbn_receiver* mygbn_receiver, unsigned char* buf, int len);
void mygbn_close_receiver(struct mygbn_receiver* mygbn_receiver);

#endif
