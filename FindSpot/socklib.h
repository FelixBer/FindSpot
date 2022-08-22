#ifndef SOCKLIBH
#define SOCKLIBH




#ifdef _WIN32

namespace WINDOWS
{
	#undef timerisset
	#undef timercmp
	#undef timerclear
	#define WIN32_LEAN_AND_MEAN
	#include "Windows.h"
	#include <WinUser.h>
	#include <Ws2tcpip.h>
};

typedef WINDOWS::SOCKET PIN_SOCKET;

//#define INVALID_SOCKET  (-1)
//#define TEXT 


#define pin_socket      p_socket
#define pin_htons       p_htons
#define pin_bind        p_bind
#define pin_listen      p_listen
#define pin_connect     p_connect
#define pin_accept(s, addr, addrlen) p_accept(s, (WINDOWS::sockaddr *)addr, addrlen)
#define pin_select(s, rds, wds, eds, tv) p_select(s, rds, wds, eds, tv)
#define pin_setsockopt(s, level, optname, optval, optlen) p_setsockopt(s, level, optname, (const char*)optval, optlen)
#define pin_closesocket(s)  p_closesocket(s)
typedef WINDOWS::timeval pin_timeval;
typedef WINDOWS::fd_set pin_fd_set;

// the following typedefs are necessary for FD_... macros
#define  fd_set WINDOWS::fd_set
typedef WINDOWS::u_int u_int;
typedef WINDOWS::u_short u_short;
typedef WINDOWS::SOCKET SOCKET;

#define pin_sockaddr    WINDOWS::sockaddr
#define pin_socklen_t   WINDOWS::socklen_t
#define pin_sockaddr_in WINDOWS::sockaddr_in
#define ULONG WINDOWS::ULONG

static int (WSAAPI* p_WSAStartup)(WINDOWS::WORD, WINDOWS::WSADATA*);
static int (WSAAPI* p_WSAGetLastError)(void);
static WINDOWS::SOCKET(WSAAPI* p_socket)(int af, int type, int protocol);
static int (WSAAPI* p_bind)(WINDOWS::SOCKET, const struct WINDOWS::sockaddr*, int);
static int (WSAAPI* p_setsockopt)(WINDOWS::SOCKET, int, int, const char* optval, int optlen);
static int (WSAAPI* p_listen)(WINDOWS::SOCKET s, int backlog);
static WINDOWS::SOCKET(WSAAPI* p_accept)(WINDOWS::SOCKET, struct WINDOWS::sockaddr*, int*);
static int (WSAAPI* p_recv)(WINDOWS::SOCKET s, char* buf, int len, int flags);
static int (WSAAPI* p_send)(WINDOWS::SOCKET s, const char* buf, int len, int flags);
static int (WSAAPI* p_select)(int, fd_set*, fd_set*, fd_set*, const struct WINDOWS::timeval*);
static int (WSAAPI* p_closesocket)(WINDOWS::SOCKET s);
static u_short(WSAAPI* p_htons)(u_short hostshort);
#define __WSAFDIsSet p__WSAFDIsSet
static int (WSAAPI* p__WSAFDIsSet)(WINDOWS::SOCKET fd, fd_set*);
static int (WSAAPI* p_connect)(WINDOWS::SOCKET, const struct WINDOWS::sockaddr*, int);
static unsigned long (WSAAPI* inet_addr)(const char*);

bool init()
{
	if (p_WSAGetLastError)
		return true; //already initialized

	//pincrt support sockets only on unix
	WINDOWS::HMODULE h = WINDOWS::LoadLibraryA(("ws2_32.dll"));
	if (h == NULL)
	{
		return false;
	}
	*(WINDOWS::FARPROC*)&p_WSAStartup = GetProcAddress(h,("WSAStartup"));
	*(WINDOWS::FARPROC*)&p_WSAGetLastError = GetProcAddress(h,("WSAGetLastError"));
	*(WINDOWS::FARPROC*)&p_socket = GetProcAddress(h,("socket"));
	*(WINDOWS::FARPROC*)&p_bind = GetProcAddress(h,("bind"));
	*(WINDOWS::FARPROC*)&p_setsockopt = GetProcAddress(h,("setsockopt"));
	*(WINDOWS::FARPROC*)&p_listen = GetProcAddress(h,("listen"));
	*(WINDOWS::FARPROC*)&p_accept = GetProcAddress(h,("accept"));
	*(WINDOWS::FARPROC*)&p_recv = GetProcAddress(h,("recv"));
	*(WINDOWS::FARPROC*)&p_send = GetProcAddress(h,("send"));
	*(WINDOWS::FARPROC*)&p_select = GetProcAddress(h,("select"));
	*(WINDOWS::FARPROC*)&p_closesocket = GetProcAddress(h,("closesocket"));
	*(WINDOWS::FARPROC*)&p_htons = GetProcAddress(h,("htons"));
	*(WINDOWS::FARPROC*)&p__WSAFDIsSet = GetProcAddress(h,("__WSAFDIsSet"));
	*(WINDOWS::FARPROC*)&p_connect = GetProcAddress(h,("connect"));
	*(WINDOWS::FARPROC*)&inet_addr = GetProcAddress(h,("inet_addr"));


	if (p_WSAStartup == NULL
		|| p_WSAGetLastError == NULL
		|| p_socket == NULL
		|| p_bind == NULL
		|| p_setsockopt == NULL
		|| p_listen == NULL
		|| p_accept == NULL
		|| p_recv == NULL
		|| p_send == NULL
		|| p_select == NULL
		|| p_closesocket == NULL
		|| p_htons == NULL
		|| p__WSAFDIsSet == NULL
		|| p_connect == NULL
		|| inet_addr == NULL)
	{
		return false;
	}

	WINDOWS::WORD wVersionRequested = 0x0202;
	WINDOWS::WSADATA wsaData;
	int err = p_WSAStartup(wVersionRequested, &wsaData);
	if (err != 0)
	{
		return false;
	}
	return true;
}

#else

bool init() {
	return true;
}

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
  int total = 0;
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

