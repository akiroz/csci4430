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
#include <pthread.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <time.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <linux/netfilter.h>  
#include <libnetfilter_queue/libnetfilter_queue.h>

#include "checksum.h"
#include "nat_table.c"

#define MAXPKTS 10

typedef struct threadargs {
	struct nfq_q_handle *qh;
	unsigned int id;
	int data_len;
	struct iphdr *iph;
	int thread_num;
} threadargs;

unsigned long internalIP, publicIP;
int subnetMask, bucketSize, fillRate, tokenBucket;
time_t start_t, end_t;
double diff_t;
pthread_t thread[MAXPKTS];
int threadUsage[MAXPKTS];
pthread_mutex_t pthread_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t bucket_mutex = PTHREAD_MUTEX_INITIALIZER;

/*  //Get publicIP and internalIP through code
void get_vma_addr() {
	struct ifaddrs *ifaddr, *ifa;
	struct sockaddr_in *sa;

	if (getifaddrs(&ifaddr) == -1) {
		fprintf(stderr, "error during getifaddrs()\n");
		exit(1);
	}

	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL)
			continue;

		sa = (struct sockaddr_in *) ifa->ifa_addr;
		if ((strcmp(ifa->ifa_name, "eth0") == 0) && (ifa->ifa_addr->sa_family == AF_INET)) {
			vma_internal_addr = sa->sin_addr.s_addr;
		} else if ((strcmp(ifa->ifa_name, "eth1") == 0) && (ifa->ifa_addr->sa_family == AF_INET)) {
			vma_public_addr = sa->sin_addr.s_addr;
		}
	}
	freeifaddrs(ifaddr);
}
*/

// function for generate & consume tokens
int token_process(){
	
	pthread_mutex_lock(&bucket_mutex);  //locking whole function so there won't be wrong time record or wrong no. of tokens left 
	
	// generate tokens
	time(&end_t);
	diff_t = difftime(end_t, start_t);  // calculating time difference ./. last token fill & now in terms of secs
	int num_of_new_tokens = (int) diff_t * fillRate;  // amount of tokens that should be put into the bucket
	tokenBucket += num_of_new_tokens;
	time(&start_t);  // restart time counting
	
	// if the token is full, discard excessive tokens
	if (tokenBucket > bucketSize){
		tokenBucket = bucketSize;
	}
	
	// if there is no tokens, return -1
	if (tokenBucket == 0){
		return -1;
	}
	
	tokenBucket--;  // consume a token if ready to transmit packet
	
	pthread_mutex_lock(&bucket_mutex);
	return 0;
}

// pthread function
void *packet_processing(void *args){
	struct threadargs *t_struct = (struct threadargs*)args;
	
	if (t_struct->iph->protocol == IPPROTO_TCP) {  // is TCP packet
		
		// retrieve TCP header & local mask
		struct tcphdr *tcph = (struct tcphdr*)(((char*)t_struct->iph) + (t_struct->iph->ihl << 2));
		unsigned int local_mask = 0xffffffff << (32 - subnetMask);
		NAT_table entry_info;
		int index;
		
		if ((ntohl(t_struct->iph->saddr) & local_mask) == (ntohl(internalIP) & local_mask)) {  // outbound traffic

			// get current source ip and port
			unsigned long source_addr;
			int source_port;
			source_addr = t_struct->iph->saddr;
			source_port = ntohs(tcph->source);
			
			if (tcph->syn == 1){
				// if SYN packet and no existing entry, create new entry
				if (search_table_source(source_addr, source_port) == -1){
					entry_info = insert_entry(source_addr, source_port, t_struct->iph->daddr);
				}
			}
			else {
				// not SYN or RST packet
				index = search_table_source(source_addr, source_port);
				if (index != -1){
					// found a matched entry, get information
					entry_info = get_table_entry(index);
					
					// if RST packet, delete any existing entry
					if (tcph->rst == 1){
						delete_entry(source_addr, source_port);
					}
					
					// check 4-way handshake, marking down the current step on "NAT_table.closing"
					if(tcph->fin == 1 && (entry_info.closing == 0 || entry_info.closing == 2)) {
						entry_info.closing++;
					}
					if (tcph->ack == 1 && (entry_info.closing == 1 || entry_info.closing == 3)){
						entry_info.closing++;
					}
				}
				else {
					//release a slot in pthread array before return
					pthread_mutex_lock(&pthread_mutex);  
					threadUsage[t_struct->thread_num] = 0;
					pthread_mutex_unlock(&pthread_mutex);
					
					// cannot find matched entry, drop packet
					return (void*) nfq_set_verdict(t_struct->qh, t_struct->id, NF_DROP, 0, NULL);
				}
			}
			// translate source ip and port
			t_struct->iph->saddr = publicIP;
			tcph->source = entry_info.translated_src_port;
			
			// checksum
			t_struct->iph->check = ip_checksum((unsigned char*)t_struct->iph);
			tcph->check = tcp_checksum((unsigned char*)tcph);
			
			// close handshake if received last ACK for FIN
			if (entry_info.closing == 4){
				delete_entry(source_addr, source_port);
			}
		}
		else {  // inbound traffic
		
			// get current destination ip and port
			unsigned long dest_addr;
			int dest_port;
			dest_addr = t_struct->iph->saddr;
			dest_port = ntohs(tcph->dest);
			
			// search for entry
			index = search_table_dest_port(dest_port);
			if (index != -1){
				
				// found a matched entry, get information
				entry_info = get_table_entry(index);
				
				// if RST packet, delete any existing entry and drop packet
				if (tcph->rst == 1){
					delete_entry(entry_info.original_src_ip, entry_info.original_src_port);
				}
				
				// check 4-way handshake, marking down the current step on "NAT_table.closing"
				if(tcph->fin == 1 && (entry_info.closing == 0 || entry_info.closing == 2)) {
					entry_info.closing++;
				}
				if (tcph->ack == 1 && (entry_info.closing == 1 || entry_info.closing == 3)){
					entry_info.closing++;
				}
			
				// translate destination ip and port
				t_struct->iph->daddr = entry_info.original_src_ip;
				tcph->dest = htons(entry_info.original_src_port);
			
				// checksum
				t_struct->iph->check = ip_checksum((unsigned char*)t_struct->iph);
				tcph->check = tcp_checksum((unsigned char*)tcph);
			
				// close handshake if received last ACK for FIN
				if (entry_info.closing == 4){
					delete_entry(entry_info.original_src_ip, entry_info.original_src_port);
				}
			} else {
				//release a slot in pthread array before return
				pthread_mutex_lock(&pthread_mutex);  
				threadUsage[t_struct->thread_num] = 0;
				pthread_mutex_unlock(&pthread_mutex);
				
				// cannot find matched entry, drop packet
				return (void*) nfq_set_verdict(t_struct->qh, t_struct->id, NF_DROP, 0, NULL);
			}
		}
	} 
	else {
		//release a slot in pthread array before return
		pthread_mutex_lock(&pthread_mutex);  
		threadUsage[t_struct->thread_num] = 0;
		pthread_mutex_unlock(&pthread_mutex);
		
		// not TCP packet, drop it
		return (void*) nfq_set_verdict(t_struct->qh, t_struct->id, NF_DROP, 0, NULL);
	}
	
	// if the packet has not dropped, forwards it
	struct timespec tim1, tim2;
	tim1.tv_sec = 0;
	tim1.tv_nsec = 5000;
	
	// until there is one token in the bucket, packet transmission is blocked
	while (token_process() == -1) {
		if (nanosleep(&tim1, &tim2) < 0){
			printf("error when waiting available token\n");
			exit(1);
		}
	}
	//release a slot in pthread array before return
	pthread_mutex_lock(&pthread_mutex);  
	threadUsage[t_struct->thread_num] = 0;
	pthread_mutex_unlock(&pthread_mutex);
		
	return (void*) nfq_set_verdict(t_struct->qh, t_struct->id, NF_ACCEPT, (t_struct->data_len+t_struct->iph->ihl<<2), (unsigned char*)t_struct->iph);
}

// called when receive a packet
int callback(struct nfq_q_handle *qh, struct nfgenmsg *nfmsg, struct nfq_data *nfad, void *data) {
	int i;
	void *tret;
	
	pthread_mutex_lock(&pthread_mutex);  //lock this door so two threads won't get the same slot
	// find available pthread
	for (i = 0; i < MAXPKTS; i++){
		if (threadUsage[i] == 0){
			threadUsage[i] = 1;
			break;
		}
	}
	pthread_mutex_unlock(&pthread_mutex);
	
	// NFQUEUE packet header
	struct nfqnl_msg_packet_hdr *header;
	header = nfq_get_msg_packet_hdr(nfad);

	// packet ID
	unsigned int id;
	if (header != NULL) {
		id = ntohl(header->packet_id);
	}

	// retrieve payload for queued packet and IP header
	char *payload;
	int data_len = nfq_get_payload(nfad, (char**)&payload);
	struct iphdr *iph = (struct iphdr*) payload;

	// if all the threads are unavailable, i.e. buffer full, drop the packet
	if (i == 10){
		return nfq_set_verdict(qh, id, NF_DROP, 0, NULL);
	}
	
	// initialize a thread struct
	struct threadargs tgs;
	tgs.qh = qh;
	tgs.id = id;
	tgs.data_len = data_len;
	tgs.iph = iph;
	tgs.thread_num = i;
	
	// create thread and join thread
	int ret_val = pthread_create(&thread[i], NULL, packet_processing, (void*)&tgs);
	for (i = 0; i < MAXPKTS; i++){
		pthread_join(thread[i], &tret);
	}
}

int main(int argc, char *argv[]) {
	struct nfq_handle *h;
	struct nfq_q_handle *qh;
	int fd;
	int rv;
	char buf[4096];
	
	// if the command input is incorrect
    if (argc != 6) {
        fprintf(stderr, "Usage: %s <public ip> <internal ip> <subnet mask> <bucket size> <fill rate>\n", argv[0]);
        exit(-1);
    }

	// assign arguments to variables
	struct in_addr *public_addr, *internal_addr;
	inet_aton(argv[1], public_addr);
	inet_aton(argv[2], internal_addr);
	publicIP = public_addr->s_addr;
	internalIP = internal_addr->s_addr;
	subnetMask = atoi(argv[3]);
	bucketSize = atoi(argv[4]);
	fillRate = atoi(argv[5]);
	
	// NFQUEUE flow
	h = nfq_open();
	if (!h) {
		fprintf(stderr, "error during nfq_open()\n");
		exit(1);
	}

	// unbinding existing nf_queue handler for AF_INET (if any)
	if (nfq_unbind_pf(h, AF_INET) < 0) {
		fprintf(stderr, "error during nfq_unbind_pf()\n");
		exit(1);
	}

	// binding nfnetlink_queue as nf_queue handler for AF_INET
	if (nfq_bind_pf(h, AF_INET) < 0) {
		fprintf(stderr, "error during nfq_bind_pf()\n");
		exit(1);
	}

	// binding this socket to queue '0'
	qh = nfq_create_queue(h, 0, &callback, NULL);
	if (!qh) {
		fprintf(stderr, "error during nfq_create_queue()\n");
		exit(1);
	}

	// setting copy_packet mode
	if (nfq_set_mode(qh, NFQNL_COPY_PACKET, 0xffff) < 0) {
		fprintf(stderr, "can't set packet_copy mode\n");
		exit(1);
	}
	
	init_table();  // initialize NAT table
	tokenBucket = bucketSize;  // number of tokens in a bucket is initialized as the bucket size
	time(&start_t);  // initialize start time (used when generating tokens)
	
	int i;
	for (i = 0; i < MAXPKTS; i++){
		threadUsage[i] = 0;  // all threads are not in use initially
	}
	
	//get_vma_addr();  // function call of the commented code
	
	// handling of incoming packets which can be done via a loop
	fd = nfq_fd(h);

	while ((rv = recv(fd, buf, sizeof(buf), 0)) >= 0) {
		printf("pkt received\n");
		nfq_handle_packet(h, buf, rv);
	}

	// destroy queue handle and close NFQUEUE
	nfq_destroy_queue(qh);
	nfq_close(h);
	return 0;
}