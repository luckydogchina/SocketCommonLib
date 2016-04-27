#include "SocketRecv.h"
#include <winnt.h>

SocketRecv::SocketRecv()
{
	InitializeCriticalSection(&this->listlock);
	InitializeCriticalSection(&this->mexecutelistlock);
	this->check_stop = CreateEvent(NULL, 0, 0, _T("check_stop"));
}

SocketRecv::~SocketRecv()
{
	CloseHandle(this->check_stop);
	CExecute* CCExcute;
	//此时无需再检查接对象是否活着,因为执行到这一步已经说明全都死了;
	EnterCriticalSection(&this->mexecutelistlock);
	for (;;)
	{
		if (this->mexecutelist.empty())
			break;
		CCExcute = this->mexecutelist.front();
		this->mexecutelist.pop_front();
		CCExcute->stopserver();
		delete(CCExcute);
	}
	LeaveCriticalSection(&this->mexecutelistlock);
	DeleteCriticalSection(&this->listlock);
	DeleteCriticalSection(&this->mexecutelistlock);
}

void SocketRecv::insertlist(SOCKET sock, sockaddr_in* _sockaddr)
{
	sockaddr_in* tmp = (sockaddr_in*)malloc(sizeof(sockaddr_in));
	memset(tmp, 0, sizeof(sockaddr_in));
	memcpy(tmp, _sockaddr, sizeof(sockaddr_in));
	EnterCriticalSection(&this->listlock);
	this->mlist.insert(std::map<SOCKET, sockaddr_in*>::value_type(sock, tmp));
	LeaveCriticalSection(&this->listlock);
}

sockaddr_in* SocketRecv::getlist(SOCKET sock)
{
	sockaddr_in* sockaddr;
	EnterCriticalSection(&this->listlock);
	sockaddr = this->mlist[sock];
	LeaveCriticalSection(&this->listlock);
	return sockaddr;
}

void SocketRecv::removelist(SOCKET sock)
{
	std::map<SOCKET, sockaddr_in*>::iterator iter;
	sockaddr_in* _sockaddr;
	EnterCriticalSection(&this->listlock);
	iter = this->mlist.find(sock);
	if (iter == this->mlist.end())
	{
		LeaveCriticalSection(&this->listlock);
		goto mexit;
	}
	_sockaddr = iter->second;
	this->mlist.erase(iter);
	
	free(_sockaddr);
	printf("remove the socket %d", sock);
	//closesocket(sock);
mexit:
	LeaveCriticalSection(&this->listlock);
	return;
}

void SocketRecv::insetexecuter(CExecute* pCExecute)
{
	EnterCriticalSection(&this->mexecutelistlock);
	this->mexecutelist.push_back(pCExecute);
	LeaveCriticalSection(&this->mexecutelistlock);
}
bool SocketRecv::startserver()
{
	if (!this->init(true, FILE_RECV_PORT))
	{
		return false;
	}

	this->registryhandle(RecvCallBackProc, CALLBACK_RECV);
	this->check_status = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)SocketRecv::checks_tatus_proc, this, 0, NULL);
	
	if (this->check_status == INVALID_HANDLE_VALUE)
	{
		closesocket(this->socket_id);
		return false;
	}
	
	this->server_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)SocketRecv::server_thread_proc, this, 0, NULL);
	if (this->server_thread == INVALID_HANDLE_VALUE)
	{
		closesocket(this->socket_id);
		return false;
	}

	return true;
}

void SocketRecv::stopserver()
{
	printf("正在终止接收服务.. ..\r\n");
	SetEvent(this->check_stop);


	if (INVALID_HANDLE_VALUE != this->check_status)
	{
		WaitForSingleObject(this->check_status, INFINITE);
		CloseHandle(this->check_status);
	}

	closesocket(this->socket_id);
	
	if (INVALID_HANDLE_VALUE != this->server_thread)
	{
		WaitForSingleObject(this->server_thread, INFINITE);
		CloseHandle(this->server_thread);
	}

	printf("接收服务停止\r\n");

	return;
}

DWORD WINAPI SocketRecv::checks_tatus_proc(void* param)
{
	SocketRecv* server = (SocketRecv*)param;
	std::list<CExecute*>::iterator iter;
	CExecute *CCExecute;
	//五秒轮询一次
	while (WaitForSingleObject(server->check_stop, 5000)!=WAIT_OBJECT_0)
	{
		//printf("检查接收者状态... ...\r\n");
		EnterCriticalSection(&server->mexecutelistlock);
		for (iter = server->mexecutelist.begin();
			iter != server->mexecutelist.end();)
		{
			CCExecute = *iter;
			if (!CCExecute->isrun())
			{
				server->mexecutelist.erase(iter++);
				printf("检查到一个死了的线程\r\n");
				CCExecute->stopserver();
				delete(CCExecute);
			}
			else
			{
				++iter;
			}
		}
		LeaveCriticalSection(&server->mexecutelistlock);
	}
	return 0;
}

DWORD WINAPI SocketRecv::server_thread_proc(void* param)
{
	SocketRecv* server = (SocketRecv*)param;
	fd_set m_set;
	int reslt;
	
	SOCKET sock;
	sockaddr_in _sockaddr;
	int socklen = sizeof(_sockaddr);

	listen(server->socket_id, LISTEN_MAX);

	while (true)
	{
		FD_ZERO(&m_set);
		FD_SET(server->socket_id, &m_set);

		reslt = select(server->socket_id + 1, &m_set, NULL, NULL, NULL);

		if (reslt > 0 && FD_ISSET(server->socket_id, &m_set))
		{
			sock = accept(server->socket_id, (sockaddr*)&_sockaddr, &socklen);
			
			if (sock == INVALID_SOCKET)
			{
				break;
			}

			EnterCriticalSection(&server->m_cs);
			if (server->recv_callback[CALLBACK_RECV])
			{
				printf("开启一个接收链接:%d %s\r\n", sock, inet_ntoa(_sockaddr.sin_addr));
				server->insertlist(sock, &_sockaddr);
				server->recv_callback[CALLBACK_RECV](sock, server);
			}
			LeaveCriticalSection(&server->m_cs);
		}

		if (reslt < 0)
		{
			printf("exit the thread\r\n");
			break;
		}
	}

	return 0;
}

void RecvCallBackProc(SOCKET sock, void* param)
{
	int nRecvBuf = FILE_PACKET_SIZE*FILE_PACKET_SIZE;
	SocketRecv* CSocketRecv = (SocketRecv*)param;

	//设置接收缓存区为1M;
	setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char*)&nRecvBuf, sizeof(int));
	CExecute* CCExecute = new CExecute(sock);
	if (!CCExecute->startserver())
	{
		CCExecute->stopserver();
		delete(CCExecute);
	}
	else
	{
		CSocketRecv->insetexecuter(CCExecute);
	}
	return;
}

CExecute::CExecute(SOCKET sock)
{
	this->mrun = 2;
	this->msocket = sock;
	this->m_udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	this->mevent = CreateEvent(NULL, 0, 0, _T("execute_data"));
	this->mstop = CreateEvent(NULL, 1, 0, _T("execute_stop"));
	InitializeCriticalSection(&this->mlistlock);
}

CExecute::~CExecute()
{
	closesocket(this->m_udp_socket);
	CloseHandle(this->mevent);
	CloseHandle(this->mstop);
	DeleteCriticalSection(&this->mlistlock);
	file_packet* _packet;
	for (;;)
	{
		if (this->mdatalist.empty())
		{
			break;
		}
		_packet = this->mdatalist.front();
		this->mdatalist.pop_front();
		free(_packet);
	}
}

bool CExecute::isrun()
{
	return ((this->mrun == 0)? false: true);
}

bool CExecute::startserver()
{
	this->mrecv = CreateThread(NULL, 0, 
		(LPTHREAD_START_ROUTINE)CExecute::RecvCallBackThread,
		this, 0, NULL);

	if (this->mrecv == INVALID_HANDLE_VALUE)
		return false;

	this->mdatamanager = CreateThread(NULL, 0,
		(LPTHREAD_START_ROUTINE)CExecute::DataManageThread,
		this, 0, NULL);

	if (this->mdatamanager == INVALID_HANDLE_VALUE)
		return false;
	return true;
}

void CExecute::stopserver()
{
	
	SetEvent(this->mstop);
	
	if (this->mrecv != INVALID_HANDLE_VALUE)
	{
		WaitForSingleObject(this->mrecv, INFINITE);
	}

	if (this->mrecv != INVALID_HANDLE_VALUE)
	{
		WaitForSingleObject(this->mdatamanager, INFINITE);
	}

	printf("处理服务停止\r\n");
	return;
}

DWORD WINAPI CExecute::DataManageThread(void* param)
{
	CExecute *CCExecute = (CExecute*)param;
	HANDLE _event[] = { CCExecute->mevent, CCExecute->mstop };
	int reslt = 0;
	pfile_packet _packet;
	FILE* pfile = NULL;
	TCHAR* tfile = NULL;
	size_t size_write;
	socket_packet msend;
	sockaddr_in* _sockaddr;
	SocketRecv* pSocketRecv = SocketRecvGet();
	bool recvsuccess = false;
	std::list<file_packet*>::iterator iter;
	if (pSocketRecv == NULL)
	{
		::InterlockedDecrement(&CCExecute->mrun);
		return 0;
	}
	_sockaddr = pSocketRecv->getlist(CCExecute->msocket);
	memset(&msend, 0, sizeof(msend));
	msend.flag = true;
	msend.type = SCOKET_START;
	//发送消息表示已经准备好通信;
	_sockaddr->sin_port = htons(CODE_RECV_PORT);
	reslt = sendto(CCExecute->m_udp_socket, (char*)&msend, sizeof(msend), 0, (sockaddr*)_sockaddr, sizeof(sockaddr_in));
	if (reslt < 0)
	{
		printf("发送SCOKET_START到%s失败： %d\r\n", inet_ntoa(_sockaddr->sin_addr), GetLastError());
		goto mExit;
	}

	pSocketRecv->AddRef();
	while (true)
	{
		//reslt = WaitForMultipleObjects(2, _event, 0, INFINITE);
		//三秒轮询一次;
		reslt = WaitForSingleObject(CCExecute->mstop, 1000);
		switch (reslt)
		{
		case WAIT_OBJECT_0:
			goto mExit;
			break;
		case WAIT_OBJECT_0 + 1:
		case WAIT_TIMEOUT:
			break;
		default:
			goto mExit;
		}
		EnterCriticalSection(&CCExecute->mlistlock);
		for (;;)
		{
			//printf("%d\r\n",CCExecute->mdatalist.size());
			if (CCExecute->mdatalist.empty())
			{
				break;
			}
				
			_packet = CCExecute->mdatalist.front();
			CCExecute->mdatalist.pop_front();

			switch (_packet->type)
			{
			case FILE_NAME:
				printf("文件的名字：%S\r\n",(LPCWSTR)_packet->data);
				pfile = _tfopen((LPCWSTR)_packet->data, _T("w+b"));
				if (pfile != NULL)
				{
					msend.type = FILE_UPLOAD_BEGIN;
					//发送消息表示文件已经创建好了;
					sendto(CCExecute->m_udp_socket,(char*)&msend, sizeof(msend), 0, (sockaddr*)_sockaddr, sizeof(sockaddr_in));
					tfile = _tcsdup((LPCWSTR)_packet->data);
				}
				else
				{
					printf("创建文件出错: %d\r\n", GetLastError());
					printf("发送断开消息\r\n");
					msend.type = SOCKET_STOP;
					//发送消息表示socket断开;
					sendto(CCExecute->m_udp_socket,(char*)&msend, sizeof(msend), 0, (sockaddr*)_sockaddr, sizeof(sockaddr_in));
					free(_packet);
					LeaveCriticalSection(&CCExecute->mlistlock);
					
					//CCExecute->mdatalist.pop_front();
					goto mExit;
				}
				break;
			case FILE_UPLOAD_END:
				printf("文件传输结束了\r\n");
				if (_packet->datalen > 0)
				{
					size_write = fwrite(_packet->data, sizeof(char), _packet->datalen, pfile);
					if (size_write != _packet->datalen)
					{
						printf("写入数据出错: %d\r\n", GetLastError());
					}
					else
					{
						recvsuccess = true;
					}
				}
				free(_packet);
				LeaveCriticalSection(&CCExecute->mlistlock);
				
				
				goto mExit;
				break;
			case FILE_UPLOAD_DATA:
				//printf("接收到文件数据... ...\r\n");
				size_write = fwrite(_packet->data, sizeof(char), _packet->datalen, pfile);
				if (size_write != _packet->datalen)
				{
					printf("写入数据出错: %d\r\n", GetLastError());
					printf("发送断开消息\r\n");
					msend.type = SOCKET_STOP;
					//发送消息表示socket断开;
					sendto(CCExecute->m_udp_socket, (char*)&msend, sizeof(msend), 0, (sockaddr*)_sockaddr, sizeof(sockaddr_in));
					free(_packet);
					LeaveCriticalSection(&CCExecute->mlistlock);
					
					//CCExecute->mdatalist.pop_front();
					goto mExit;
				}
				break;
			case FILE_UPLOAD_ERROR:
			case FILE_TRANSFER_OVER:
				printf("传输过程出错\r\n");
				free(_packet);
				LeaveCriticalSection(&CCExecute->mlistlock);
				
				//CCExecute->mdatalist.pop_front();
				goto mExit;
				break;
			default:
				printf("消息类型未知:%d %d\r\n", _packet->type, _packet->datalen);
				LeaveCriticalSection(&CCExecute->mlistlock);
				break;
			}
				
			//printf("packet: %d %d\r\n",_packet->type, _packet->datalen);
			free(_packet);
			
		}
		LeaveCriticalSection(&CCExecute->mlistlock);
	}

mExit:

	if (pfile)
	{
		if (!recvsuccess)
		{
			DeleteFile(tfile);
		}
		fclose(pfile);
	}

	if (tfile)
	{
		free(tfile);
	}
	
	::InterlockedDecrement(&CCExecute->mrun);
	//先释放;
	pSocketRecv->removelist(CCExecute->msocket);
	closesocket(CCExecute->msocket);
	printf("退出数据处理线程: %d\r\n", CCExecute->msocket);
	pSocketRecv->Release();
	return 0;
}

DWORD WINAPI CExecute::RecvCallBackThread(void *param)
{
	CExecute* CCExecute = (CExecute*)param;
	SocketRecv* pSocketRecv = SocketRecvGet();
	
	char*		  mrecv;
	TCHAR*		  tfile = NULL;
	FILE*		  hFile = NULL;
	char recvbuffer[FILE_PACKET_SIZE*2 + 1] = {""};
	char spacket[FILE_PACKET_SIZE] = {""};
	unsigned int  recvcounter = 0;
	unsigned int  pointstn = 0;
	unsigned int  inputlen = 0;
	fd_set	m_set;
	int		reslt;
	timeval m_time = {5000, 0};
	unsigned long ul = 0;
	if (pSocketRecv == NULL)
	{
		::InterlockedDecrement(&CCExecute->mrun);
		return 0;
	}
	

	pSocketRecv->AddRef();
	mrecv = (char*)file_packet_alloc();
	while (true)
	{
		FD_ZERO(&m_set);
		FD_SET(CCExecute->msocket, &m_set);
       //设置5秒超时;
		reslt = select(CCExecute->msocket + 1, &m_set, NULL, NULL, &m_time);
		if (reslt > 0 || !FD_ISSET(CCExecute->msocket, &m_set))
		{
			
			
			int recv_size = recv(CCExecute->msocket, recvbuffer, sizeof(recvbuffer) - 1, 0);
			//printf("从缓存中读取：%d个字节\r\n",recv_size);
			if (recv_size <= 0)
			{
				printf("tcp传输通道终止:%d \r\n", GetLastError());
				EnterCriticalSection(&CCExecute->mlistlock);
				reslt = FILE_TRANSFER_OVER;
				memcpy(mrecv , &reslt, sizeof(FILE_TRANSFER_OVER));
				CCExecute->mdatalist.push_back((pfile_packet)mrecv);
				LeaveCriticalSection(&CCExecute->mlistlock);
				break;
			}
			EnterCriticalSection(&CCExecute->mlistlock);
			
			//拼接数据包
			while (recv_size > pointstn)
			{
				inputlen = (recv_size - pointstn > FILE_PACKET_SIZE - recvcounter) ? FILE_PACKET_SIZE - recvcounter: recv_size - pointstn;
				//接收的数据大于包的长度;
				memcpy(mrecv + recvcounter, recvbuffer + pointstn, inputlen);
				pointstn += inputlen;
				recvcounter += inputlen;

				if (recvcounter == FILE_PACKET_SIZE)
				{
					//memcpy(mrecv, spacket, FILE_PACKET_SIZE);
					CCExecute->mdatalist.push_back((pfile_packet)mrecv);
					mrecv = (char*)file_packet_alloc();
					//memset(spacket, 0, FILE_PACKET_SIZE);
					recvcounter = 0;
				}

			}
			pointstn = 0;
			LeaveCriticalSection(&CCExecute->mlistlock);
			SetEvent(CCExecute->mevent);
			memset(recvbuffer, 0, sizeof(recvbuffer));
		}
		else
		{
			goto mExit;
		}
	
	}
	
mExit:
	
	printf("退出数据接收线程\r\n");
	pSocketRecv->Release();
	::InterlockedDecrement(&CCExecute->mrun);
	return 0;
}

////此处皆为私有
static CRITICAL_SECTION g_cs;
static SocketRecv* pSocketRecv;

void SocketRecvInit()
{
	InitializeCriticalSection(&g_cs);

	EnterCriticalSection(&g_cs);
	if (NULL == pSocketRecv)
	{
		pSocketRecv = new SocketRecv();
	}
	LeaveCriticalSection(&g_cs);
}

SocketRecv* SocketRecvGet()
{
	SocketRecv* reslt;
	
	EnterCriticalSection(&g_cs);
	reslt = pSocketRecv;
	LeaveCriticalSection(&g_cs);

	return reslt;
}

void SocketRecvDone()
{
	EnterCriticalSection(&g_cs);
	if (NULL != pSocketRecv)
	{
		pSocketRecv->Release();
	}
	LeaveCriticalSection(&g_cs);
	DeleteCriticalSection(&g_cs);
}
