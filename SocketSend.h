#ifndef __SOCKET_SEND_H__
#define	__SOCKET_SEND_H__

#include "SocketProtocol.h"
#include <vector>

class SocketAddr
{
public:
	SocketAddr();
	~SocketAddr();
	bool init();
	bool updateserveraddr();
	void stopserver();
	void release();
private:
	SOCKET ServerAddrRecv;
	sockaddr_in ServerAddrRecvId;
	HANDLE mHandle;
	HANDLE mEvent;
	std::vector<sockaddr_in*> ServerAddr;
	CRITICAL_SECTION ServerAddrLock;

	static DWORD WINAPI ServerAddrRecvThread(void* param);
};

class SocketSend
{
public:
	SocketSend(const char* dest); //Ä¿±êµØÖ·;

	~SocketSend();
	bool init();
	bool inittcp();
	bool sendfile(const TCHAR* path, const TCHAR* filename);
	//void stopsend();
	bool startserver();
	void stopserver();
	//void release();
private:
	SOCKET tcptransfer;
	SOCKET udpcoderecv;
	char* tcpdest;
	sockaddr_in updbind;
	sockaddr_in tcpaddr;
	HANDLE hudpcoderecv;
	HANDLE hSocketStart;
	HANDLE hSocketStop;
	HANDLE hUploadBegin;

	static DWORD WINAPI udpcoderecvhread(void* param);
	//static DWORD WINAPI tcptransferthread(void* param);
};

static bool SendBroadCastMessage();

#endif