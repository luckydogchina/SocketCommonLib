#ifndef __SOCKET_BROADCAST_H__
#define	__SOCKET_BROADCAST_H__

#include"SocketBase.h"

class SocketBroadCast:public SocketBase
{
public:
	SocketBroadCast();
	~SocketBroadCast();

	bool startserver();
	void stopserver();
	void getserverid(sockaddr_in* param);
private:

	HANDLE server_thread;
	static DWORD WINAPI server_thread_proc(void* param);

};

static void RecvCallBackProc(SOCKET sock, void* param);

#endif