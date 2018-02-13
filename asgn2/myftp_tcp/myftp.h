#ifndef __myftp_h__
#define __myftp_h__

#define BUF_SIZE 16384

int sendn(int sd, void* buf, int buf_len);
int recvn(int sd, void* buf, int buf_len);
#endif
