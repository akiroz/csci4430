#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#define MAX_PAIRS 2000

typedef struct NAT_table {
	unsigned int original_src_ip;
	int original_src_port;
	unsigned int translated_src_ip;
	int translated_src_port;
    int closing;  //track 4-way handshake status
} NAT_table;

NAT_table nat_table[MAX_PAIRS];

// initialize NAT table, called at the beginning
void init_table() {
	int i;
	NAT_table blank_entry = {-1,-1,-1,-1,0};  //initial values: -1, -1, -1, -1, 0
	
    for(i = 0; i < MAX_PAIRS; i++) {
		nat_table[i] = blank_entry;
    }
}

// print the NAT table whenever there is update
void print_table() {
    int i;
	
	// table header
    printf("src.addr         src.port  translated.addr  translated.port\n");
	
    for(i = 0; i < MAX_PAIRS; i++) {
        if(nat_table[i].original_src_ip != -1) {  // if the row has a mapping
			
			// convert unsigned long IP addresses to strings
            struct in_addr ori_addr, tran_addr;
            ori_addr.s_addr = nat_table[i].original_src_ip;
            tran_addr.s_addr = nat_table[i].translated_src_ip;
            char ori_addr_str[16], tran_addr_str[16];
            strcpy(ori_addr_str, (char*)inet_ntoa(ori_addr));
            strcpy(tran_addr_str, (char*)inet_ntoa(tran_addr));
			
			// print table
            printf("%s %d %s %d\n",
                    ori_addr_str,
                    nat_table[i].original_src_port,
                    tran_addr_str,
                    nat_table[i].translated_src_port);
        }
    }
}

// create a new entry in the NAT table and return it
NAT_table insert_entry(unsigned int ori_ip, int ori_port, unsigned int dest_ip) {
	int i, open_port;
    char ports[MAX_PAIRS] = {-1};
	
	// find ports that are used by current connections
    for(i = 0; i < MAX_PAIRS; i++) {
        if(nat_table[i].translated_src_port != -1) {
            ports[i] = 1;
        }
    }
	
	// find the smallest available port
    for(i = 0; i < MAX_PAIRS; i++) {
        if(ports[i] == -1)
			break;
    }
    open_port = 10000 + i;
	
	// create a new entry in NAT table on position "i"
	nat_table[i].original_src_ip = ori_ip;
	nat_table[i].original_src_port = ori_port;
    nat_table[i].translated_src_ip = dest_ip;
	nat_table[i].translated_src_port = open_port;
	
    print_table();
    return nat_table[i];
}

// search NAT table with original source IP and port, return -1 if not found
int search_table_source(unsigned int src_ip, int src_port) {
	int i;

    for(i = 0; i < MAX_PAIRS; i++) {
        if(nat_table[i].original_src_ip == src_ip && nat_table[i].original_src_port == src_port) {
            return i;
        }
    }
    return -1;
}

// search NAT table with translated source port, return -1 if not found
int search_table_dest_port(int dest_port) {
	int i;
	
    for(i = 0; i < MAX_PAIRS; i++) {
        if(nat_table[i].translated_src_port == dest_port) {
            return i;
        }
    }
    return -1;
}

// return a particular entry with known position in NAT table
NAT_table get_table_entry(int index){
	return nat_table[index];
}

// delete a particular entry
void delete_entry(unsigned int id, int port) {
    int i;
    NAT_table blank_entry = {-1,-1,-1,-1,0};
	
	// search the entry with original source IP and port
    for(i = 0; i < MAX_PAIRS; i++) {
        if(nat_table[i].original_src_ip == id && nat_table[i].original_src_port == port){
            nat_table[i] = blank_entry;
            print_table();
            return;
        }
    }
}