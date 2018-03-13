/*
 * myftpclient.c
 *
 * - A simple client program
 */

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include "myftp.h"

int main(int argc, char *argv[]) {
	struct hostent *ht;
	struct sockaddr_in servaddr;
	int sd;

	// Usage
	if (argc != 4) {
		fprintf(stderr, "Usage: %s <ip address> <port> <filename>\n", argv[0]); 
		exit(-1);
	}

	/* create socket */
	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("ERROR: Cannot create socket\n");
		exit(-1);
	}

	/* fill up destination info in servaddr */
	memset(&servaddr, 0, sizeof(servaddr));
	ht = gethostbyname(argv[1]);
	if (ht == NULL) {
		perror("ERROR: invalid host.\n");
		exit(-1);
	}
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(argv[2]));
	memcpy(&servaddr.sin_addr, ht->h_addr, ht->h_length);

	// associate the opened socket with the destination's address
	if (connect(sd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
		perror("ERROR: connect failed.\n");
		exit(-1);
	}

	// Read the file
	int ifd = open(argv[3], O_RDONLY); 
	if (ifd == -1) {
		fprintf(stderr, "ERR: %s doesn't exist\n", argv[1]);
		exit(-1);
	}

	// determine the size of a file	
	unsigned int filesize = (unsigned int)lseek(ifd, 0, SEEK_END);

	// read-ahead and mmap	
	posix_fadvise(ifd, 0, filesize, POSIX_FADV_WILLNEED);
	uint8_t* data = (uint8_t*)mmap(0, filesize, PROT_READ, MAP_SHARED, ifd, 0);
	
	// send data.. 
	
	// (1) send filename size
	unsigned int filenamesize = strlen(argv[3]);
	filenamesize = htonl(filenamesize); 
	sendn(sd, &filenamesize, sizeof(unsigned int));
	filenamesize = ntohl(filenamesize);

	// (2) send file name 
	sendn(sd, argv[3], filenamesize); 

	// (3) send file size 
	filesize = htonl(filesize);
	sendn(sd, &filesize, sizeof(unsigned int));
	filesize = ntohl(filesize); 

	// (4) send actual file
	sendn(sd, data, filesize); 

	// unmap, close the file
	munmap(data, filesize);
	close(ifd);

	// close the socket
	close(sd);

	return 0;
}
