#include <stdio.h>      // perror, fprintf
#include <stdlib.h>     // NULL, malloc, free
#include <string.h>     // strerror, strlen, strcpy
#include <errno.h>      // errno
#include <unistd.h>     // close
#include <sys/types.h>
#include <sys/socket.h> // struct sockaddr, socklen_t, listen, accept, send
#include <netdb.h>      // getnameinfo
#include <dirent.h>     // struct dirent, opendir, closedir, rewinddir
#include <pthread.h>    // pthread_t, pthread_attr_t, pthread_attr_init,
                        // pthread_attr_setdetachstate, pthread_create,
                        // pthread_attr_destroy

#include "myftp.h"      // fatal_error, open_socket, myftp_msg_ok, MYFTP_BIND
                        // MYFTP_LIST_REQUEST, MYFTP_GET_REQUEST, MYFTP_PUT_REQUEST

struct request {
  int sock_fd;
  struct sockaddr addr;
  socklen_t addrlen;
};

struct str_list {
  size_t length;
  char *data;
  struct str_list *next;
};

void myftp_server_list( int sock_fd )
{
  struct str_list *filelist = NULL;
  struct str_list *filename = NULL;
  DIR *data_dir = opendir("data");
  struct dirent *dir;
  while( (dir = readdir(data_dir)) ) {
    if( dir->d_type == DT_REG ) {
      if( filelist == NULL ) {
        filename = malloc( sizeof (struct str_list) );
        filelist = filename;
      } else {
        filename->next = malloc( sizeof (struct str_list) );
        filename = filename->next;
      }
      filename->length = strlen(dir->d_name) + 1;
      filename->data = malloc( filename->length );
      strcpy( filename->data, dir->d_name );
      filename->next = NULL;
    }
  }
  closedir(data_dir);

  struct myftp_msg msg = new_myftp_msg( MYFTP_LIST_REPLY );
  for( filename = filelist; filename; filename = filename->next ) {
    msg.length += filename->length;
  }
  send_myftp_msg( sock_fd, msg );

  struct str_list *next_filename;
  for( filename = filelist; filename; filename = next_filename ) {
    send( sock_fd, filename->data, filename->length, 0 );
    next_filename = filename->next;
    free( filename->data );
    free( filename );
  }

}

void myftp_server_get( int sock_fd )
{

}

void myftp_server_put( int sock_fd, unsigned int length )
{

}

void *handle_request( void *arg )
{
  struct request *req = (struct request*) arg;

  char hostname[256] = {0};
  int err = getnameinfo(
      &(req->addr), req->addrlen,
      hostname, 256, NULL, 0, 0);
  if( err != 0 ) {
    fprintf( stderr, "getnameinfo: %s\n", gai_strerror(err) );
  }

  struct myftp_msg msg = recv_myftp_msg( req->sock_fd );
  fprintf( stderr, "CLIENT: %s TYPE: 0x%X\n", hostname, msg.type );

  if( !myftp_msg_ok( msg ) ) {
    fprintf( stderr, "Client Error: Malformed message\n" );
  }
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

  DIR *data_dir = opendir("data");
  if( data_dir == NULL ) {
    fatal_error( 1, "Cannot open data directory" );
  }
  closedir(data_dir);

  int sock_fd = open_socket( "0.0.0.0", argv[1], AI_PASSIVE, MYFTP_BIND );
  int err = listen( sock_fd, 128 );
  if( err != 0 ) fatal_error( 2, "listen", strerror(errno) );

  fprintf( stderr, "Listening on port %s...\n", argv[1] );
  while(1) {
    struct request *req = malloc( sizeof (struct request) );
    req->addr.sa_family = AF_INET;

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

    //handle_request(req);
    pthread_t handler_thread;
    pthread_attr_t attr;
    pthread_attr_init( &attr );
    pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_DETACHED );
    err = pthread_create( &handler_thread, &attr, handle_request, req );
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
    pthread_attr_destroy( &attr );
  }

  return 0;
}
