#ifndef SOCKLIBH
#define SOCKLIBH




#ifdef _WIN32

//todo

#else

#include <netinet/in.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>



#define PIN_SOCKET      int
#define INVALID_SOCKET  (-1)
#define pin_socket      socket
#define pin_accept      accept
#define pin_select      select
#define pin_connect     connect
#define pin_timeval     struct timeval
#define pin_fd_set      fd_set
#define pin_setsockopt  setsockopt
#define pin_sockaddr_in sockaddr_in
#define pin_htons       htons
#define pin_bind        bind
#define pin_listen      listen
#define pin_sockaddr    sockaddr
#define pin_socklen_t   socklen_t
#define pin_closesocket close

#endif


int pin_send(PIN_SOCKET cli_socket, const char* buf, size_t n)
{
  int ret;
#ifdef _WIN32
  ret = p_send(cli_socket, (const char *)buf, (int)n, 0);
#else
  do
    ret = send(cli_socket, buf, n, 0);
  while ( ret == -1 && errno == EINTR );
#endif
  return ret;
}

int pin_recv(PIN_SOCKET fd, char* buf, size_t n)
{
  char *bufp = (char*)buf;
  ssize_t total = 0;
  while ( n > 0 )
  {
    int ret;
#ifdef _WIN32
    ret = p_recv(fd, bufp, (int)n, 0);
#else
    do
      ret = read(fd, bufp, n);
    while ( ret == -1 && errno == EINTR );
#endif
    if ( ret <= 0 )
      return ret;
    n -= ret;
    bufp += ret;
    total += ret;
  }
  return total;
}


#endif
