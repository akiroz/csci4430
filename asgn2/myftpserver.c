/*
 * myftpserver.c
 *
 * - A tcp server supporting multiple clients
 */

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>

#include "mygbn.h"

int main(int argc, char** argv){
  // Usage
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <port>\n", argv[0]);
    exit(-1);
  }

  // parse input
  int port = atoi(argv[1]);

  // mygbn_receiver
  struct mygbn_receiver receiver;

  // init mygbn_receiver
  mygbn_init_receiver(&receiver, port);

  while(1) {
    // (1) read filename size
    int filenamesize;
    mygbn_recv(&receiver, (unsigned char*)&filenamesize, sizeof(filenamesize));
    filenamesize = ntohl(filenamesize);
    printf("APP::filenamesize = %d\n", filenamesize);

    // (2) read filename 
    unsigned char* filename = (unsigned char*)calloc(filenamesize, sizeof(unsigned char));
    mygbn_recv(&receiver, filename, filenamesize);
    printf("APP::filename = %s\n", filename);

    // (3) read file size
    int filesize;
    mygbn_recv(&receiver, (unsigned char*)&filesize, sizeof(filesize));
    filesize = ntohl(filesize);
    printf("APP::filesize = %d\n", filesize);

    // (4) read file
    int f_left = filesize;
    int recv_size;
    char ofilename[256];
    sprintf(ofilename, "data/%s", filename);
    FILE* ofp = fopen(ofilename, "wb");
    char buff[4096];
    while (f_left > 0) {
      memset(buff, 0, sizeof(buff));
      recv_size = mygbn_recv(&receiver, (unsigned char*)buff, sizeof(buff));
      f_left -= recv_size;
      fwrite(buff, 1, recv_size, ofp);
    }
    fclose(ofp);
  }

  mygbn_close_receiver(&receiver);
  return 0;
}

