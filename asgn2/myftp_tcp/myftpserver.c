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
#include "myftp.h"

void* process_th(int* s) {
	char filename[256];
	unsigned int filenamesize; 
	unsigned int filesize; 
	char buff[BUF_SIZE];
	int len;
	int clisd = *s;

	// detach myself, so that all resources are freed after I quit
	pthread_detach(pthread_self());

	// (1) read filename size	
	len = recvn(clisd, &filenamesize, sizeof(unsigned int));
	filenamesize = ntohl(filenamesize);
	
	// (2) read filename 
	memset(filename, 0, sizeof(filename)); 
	len = recvn(clisd, filename, filenamesize);
	printf("Server receives a filename %s (%d)\n", filename, len);
		
	// (3) read file size 
	len = recvn(clisd, &filesize, sizeof(unsigned int));
	filesize = ntohl(filesize); 
	printf("Server receives a file size %u (%d)\n", filesize, len);

	// (4) read the actual file content
	unsigned int f_left = filesize; 
	unsigned int recv_size; 
	char ofilename[256];
	sprintf(ofilename, "data/%s", filename); 	
	FILE* ofp = fopen(ofilename, "wb"); 
	while (f_left > 0) {
		memset(buff, 0, sizeof(buff));
		recv_size = f_left < sizeof(buff) ? f_left : sizeof(buff);
		len = recvn(clisd, buff, recv_size); 
		if (len < 0) {
			fprintf(stderr, "ERR: something goes wrong in receiving file\n"); 
			break;
		}	
		fwrite(buff, 1, len, ofp); 
		f_left -= len; 
	}
	fclose(ofp); 
	printf("Server receives the whole file.\n"); 

	close(clisd);
	return NULL;
}


int main(int argc, char** argv){
	struct sockaddr_in servaddr, cliaddr;
	unsigned int cliaddrlen;
	int servsd;
	int	clisd;
	int one = 1;
	pthread_t client_thread;

	// Usage
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <port>\n", argv[0]); 
		exit(-1);
	}

	/* create socket */	
	if ((servsd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("ERROR: cannot create socket\n");
		exit(-1);
	}
	
	/* set socket option */
	if (setsockopt(servsd, SOL_SOCKET, SO_REUSEADDR, (char*)&one, sizeof(one))
			< 0) {
		perror("ERROR: cannot set socket option\n");
		exit(-1);
	}
	
	/* prepare the address structure */	
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(argv[1]));
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
										/* or inet_addr("137.189.89.182") */

	/* bind the socket to network structure */
	if (bind(servsd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		perror("Can't bind\n");
		exit(-1);
	}

	/* listen for any pending request */
	if (listen(servsd, 3) < 0) {
		perror("Can't listen\n");
		exit(-1);
	}

	printf("Server is ready.....\n");

	/* get the size of the client address sturcture */
	cliaddrlen = sizeof(cliaddr);
	
	while (1) {	
		clisd = accept(servsd, (struct sockaddr *)&cliaddr, &cliaddrlen);
	/*	clisd = accept(servsd, NULL, NULL);  <-- this is accepted if you don't
	 *	need client information */
		
		printf("Connected......\n");

		pthread_create(&client_thread, NULL, (void*)process_th, &clisd);
	}
	
	/* Control never goes here */
	return 0;
}

