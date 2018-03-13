#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>

#include "mygbn.h"

void die(char eno_set, const char* reason) {
  if(eno_set) perror(reason);
  else printf("%s\n", reason);
  exit(1);
}

void trace(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  if(1) {
    vprintf(fmt, args);
    printf("\n");
  }
}

struct addrinfo* udp_addrinfo(
    char* ip,
    int port,
    int flags
    )
{
  struct addrinfo *host;
  struct addrinfo hints = {
    .ai_family    = AF_UNSPEC,
    .ai_socktype  = SOCK_DGRAM,
    .ai_protocol  = IPPROTO_UDP,
    .ai_flags     = flags
  };
  char port_str[6]; sprintf(port_str, "%d", port);
  int rc = getaddrinfo( ip, port_str, &hints, &host );
  if( rc != 0 ) die(0, gai_strerror(rc));

  return host;
}

int open_socket(
    char* ip,
    int port,
    int flags
    )
{
  trace("open_socket(%s, %d)", ip, port);

  // Get addr info
  struct addrinfo *host = udp_addrinfo(ip, port, flags);

  // Create socket
  int sd = socket(
      host->ai_family,
      host->ai_socktype,
      host->ai_protocol);
  if( sd == -1 ) die(1, "socket");

  // Bind socket
  int rc = bind( sd, host->ai_addr, host->ai_addrlen );
  if( rc != 0 ) die(1, "bind");

  // Free addr info
  freeaddrinfo(host);

  return sd;
}

int mygbn_send_packet(
    int sd,
    unsigned char type,
    unsigned int seqNum,
    unsigned int payload_len,
    void* payload,
    struct sockaddr *addr,
    socklen_t addrlen
    )
{
  trace("mygbn_send_packet(0x%X, seq=%d, len=%d)", type, seqNum, payload_len);
  struct MYGBN_Packet pkt = {
    .protocol = "gbn",
    .type = type,
    .seqNum = seqNum,
    .length = GBN_HEADER_SIZE + payload_len
  };
  if(payload_len > 0) {
    memcpy(pkt.payload, payload, payload_len);
  }
  return sendto( sd, &pkt, pkt.length, 0, addr, addrlen );
}

void* timeout_thread( void* arg ) {
  trace("timeout_thread()");
  struct mygbn_sender* mygbn_sender = arg;
  pthread_mutex_t *lock = &mygbn_sender->lock;
  pthread_cond_t *send_cond = &mygbn_sender->send_cond;
  pthread_cond_t *ack_cond = &mygbn_sender->ack_cond;
  pthread_cond_t *queue_cond = &mygbn_sender->queue_cond;
  unsigned int *base = &mygbn_sender->base;
  unsigned int *next = &mygbn_sender->next;
  int rc;

  pthread_mutex_lock(lock);
  while(1) {

    // wait for sent & unAck'd packet
    while(*base >= *next) {
      trace("timeout_thread:wait:send_cond(base=%d, next=%d)", *base, *next);
      rc = pthread_cond_wait(send_cond, lock);
      if(rc != 0) perror("pthread_cond_wait");
      trace("timeout_thread:send_cond");
    }

    // Set timeout
    struct timeval now;
    struct timespec timeout;
    gettimeofday(&now, NULL);
    timeout.tv_sec = now.tv_sec + mygbn_sender->timeout;
    timeout.tv_nsec = now.tv_usec * 1000;
    trace("timeout_thread:timewait:ack_cond");
    switch(pthread_cond_timedwait(ack_cond, lock, &timeout)) {

      case ETIMEDOUT:
        // Timeout, trigger retransmit
        trace("timeout_thread:timeout");
        *next = *base;
        pthread_cond_signal(queue_cond);
        break;

      case 0:
        // Ack recieved
        trace("timeout_thread:ack_cond");
        break;

      default:
        perror("pthread_cond_timedwait");
        break;
    }
  }
  return NULL;
}

void* ack_thread( void* arg ) {
  trace("ack_thread()");
  struct mygbn_sender* mygbn_sender = arg;
  int sd = mygbn_sender->sd;
  pthread_mutex_t *lock = &mygbn_sender->lock;
  pthread_cond_t *ack_cond = &mygbn_sender->ack_cond;
  pthread_cond_t *queue_cond = &mygbn_sender->queue_cond;
  while(1) {

    // Recv packet
    struct MYGBN_Packet pkt;
    int rc = recvfrom( sd, &pkt, sizeof pkt, 0, NULL, NULL );
    if(rc < 0) {
      perror("recvfrom");
      continue;
    }

    // Validate packet
    if( memcmp( pkt.protocol, "gbn", 3 ) != 0 ||
        pkt.type != GBN_ACK_PACKET) {
      printf("Invalid GBN ACK packet");
      continue;
    }

    // Ack recieved, update base
    pthread_mutex_lock(lock);
    trace("ack_thread:ack(%d)", pkt.seqNum);
    unsigned int *base = &mygbn_sender->base;
    for(; *base <= pkt.seqNum; *base += 1) {
      int i = *base % mygbn_sender->N;
      mygbn_sender->sent_len += mygbn_sender->queue[i].length;
      trace("ack_thread:ack:sent_len(seq=%d, len=%d)",
          *base, mygbn_sender->sent_len);
    }
    pthread_cond_signal(ack_cond);
    pthread_cond_signal(queue_cond);
    pthread_mutex_unlock(lock);
  }
  return NULL;
}

void mygbn_init_sender(
    struct mygbn_sender* mygbn_sender,
    char* ip,
    int port,
    int N,
    int timeout
    )
{
  trace("mygbn_init_sender(ip=%s, port=%d, N=%d, timeout=%d)", ip, port, N, timeout);
  // Open socket & init state
  mygbn_sender->sd = open_socket(NULL, 0, AI_PASSIVE);
  mygbn_sender->srv = udp_addrinfo(ip, port, 0);
  mygbn_sender->N = N;
  mygbn_sender->timeout = timeout;
  mygbn_sender->base = 1;
  mygbn_sender->next = 1;
  mygbn_sender->queue = calloc(N, sizeof(struct mygbn_queue));

  // Init threads
  int rc = pthread_mutex_init(&mygbn_sender->lock, NULL);
  if(rc != 0) die(1, "pthread_mutex_init");
  rc = pthread_cond_init(&mygbn_sender->queue_cond, NULL);
  if(rc != 0) die(1, "pthread_cond_init");
  rc = pthread_cond_init(&mygbn_sender->send_cond, NULL);
  if(rc != 0) die(1, "pthread_cond_init");
  rc = pthread_cond_init(&mygbn_sender->ack_cond, NULL);
  if(rc != 0) die(1, "pthread_cond_init");
  rc = pthread_create(
      &mygbn_sender->timeout_thread, NULL, timeout_thread, mygbn_sender);
  if(rc != 0) die(1, "pthread_create");
  rc = pthread_create(
      &mygbn_sender->ack_thread, NULL, ack_thread, mygbn_sender);
  if(rc != 0) die(1, "pthread_create");
}

int mygbn_send(
    struct mygbn_sender* mygbn_sender,
    unsigned char* buf,
    int len
    )
{
  trace("mygbn_send(len=%d)", len);
  int N = mygbn_sender->N;
  int sd = mygbn_sender->sd;
  struct addrinfo *srv = mygbn_sender->srv;
  pthread_mutex_t *lock = &mygbn_sender->lock;
  pthread_cond_t *queue_cond = &mygbn_sender->queue_cond;
  pthread_cond_t *send_cond = &mygbn_sender->send_cond;

  // Check valid len & buf
  if(len < 1 || buf == NULL) return 0;

  pthread_mutex_lock(lock);
  unsigned int *base = &mygbn_sender->base;
  unsigned int *next = &mygbn_sender->next;
  struct mygbn_queue *queue = mygbn_sender->queue;
  int *sent_len = &mygbn_sender->sent_len;
  *sent_len = 0;
  int queued_len = 0;
  unsigned int allocSeq = *next;

  while(*sent_len < len) {
    // Populate send queue
    while(queued_len < len && allocSeq <= *base + N - 1) {
      unsigned int i = allocSeq % N;
      queue[i].payload = buf + queued_len;
      queue[i].length = 
        (len - queued_len < MAX_PAYLOAD_SIZE)?
        len - queued_len:
        MAX_PAYLOAD_SIZE;
      trace("mygbn_send:alloc(seq=%d, i=%d, off=%d, len=%d)",
          allocSeq, i, queued_len, queue[i].length);
      queued_len += queue[i].length;
      if(*next == *base) {
        *next = allocSeq;
      }
      allocSeq += 1;
    }

    // Check if anything to send
    if(*next >= allocSeq) {
      trace("mygbn_send:wait:queue_cond");
      int rc = pthread_cond_wait(queue_cond, lock);
      if(rc != 0) perror("pthread_cond_wait");
      trace("mygbn_send:queue_cond");
      continue; // populate send queue again
    }
    
    // Send packets
    while(*next < allocSeq) {
      unsigned int i = *next % N;
      mygbn_send_packet(
          sd, GBN_DATA_PACKET, *next,
          queue[i].length, queue[i].payload,
          srv->ai_addr, srv->ai_addrlen);
      trace("mygbn_send:send(seq=%d, i=%d, len=%d)", *next, i, queue[i].length);
      pthread_cond_signal(send_cond);
      *next += 1;
    }
  
  }

  pthread_mutex_unlock(lock);
  return *sent_len;
}

void mygbn_close_sender(
    struct mygbn_sender* mygbn_sender
    )
{
  trace("mygbn_close_sender()");
  int sd = mygbn_sender->sd;
  struct addrinfo *srv = mygbn_sender->srv;
  pthread_mutex_t *lock = &mygbn_sender->lock;
  pthread_cond_t *queue_cond = &mygbn_sender->queue_cond;
  pthread_cond_t *send_cond = &mygbn_sender->send_cond;
  pthread_mutex_lock(lock);

  // Send end packet
  int retry_count = 0;
  unsigned int original_base = mygbn_sender->base;
  while(1) {
    int rc = mygbn_send_packet(
        sd, GBN_END_PACKET, mygbn_sender->next,
        0, NULL, srv->ai_addr, srv->ai_addrlen);
    if(rc < 0) perror("mygbn_queue_send");
    pthread_cond_signal(send_cond);
    mygbn_sender->next += 1;
    trace("mygbn_close_sender:send_end(retry=%d)", retry_count);

    trace("mygbn_close_sender:wait:queue_cond");
    rc = pthread_cond_wait(queue_cond, lock);
    if(rc != 0) perror("pthread_cond_wait");
    trace("mygbn_close_sender:queue_cond");

    // Check if base is updated
    if(mygbn_sender->base == original_base) {
      if(retry_count < 3) {
        retry_count += 1;
        continue;
      } else {
        printf("Failed to send end packet\n");
        break;
      }
    } else {
      break;
    }
  }

  // Cleanup threads
  int rc = pthread_cancel( mygbn_sender->ack_thread );
  if(rc != 0) die(1, "pthread_cancel");
  rc = pthread_cancel( mygbn_sender->timeout_thread );
  if(rc != 0) die(1, "pthread_cancel");
  rc = pthread_cond_destroy( &mygbn_sender->queue_cond );
  if(rc != 0) die(1, "pthread_cond_destroy");
  rc = pthread_cond_destroy( &mygbn_sender->send_cond );
  if(rc != 0) die(1, "pthread_cond_destroy");
  rc = pthread_cond_destroy( &mygbn_sender->ack_cond );
  if(rc != 0) die(1, "pthread_cond_destroy");

  pthread_mutex_unlock(lock);
  rc = pthread_mutex_destroy( &mygbn_sender->lock );
  if(rc != 0) die(1, "pthread_mutex_destroy");

  // Close ack socket
  close( mygbn_sender->sd );
  freeaddrinfo( mygbn_sender->srv );
  free( mygbn_sender->queue );
}

void mygbn_init_receiver(
    struct mygbn_receiver* mygbn_receiver,
    int port
    )
{
  trace("mygbn_init_receiver(port=%d)", port);
  // Open socket & init state
  mygbn_receiver->sd = open_socket(NULL, port, AI_PASSIVE);
  mygbn_receiver->ackNum = 0;
  mygbn_receiver->payload_offset = 0;
}

int mygbn_recv(
    struct mygbn_receiver* mygbn_receiver,
    unsigned char* buf,
    int len
    )
{
  trace("mygbn_recv(len=%d)", len);
  int sd = mygbn_receiver->sd;
  unsigned int *ackNum = &mygbn_receiver->ackNum;
  struct MYGBN_Packet *pkt = &mygbn_receiver->pkt;

  int recv_len = 0;
  while(recv_len < len) {
    if(mygbn_receiver->payload_offset != 0) {
      unsigned int remaining_size = pkt->length;
      remaining_size -= GBN_HEADER_SIZE;
      remaining_size -= mygbn_receiver->payload_offset;
      trace("mygbn_recv:payload_offset %d (remain=%d)",
          mygbn_receiver->payload_offset, remaining_size);
      if(len > remaining_size) {
        memcpy(buf, pkt->payload + mygbn_receiver->payload_offset, remaining_size);
        mygbn_receiver->payload_offset = 0;
        recv_len += remaining_size;
      } else {
        memcpy(buf, pkt->payload + mygbn_receiver->payload_offset, len);
        mygbn_receiver->payload_offset += len;
        return len;
      }
    }

    // Recv packet
    struct sockaddr src_addr;
    socklen_t src_len = sizeof(src_addr);
    int rc = recvfrom(
        sd, pkt, sizeof(struct MYGBN_Packet),
        0, &src_addr, &src_len);
    if(rc < 0) {
      perror("recvfrom");
      return -1;
    }

    // Validate packet
    if( memcmp( pkt->protocol, "gbn", 3 ) != 0 ) {
      printf("Invalid GBN packet\n");
      continue;
    }

    // Discard out-of-order packet
    if(pkt->seqNum != *ackNum + 1) {
      printf("Drop out-of-order packet: %u/%u\n", pkt->seqNum, *ackNum);
      if( pkt->seqNum <= *ackNum) {
        rc = mygbn_send_packet(
            sd, GBN_ACK_PACKET, pkt->seqNum, 0, NULL, &src_addr, src_len);
        if(rc < 0) perror("mygbn_send_packet");
      }
      continue;
    } else {
      trace("mygbn_recv:packet(type=0x%X, seq=%d, len=%d)",
          pkt->type, pkt->seqNum, pkt->length);
    }

    // Ack packet
    *ackNum += 1;
    rc = mygbn_send_packet(
        sd, GBN_ACK_PACKET, *ackNum, 0, NULL, &src_addr, src_len);
    if(rc < 0) {
      perror("mygbn_send_packet");
      return -1;
    }

    // Check packet type
    int payload_len;
    switch(pkt->type) {
      case GBN_DATA_PACKET:
        // Copy data into buffer
        payload_len = pkt->length - GBN_HEADER_SIZE;
        trace("mygbn_recv:memcpy(off=%d, size=%d)", recv_len, payload_len);
        if(payload_len > len - recv_len) {
          unsigned int size = len - recv_len;
          memcpy(buf + recv_len, pkt->payload, size);
          mygbn_receiver->payload_offset = size;
          recv_len += size;
        } else {
          memcpy(buf + recv_len, pkt->payload, payload_len);
          recv_len += payload_len;
        }
        trace("mygbn_recv %d/%d", recv_len, len);
        break;

      case GBN_END_PACKET:
        // Reset cumulative ack
        *ackNum = 0;
        return recv_len;

      default:
        printf("Invalid GBN type: %X\n", pkt->type);
        continue;
    }

  }

  return recv_len;
}

void mygbn_close_receiver(
    struct mygbn_receiver* mygbn_receiver
    )
{
  // Cleanup resources
  close( mygbn_receiver->sd );
}

