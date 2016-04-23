#ifndef __SOCKET_RECEIVE_H__
#define __SOCKET_RECEIVE_H__

#include "SocketBase.h"
#include <map>
#include <list>
#define LISTEN_MAX		16
class CExecute;

class SocketRecv:public SocketBase
{
public:
	SocketRecv();
	~SocketRecv();
	bool startserver();
	void stopserver();
	void insertlist(SOCKET sock, sockaddr_in* _sockaddr);
	sockaddr_in* getlist(SOCKET sock);
	void removelist(SOCKET sock);
	void insetexecuter(CExecute* pCExecute);
private:
	std::map<SOCKET, sockaddr_in*> mlist;
	HANDLE server_thread;
	HANDLE check_status;
	HANDLE check_stop;
	CRITICAL_SECTION mexecutelistlock;
	CRITICAL_SECTION listlock;
	std::list<CExecute*> mexecutelist;
	static DWORD WINAPI checks_tatus_proc(void* param);
	static DWORD WINAPI server_thread_proc(void* param);
};

class CExecute
{
public:
	CExecute(SOCKET sock);
	~CExecute();
	bool startserver();
	void stopserver();
	bool isrun();
private:
	volatile ULONG mrun;
	SOCKET m_udp_socket;
	SOCKET msocket;
	CRITICAL_SECTION mlistlock;
	HANDLE mevent;
	HANDLE mstop;
	HANDLE mrecv;
	HANDLE mdatamanager;
	//接收数据;
	static DWORD WINAPI 
		RecvCallBackThread(void *param);
	//处理数据;
	static DWORD WINAPI
		DataManageThread(void *param);
	//存储数据链表;
	std::list<file_packet*> mdatalist;


};


static void RecvCallBackProc(SOCKET sock, void* param);


extern void SocketRecvInit();
extern SocketRecv* SocketRecvGet();
extern void SocketRecvDone();

#endif