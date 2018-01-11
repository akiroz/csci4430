#include <stdio.h>      // perror, fprintf
#include <stdlib.h>     // NULL, malloc, free
#include <string.h>     // strerror
#include <errno.h>      // errno
#include <unistd.h>     // close
#include <sys/types.h>
#include <sys/socket.h> // struct sockaddr, socklen_t, listen, accept
#include <netdb.h>      // getnameinfo
#include <pthread.h>    // pthread_t, pthread_attr_t, pthread_attr_init,
                        // pthread_attr_setdetachstate, pthread_create,
                        // pthread_attr_destroy

#include "myftp.h"      // fatal_error, open_socket, MYFTP_BIND
                        // MYFTP_LIST_REQUEST, MYFTP_GET_REQUEST, MYFTP_PUT_REQUEST

struct request {
  int sock_fd;
  struct sockaddr addr;
  socklen_t addrlen;
};

void myftp_server_list( int sock_fd )
{
  
}

void myftp_server_get( int sock_fd )
{

}

void myftp_server_put( int sock_fd, unsigned int length )
{

}

void *handle_request(void *arg)
{
  struct request *req = (struct request*) arg;
  struct myftp_msg msg = recv_myftp_msg( req->sock_fd );

  char hostname[256];
  getnameinfo( &(req->addr), req->addrlen, hostname, 256, NULL, 0, 0 );
  fprintf( stderr, "REQ: %s TYPE: 0x%X", hostname, msg.type );

  switch( msg.type ) {
    case MYFTP_LIST_REQUEST:
      myftp_server_list( req->sock_fd );
      break;
    case MYFTP_GET_REQUEST:
      myftp_server_get( req->sock_fd );
      break;
    case MYFTP_PUT_REQUEST:
      myftp_server_put( req->sock_fd, msg.length );
      break;
    default:
      fprintf( stderr, "Client Error: Invalid message type\n" );
  }

  close( req->sock_fd );
  free( req );
  return NULL;
}

int main( int argc, char *argv[] )
{
  if( argc < 2 ) fatal_error( 1, "Too few arguments" );

  int sock_fd = open_socket( "0.0.0.0", argv[1], 0, MYFTP_BIND );
  int err = listen( sock_fd, 128 );
  if( err != 0 ) fatal_error( 2, "listen", strerror(errno) );

  while(1) {
    struct request *req = malloc( sizeof (struct request) );

    req->sock_fd = accept( sock_fd, &(req->addr), &(req->addrlen) );
    if( req->sock_fd == -1 ) {
      switch( errno ) {
        case ECONNABORTED:
        case EINTR:
        case EMFILE:
        case ENFILE:
        case ENOBUFS:
        case ENOMEM:
        case EPROTO:
        case EPERM:
        case ENOSR:
        case ETIMEDOUT:
          perror( "Error: accept" );
          free( req );
          continue;
        default:
          fatal_error( 2, "accept", strerror(errno) );
      }
    }

    pthread_t *handler_thread;
    pthread_attr_t *attr;
    pthread_attr_init( attr );
    pthread_attr_setdetachstate( attr, PTHREAD_CREATE_DETACHED );
    err = pthread_create( handler_thread, attr, handle_request, req );
    if( err != 0 ) {
      switch( errno ) {
        case EAGAIN:
          perror( "Error: pthread_create" );
          free( req );
          continue;
        default:
          fatal_error( 2, "pthread_create", strerror(errno) );
      }
    }
    pthread_attr_destroy( attr );
  }

  return 0;
}
