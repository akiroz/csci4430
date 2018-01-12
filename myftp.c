#include <stdio.h>      // fprintf, sprintf
#include <stdlib.h>     // NULL, exit
#include <errno.h>      // errno
#include <string.h>     // strerror, memcmp
#include <stdarg.h>     // va_start, va_arg, va_end
#include <sys/types.h>  // Legacy header for socket.h types
#include <sys/socket.h> // AF_UNSPEC, SOCK_STREAM, socket, bind, connect
#include <netdb.h>      // struct addrinfo, struct protoent, getprotobyname,
                        // endprotoent, getaddrinfo 
#include <netinet/in.h> // htonl, ntohl

#include "myftp.h"      // MYFTP_BIND, MYFTP_CONNECT fatal_error, get_socket


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

bool myftp_msg_ok(struct myftp_msg msg)
{
  return memcmp( msg.protocol, "myftp", 5 ) == 0;
}

struct myftp_msg new_myftp_msg(unsigned char type)
{
  struct myftp_msg msg = {
    .protocol = "myftp",
    .type     = type,
    .length   = sizeof (struct myftp_msg)
  };
  return msg;
}

struct myftp_msg recv_myftp_msg( int sock_fd )
{
  struct myftp_msg resp;
  int err = recv( sock_fd, &resp, sizeof resp, 0 );
  if( err == -1 ) fatal_error( 2, "recv", strerror(errno) );
  resp.length = ntohl( resp.length );
  return resp;
}

void send_myftp_msg( int sock_fd, struct myftp_msg msg )
{
  msg.length = htonl( msg.length );
  int err = send( sock_fd, &msg, sizeof msg, 0 );
  if( err == -1 ) fatal_error( 2, "send", strerror(errno) );
}

