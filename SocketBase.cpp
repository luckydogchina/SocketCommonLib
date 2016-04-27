#include "SocketBase.h"

SocketBase::SocketBase()
{
	this->mRef = 1;
	InitializeCriticalSection(&this->m_cs);
	memset(&this->server_id, 0, sizeof(sockaddr_in));
	memset(&this->recv_callback, 0, sizeof(recv_callback));
}

SocketBase::~SocketBase()
{
	DeleteCriticalSection(&this->m_cs);
}

void SocketBase::registryhandle(RECV_CALLBACK fun_addr, unsigned int type)
{
	if (fun_addr)
	{
		EnterCriticalSection(&this->m_cs);
		this->recv_callback[type] = fun_addr;
		LeaveCriticalSection(&this->m_cs);
	}
	return;
}

ULONG SocketBase::AddRef()
{
	return ::InterlockedIncrement(&mRef);
}

ULONG SocketBase::Release()
{
	if (::InterlockedDecrement(&mRef) == 0)
	{
		delete (this);
		return 0;
	}
	return mRef;
}

bool SocketBase::init(bool isTcp, short port)
{
	int ret;
	unsigned long ul = 1;
	if (isTcp)
	{
		this->socket_id = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	}
	else
	{
		this->socket_id = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	}
	
	this->server_id.sin_port = htons(port);
	this->server_id.sin_family = AF_INET;
	this->server_id.sin_addr.s_addr = htonl(INADDR_ANY);

	if (this->socket_id < 0)
	{
		printf("create a socket error:%d", GetLastError());
		return false;
	}

	ret = ioctlsocket(this->socket_id, FIONBIO, (unsigned long *)&ul);
	if (ret < 0)
	{
		_closesocket(this->socket_id);
		return false;
	}

	if (bind(this->socket_id, (sockaddr*)&(this->server_id), sizeof(sockaddr_in)) < 0)
	{
		printf("bind error: %d\r\n", WSAGetLastError());
		_closesocket(this->socket_id);
		return false;
	}
	return true;
}