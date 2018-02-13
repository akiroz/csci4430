#include "mygbn.h"

void mygbn_init_sender(struct mygbn_sender* mygbn_sender, char* ip, int port, int N, int timeout){

}

int mygbn_send(struct mygbn_sender* mygbn_sender, unsigned char* buf, int len){
  return 0;
}

void mygbn_close_sender(struct mygbn_sender* mygbn_sender){

}

void mygbn_init_receiver(struct mygbn_receiver* mygbn_receiver, int port){

}

int mygbn_recv(struct mygbn_receiver* mygbn_receiver, unsigned char* buf, int len){
  return 0;
}

void mygbn_close_receiver(struct mygbn_receiver* mygbn_receiver) {

}
