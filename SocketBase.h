#ifndef __SOCKET_BASE_H__
#define	__SOCKET_BASE_H__

#include "SocketProtocol.h"

#define CALLBACK_BROADCAST		0
#define CALLBACK_RECV			1

class SocketBase
{
public:
	SocketBase();
	~SocketBase();
	virtual bool startserver() = 0;
	virtual void stopserver() = 0;

	ULONG AddRef();
    ULONG Release();
    bool init(bool isTcp, short port);
	void registryhandle(RECV_CALLBACK fun_addr, unsigned int type);
protected:
	RECV_CALLBACK recv_callback[16];
	SOCKET socket_id;
	sockaddr_in server_id;
	CRITICAL_SECTION m_cs;
private:
	volatile ULONG mRef;
};

#endif