/*
 * myftpclient.c
 *
 * - A simple client program
 */

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include "mygbn.h"

int main(int argc, char *argv[]) {
  // Usage
  if (argc != 6) {
    fprintf(stderr, "Usage: %s <ip address> <port> <filename> <N> <timeout>\n", argv[0]);
    exit(-1);
  }

  // parse input
  char* ip = argv[1];
  int port = atoi(argv[2]);
  char* filename = argv[3];
  int N = atoi(argv[4]);
  int timeout = atoi(argv[5]);

  // mygbn_sender
  struct mygbn_sender sender; 

  // init mygbn_send
  mygbn_init_sender(&sender, ip, port, N, timeout);

  // Read the file
  int ifd = open(filename, O_RDONLY);
  if (ifd == -1) {
    fprintf(stderr, "file does not exists!\n");
    exit(-1);
  }

  // determine the size of a file
  int filesize = (int)lseek(ifd, 0, SEEK_END);
  
  // read-ahead and mmap 
  posix_fadvise(ifd, 0, filesize, POSIX_FADV_WILLNEED);
  uint8_t* data = (uint8_t*)mmap(0, filesize, PROT_READ, MAP_SHARED, ifd, 0);

  // send data.. 
  // (1) send filename size
  int filenamesize = strlen(filename);
  filenamesize = htonl(filenamesize);
  mygbn_send(&sender, (unsigned char*)&filenamesize, sizeof(filenamesize));
  filenamesize = ntohl(filenamesize);
  fprintf(stdout, "APP::send filename size = %d\n", filenamesize);
  
  // (2) send file name 
  mygbn_send(&sender, (unsigned char*)filename, filenamesize); 
  fprintf(stdout, "APP::send filename = %s\n", filename);
  
  // (3) send file size 
  filesize = htonl(filesize);
  mygbn_send(&sender, (unsigned char*)&filesize, sizeof(filesize));
  filesize = ntohl(filesize); 
  fprintf(stdout, "APP::send filesize = %d\n", filesize);
  
  // (4) send actual file
  mygbn_send(&sender, data, filesize); 

  // unmap, close the file
  munmap(data, filesize);
  close(ifd);
  
  // close client
  mygbn_close_sender(&sender);

  return 0;
}
