#include <stdlib.h>     // NULL, malloc, free
#include <stdio.h>      // printf
#include <string.h>     // strcmp, strerror
#include <errno.h>      // errno
#include <unistd.h>     // close
#include <sys/types.h>
#include <sys/socket.h> // send, recv

#include "myftp.h"      // MYFTP_CONNECT, fatal_error, open_socket,
                        // new_myftp_msg, send_myftp_msg, recv_myftp_msg,
                        // MYFTP_LIST_REQUEST, MYFTP_LIST_REPLY


void myftp_client_list( int sock_fd )
{
  send_myftp_msg( sock_fd, new_myftp_msg( MYFTP_LIST_REQUEST ) );

  struct myftp_msg list_resp = recv_myftp_msg( sock_fd );
  size_t payload_length = list_resp.length - (sizeof list_resp);
  char *buf = malloc( payload_length + 1 );
  if( buf == NULL ) fatal_error( 2, "malloc", strerror(errno) );

  int err = recv( sock_fd, buf + 1, payload_length, 0 );
  if( err == -1 ) fatal_error( 2, "recv", strerror(errno) );

  buf[0] = '\0';
  for(char *filename = buf;
      filename < buf + payload_length;
      filename++) {
    if( filename[-1] == '\0' ) printf( "%s\n", filename );
  }

  free( buf );
}

void myftp_client_get( int sock_fd, char *filename )
{

}

void myftp_client_put( int sock_fd, char *filename )
{

}

int main( int argc, char *argv[] )
{
  if( argc < 4 ) fatal_error( 1, "Too few arguments" );

  int sock_fd = open_socket( argv[1], argv[2], 0, MYFTP_CONNECT );

  if( strcmp( argv[3], "list" ) == 0 ) {
    myftp_client_list( sock_fd );

  } else if( strcmp( argv[3], "get" ) == 0 ) {
    if( argc < 5 ) fatal_error( 1, "Missing file argument" );
    myftp_client_get( sock_fd, argv[4] );

  } else if( strcmp( argv[3], "put" ) == 0 ) {
    if( argc < 5 ) fatal_error( 1, "Missing file argument" );
    myftp_client_put( sock_fd, argv[4] );

  } else {
    fatal_error( 2, "Invalid command", argv[3] );
  }

  close( sock_fd );
  return 0;
}

