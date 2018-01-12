#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include "myftp.h"


void fatal_error( int argc, ... )
{
  fprintf( stderr, "Error: " );

  va_list args;
  va_start( args, argc );
  for(int i = 0; i < argc; i++) {
    fprintf( stderr, "%s ", va_arg( args, char* ) );
  }
  va_end( args );

  fprintf( stderr, "\n" );
  exit(1);
}

int open_socket( char *hostname, char *port, int flags, int action )
{
  int err;

  struct protoent *tcp_proto = getprotobyname("tcp");
  if( tcp_proto == NULL ) fatal_error( 1, "getprotobyname(\"tcp\")" );
  endprotoent();

  struct addrinfo *host;
  struct addrinfo hints = {
    .ai_family    = AF_INET,
    .ai_socktype  = SOCK_STREAM,
    .ai_protocol  = tcp_proto->p_proto,
    .ai_flags     = flags
  };
  err = getaddrinfo( hostname, port, &hints, &host );
  if( err != 0 ) fatal_error( 2, "getaddrinfo", (char*) gai_strerror(err) );

  int sock_fd = socket(
      host->ai_family,
      host->ai_socktype,
      host->ai_protocol);
  if( sock_fd == -1 ) fatal_error( 2, "socket", strerror(errno) );

  switch(action) {
    case MYFTP_BIND:
      err = bind( sock_fd, host->ai_addr, host->ai_addrlen );
      if( err != 0 ) fatal_error( 2, "bind", strerror(errno) );
      break;

    case MYFTP_CONNECT:
      err = connect( sock_fd, host->ai_addr, host->ai_addrlen );
      if( err != 0 ) fatal_error( 2, "connect", strerror(errno) );
      break;
  }

  freeaddrinfo(host);

  return sock_fd;
}

bool myftp_msg_ok( struct myftp_msg msg )
{
  return memcmp( msg.protocol, "myftp", 5 ) == 0;
}

struct myftp_msg new_myftp_msg( unsigned char type )
{
  struct myftp_msg msg = {
    .protocol = "myftp",
    .type     = type,
    .length   = sizeof (struct myftp_msg)
  };
  return msg;
}

int recv_myftp_msg( int sock_fd, struct myftp_msg *msg )
{
  int err = recv_all( sock_fd, (char*) msg, sizeof (struct myftp_msg) );
  if( err == -1 ) return -1;
  msg->length = ntohl( msg->length );
  return 0;
}

int send_myftp_msg( int sock_fd, struct myftp_msg *msg )
{
  msg->length = htonl( msg->length );
  int err = send_all( sock_fd, (char*) msg, sizeof (struct myftp_msg) );
  if( err == -1 ) return -1;
  return 0;
}

int recv_all( int sock_fd, char *buf, size_t length )
{
  int size;
  size_t recv_len = 0;
  while(1) {
    size = recv( sock_fd, buf + recv_len, length - recv_len, 0 );
    if( size == -1 ) return -1;
    recv_len += size;
    if( recv_len >= length ) return 0;
  }
}

int send_all( int sock_fd, char *buf, size_t length )
{
  int size;
  size_t send_len = 0;
  while(1) {
    size = send( sock_fd, buf + send_len, length - send_len, 0 );
    if( size == -1 ) return -1;
    send_len += size;
    if( send_len >= length ) return 0;
  }
}

