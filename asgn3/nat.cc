#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include <libnetfilter_queue/libnetfilter_queue.h>

#include "state_table.h"

struct nfq_handle* nfq;
struct nfq_q_handle* qh;

void die(char eno_set, const char* reason) {
  if(eno_set) perror(reason);
  else printf("%s\n", reason);
  exit(1);
}

void trace(const char* fmt, ...) {
  va_list args; va_start(args, fmt);
  if(1) {
    vprintf(fmt, args);
    printf("\n");
  }
}

void shutdown(int sig) {
  if(nfq != NULL) {
    if(nfq_close(nfq) != 0) die(0, "nfq_close");
  }
  exit(0);
}



void main(int argc, char *argv[]) {
  // TODO: parse args

  signal(SIGHUP,  shutdown);
  signal(SIGINT,  shutdown);
  signal(SIGQUIT, shutdown);
  signal(SIGABRT, shutdown);
  signal(SIGTERM, shutdown);

  if(!(nfq = nfq_open()))             die(0, "nfq_open");
  if(nfq_unbind_pf(nfq, AF_INET) < 0) die(0, "nfq_unbind_pf");
  if(nfq_bind_pf(nfq, AF_INET) < 0)   die(0, "nfq_bind_pf");

  if(!(qh = nfq_create_queue(nfq, 0, &packet_handle, NULL))) {
    die(0, "nfq_create_queue");
  }
  if(nfq_set_mode(qh, NFQNL_COPY_PACKET, 0xffff) < 0) {
    die(0, "nfq_set_mode");
  }

  
  }
}
