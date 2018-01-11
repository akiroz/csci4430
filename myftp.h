
#define MYFTP_BIND 0
#define MYFTP_CONNECT 1

#define MYFTP_LIST_REQUEST      0xA1
#define MYFTP_LIST_REPLY        0xA2
#define MYFTP_GET_REQUEST       0xB1
#define MYFTP_GET_REPLY_SUCCESS 0xB2
#define MYFTP_GET_REPLY_FAILURE 0xB3
#define MYFTP_PUT_REQUEST       0xC1
#define MYFTP_PUT_REPLY         0xC2
#define MYFTP_FILE_DATA         0xFF

struct myftp_msg {
  unsigned char protocol[5];
  unsigned char type;
  unsigned int length;
} __attribute__ ((packed));

void fatal_error( int argc, ... );

int open_socket( char *hostname, char *port, int flags, int action );

struct myftp_msg new_myftp_msg( unsigned char type );
struct myftp_msg recv_myftp_msg( int sock_fd );
void send_myftp_msg( int sock_fd, struct myftp_msg msg );
