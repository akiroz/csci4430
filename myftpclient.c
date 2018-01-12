#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "myftp.h"


void myftp_client_list( int sock_fd )
{
  struct myftp_msg req = new_myftp_msg( MYFTP_LIST_REQUEST );
  if( send_myftp_msg( sock_fd, &req ) == -1 ) {
    fatal_error( 2, "send", strerror(errno) );
  }

  struct myftp_msg resp;
  if( recv_myftp_msg( sock_fd, &resp ) == -1 ) {
    fatal_error( 2, "recv", strerror(errno) );
  }
  if( !myftp_msg_ok( resp ) ) {
    fatal_error( 1, "Malformed response" );
  }

  size_t payload_length = resp.length - (sizeof resp);
  char *buf = malloc( payload_length + 1 );
  if( buf == NULL ) {
    fatal_error( 2, "malloc", strerror(errno) );
  }

  if( recv_all( sock_fd, buf + 1, payload_length ) == -1 ) {
    fatal_error( 2, "recv", strerror(errno) );
  }

  buf[0] = '\0';
  for(char *filename = buf + 1;
      filename < buf + payload_length;
      filename++) {
    if( filename[-1] == '\0' ) printf( "%s\n", filename );
  }

  free( buf );
}

void myftp_client_get( int sock_fd, char *filename )
{
  int filename_len = strlen( filename ) + 1;
  struct myftp_msg req = new_myftp_msg( MYFTP_GET_REQUEST );
  req.length += filename_len;
  if( send_myftp_msg( sock_fd, &req ) == -1 ) {
    fatal_error( 2, "send", strerror(errno) );
  }
  if( send_all( sock_fd, filename, filename_len ) == -1 ) {
    fatal_error( 2, "send", strerror(errno) );
  }

  struct myftp_msg resp;
  if( recv_myftp_msg( sock_fd, &resp ) == -1 ) {
    fatal_error( 2, "recv", strerror(errno) );
  }
  if( !myftp_msg_ok( resp ) ) {
    fatal_error( 1, "Malformed response" );
  }
  if( resp.type == MYFTP_GET_REPLY_FAILURE ) {
    fatal_error( 1, "File does not exist" );
  }

  int file_size = resp.length - (sizeof resp);
  int file_fd = open( filename, O_WRONLY | O_TRUNC );

#if defined(__APPLE__)
    sendfile( sock_fd, file_fd, 0, (off_t*) &file_size, NULL, 0 );
#else
    sendfile( file_fd, sock_fd, 0, file_size );
#endif

  close( file_fd );
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

