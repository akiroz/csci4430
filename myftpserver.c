#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include "myftp.h"

#if defined(__linux__) || defined(__sun)
  #include <sys/sendfile.h>
#endif

struct request {
  int sock_fd;
  struct sockaddr addr;
  socklen_t addrlen;
};


// absolute data repo path initialized at startup
char *abs_data_path;


void myftp_server_list( int sock_fd )
{
  struct str_list {
    size_t length;
    char *data;
    struct str_list *next;
  };
  struct str_list *filelist = NULL;
  struct str_list *filename = NULL;
  DIR *data_dir = opendir("data");
  struct dirent *dir;
  while( (dir = readdir(data_dir)) ) {
    if( dir->d_type == DT_REG && dir->d_name[0] != '.' ) {
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
  if( send_myftp_msg( sock_fd, &msg ) == -1 ){
    fprintf( stderr, "send: %s\n", strerror(errno) );
    return;
  }

  struct str_list *next_filename;
  for( filename = filelist; filename; filename = next_filename ) {
    if( send_all( sock_fd, filename->data, filename->length ) == -1 ) {
      fprintf( stderr, "send: %s\n", strerror(errno) );
      return;
    }
    next_filename = filename->next;
    free( filename->data );
    free( filename );
  }
}

bool startsWith(const char *pre, const char *str)
{
  size_t lenpre = strlen(pre),
         lenstr = strlen(str);
  return lenstr < lenpre ? false : strncmp(pre, str, lenpre) == 0;
}

int open_file( char *filename, int *file_size )
{
  char *file_path = (char*) malloc( strlen("data/") + strlen(filename) + 2 );
  strcpy( file_path, "data/" );
  strcat( file_path, filename );

  char *abs_path = realpath( file_path, NULL );
  if( abs_path == NULL ) {
    switch(errno) {
      case ELOOP:
      case ENAMETOOLONG:
      case ENOENT:
      case ENOTDIR:
        fprintf( stderr, "Client Error: %s\n", strerror(errno) );
        return -1;
      default:
        fprintf( stderr, "realpath: %s\n", strerror(errno) );
        return -2;
    }
  }

  bool path_valid = startsWith( abs_data_path, abs_path );
  free( abs_path );
  if( !path_valid ) {
    fprintf( stderr, "Client Error: Invalid file path: %s\n", file_path );
    return -1;
  }

  struct stat file_stat;
  if( stat( file_path, &file_stat ) == -1 ) {
    if( errno == ENOENT ) {
      fprintf( stderr, "Client Error: File does not exist\n" );
      return -1;
    } else {
      fprintf( stderr, "stat: %s\n", strerror(errno) );
      return -2;
    }
  }
  if( file_stat.st_mode != S_IFREG ) {
    fprintf( stderr, "Client Error: Not a regular file\n" );
    return -1;
  }

  *file_size = file_stat.st_size;

  int file_fd = open( file_path, O_RDONLY );
  if( file_fd == -1 ) {
    fprintf( stderr, "open: %s\n", strerror(errno) );
    return -2;
  }  

  return file_fd;
}

void myftp_server_get( int sock_fd, unsigned int msg_len )
{
  if( msg_len < 1 ) {
    fprintf( stderr, "Client Error: Invalid payload length\n" );
    return;
  }

  char *file_path = (char*) malloc( msg_len );
  if( recv_all( sock_fd, file_path, msg_len ) ) {
    fprintf( stderr, "recv: %s\n", strerror(errno) );
    return;
  }
  if( file_path[msg_len - 1] != '\0' ) {
    fprintf( stderr, "Client Error: Invalid file path\n" );
    return;
  }
  
  fprintf( stderr, "GET: %s\n", file_path );

  int file_size;
  int file_fd = open_file( file_path, &file_size );
  free( file_path );
  if( file_fd == -2 ) return;
  if( file_fd == -1 ) {
    struct myftp_msg resp = new_myftp_msg( MYFTP_GET_REPLY_FAILURE );
    send_myftp_msg( sock_fd, &resp );
  } else {
    struct myftp_msg resp = new_myftp_msg( MYFTP_GET_REPLY_SUCCESS );
    resp.length += file_size;
    send_myftp_msg( sock_fd, &resp );

#if defined(__APPLE__)
    sendfile( file_fd, sock_fd, 0, (off_t*) &file_size, NULL, 0 );
#else
    sendfile( sock_fd, file_fd, 0, file_size );
#endif

    close( file_fd );
  }
  return;
}

void myftp_server_put( int sock_fd, unsigned int msg_len )
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
    goto fail;
  }

  struct myftp_msg msg;
  if( recv_myftp_msg( req->sock_fd, &msg ) == -1 ){
    fprintf( stderr, "recv: %s\n", strerror(errno) );
    goto fail;
  }

  fprintf(
      stderr, "Request: 0x%X %s (%lu bytes)\n",
      msg.type, hostname, msg.length - (sizeof msg));

  if( !myftp_msg_ok( msg ) ) {
    fprintf( stderr, "Client Error: Malformed message\n" );
    goto fail;
  }

  switch( msg.type ) {
    case MYFTP_LIST_REQUEST:
      myftp_server_list( req->sock_fd );
      break;
    case MYFTP_GET_REQUEST:
      myftp_server_get( req->sock_fd, msg.length - (sizeof msg) );
      break;
    case MYFTP_PUT_REQUEST:
      myftp_server_put( req->sock_fd, msg.length - (sizeof msg) );
      break;
    default:
      fprintf( stderr, "Client Error: Invalid message type\n" );
      goto fail;
  }

fail:
  close( req->sock_fd );
  free( req );
  return NULL;
}

int main( int argc, char *argv[] )
{
  if( argc < 2 ) fatal_error( 1, "Too few arguments" );

  abs_data_path = realpath( "data", NULL );
  DIR *data_dir = opendir( "data" );
  if( data_dir == NULL ) {
    fatal_error( 1, "Cannot open data directory" );
  }
  closedir(data_dir);
  fprintf( stderr, "Using data dir: %s\n", abs_data_path );

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

    handle_request(req);
    //pthread_t handler_thread;
    //pthread_attr_t attr;
    //pthread_attr_init( &attr );
    //pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_DETACHED );
    //err = pthread_create( &handler_thread, &attr, handle_request, req );
    //if( err != 0 ) {
    //  switch( errno ) {
    //    case EAGAIN:
    //      perror( "Error: pthread_create" );
    //      free( req );
    //      continue;
    //    default:
    //      fatal_error( 2, "pthread_create", strerror(errno) );
    //  }
    //}
    //pthread_attr_destroy( &attr );
  }

  return 0;
}
