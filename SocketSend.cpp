#include "SocketSend.h"

SocketAddr::SocketAddr()
{
	memset(&this->ServerAddrRecvId, 0, sizeof(ServerAddrRecvId));
	this->mEvent = (NULL, FALSE, FALSE, NULL);
	InitializeCriticalSection(&this->ServerAddrLock);
}

SocketAddr::~SocketAddr()
{
	std::vector<sockaddr_in*>::iterator iter;
	CloseHandle(this->mEvent);

	EnterCriticalSection(&this->ServerAddrLock);
	for (iter = this->ServerAddr.begin(); iter != this->ServerAddr.end(); iter++)
	{
		if (NULL != *iter)
		{
			free(*iter);
		}
	}
	this->ServerAddr.clear();
	LeaveCriticalSection(&this->ServerAddrLock);

	DeleteCriticalSection(&this->ServerAddrLock);
}

bool SocketAddr::init()
{
	struct timeval tv_out;
	//tv_out.tv_sec = 5000;//等待10秒
	//tv_out.tv_usec = 0;
	tv_out = { 5000, 0 };
	this->ServerAddrRecv = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	this->ServerAddrRecvId.sin_family = AF_INET;
	this->ServerAddrRecvId.sin_port = htons(BROADCAST_REQUEST_PORT);
	this->ServerAddrRecvId.sin_addr.s_addr = htonl(INADDR_ANY);

	if (this->ServerAddrRecv == INVALID_SOCKET)
	{
		printf("socket function failed with error: %ld\n", GetLastError());
		return false;
	}
	//设置5秒超时;
	setsockopt(this->ServerAddrRecv, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv_out, sizeof(tv_out));
	printf("setsockopt %d\n", GetLastError());
	if (bind(this->ServerAddrRecv, (sockaddr*)&this->ServerAddrRecvId, sizeof(sockaddr_in)) < 0)
	{
		printf("bind function failed with error: %ld\n", GetLastError());
		closesocket(this->ServerAddrRecv);
		return false;
	}
	return true;
}

void SocketAddr::release()
{
	closesocket(this->ServerAddrRecv);
}

bool SocketAddr::updateserveraddr()
{
	this->mHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)SocketAddr::ServerAddrRecvThread, this, 0, NULL);
	
	if (this->mHandle == INVALID_HANDLE_VALUE)
	{
		return false;
	}
	
	WaitForSingleObject(this->mEvent, INFINITE);
	
	if ( !SendBroadCastMessage())
	{
		return false;
	}
	
	WaitForSingleObject(this->mHandle, INFINITE);
	CloseHandle(this->mHandle);
	return true;
}

DWORD WINAPI SocketAddr::ServerAddrRecvThread(void* param)
{
	SocketAddr* Server = (SocketAddr*)param;
	sockaddr_in* pServerAddr;
	sockaddr_in* pIter;
	int          ServerAddrLen = sizeof(sockaddr_in);
	int          ret;
	bool         bfind = false;
	socket_packet* psockpacket;
	char recvbuff[SOCKET_PACKET_SIZE] = { "" };
	std::vector<sockaddr_in*>::iterator iter;
	//表明已经准备好了;
	SetEvent(Server->mEvent);
	while (true)
	{
		pServerAddr = (sockaddr_in*)malloc(sizeof(sockaddr_in));
		memset(pServerAddr, 0, sizeof(sockaddr_in));
		ret = recvfrom(Server->ServerAddrRecv, recvbuff, sizeof(recvbuff), 0, (sockaddr*)pServerAddr, &ServerAddrLen);
		//printf("超时了\r\n");
		if (ret == SOCKET_ERROR || ret == 0)
		{
			printf("超时了\r\n");
			return 0;
		}
		else
		{
			psockpacket = (socket_packet*)recvbuff;
			if (psockpacket->type == SOCKET_ADDR && !psockpacket->flag)
			{
				EnterCriticalSection(&Server->ServerAddrLock);

				//查找是否已经存在;
				for (iter = Server->ServerAddr.begin(); iter != Server->ServerAddr.end(); iter++)
				{
					pIter = *iter;
					if (!memcmp(&pIter->sin_addr, &pServerAddr->sin_addr, sizeof(pIter->sin_addr)))
					{
						bfind = true;
						break;
					}
				}

				if (!bfind)
				{
					printf("the server addr :%s \r\n", inet_ntoa(pServerAddr->sin_addr));
					Server->ServerAddr.push_back(pServerAddr);
				}

				bfind = false;
				LeaveCriticalSection(&Server->ServerAddrLock);
			}
		}
	}
}

SocketSend::SocketSend(const char* dest)
{
	if (NULL != dest)
	{
		this->tcpdest = strdup(dest);
	}
	
	memset(&this->tcpaddr, 0, sizeof(this->tcpaddr));
	this->tcpaddr.sin_addr.s_addr = inet_addr(this->tcpdest);
	this->tcpaddr.sin_family = AF_INET;
	this->tcpaddr.sin_port = htons(FILE_RECV_PORT);
	memset(&this->updbind, 0, sizeof(this->updbind));
	hSocketStart = CreateEvent(NULL, FALSE, FALSE, _T("start"));
	hSocketStop =  CreateEvent(NULL, FALSE, FALSE, _T("stop"));
	hUploadBegin = CreateEvent(NULL, TRUE,  FALSE, _T("upload"));
}

SocketSend::~SocketSend()
{
	CloseHandle(hSocketStart);
	CloseHandle(hSocketStop);
	CloseHandle(hUploadBegin);
	free(this->tcpdest);
}

bool SocketSend::init()
{
	int reslt;
	unsigned long ul = 1;
	
	this->udpcoderecv = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if (this->udpcoderecv < 0)
	{
		printf("this socket is created error %d\r\n", GetLastError());
	}

	this->updbind.sin_addr.s_addr = htonl(INADDR_ANY);
	this->updbind.sin_family = AF_INET;
	this->updbind.sin_port = htons(CODE_RECV_PORT);
	
	reslt = ioctlsocket(this->udpcoderecv, FIONBIO, (unsigned long *)&ul);
	if (reslt < 0 )
	{
		goto mExit;
	}

	reslt = bind(this->udpcoderecv, (sockaddr*)&this->updbind, sizeof(this->updbind));
	if (reslt < 0)
	{
		printf("this socket is binded error %d\r\n", GetLastError());
		goto mExit;
	}

	return true;
mExit:
	//closesocket(this->tcptransfer);
	closesocket(this->udpcoderecv);
	return false;
}

bool SocketSend::inittcp()
{
	int reslt;
	struct timeval tv_out;
	tv_out = { 5000, 0 };
	
	this->tcptransfer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	
	if (this->tcptransfer < 0)
	{
		printf("this socket is created error %d\r\n", GetLastError());
	}

	//设置5秒超时;
	reslt = setsockopt(this->tcptransfer, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv_out, sizeof(tv_out));

	if (reslt < 0)
	{
		closesocket(this->tcptransfer);
		return false;
	}
	return true;
}


bool SocketSend::startserver()
{
	if (!this->init())
	{
		return false;
	}
	this->hudpcoderecv = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)SocketSend::udpcoderecvhread, this, 0, NULL);

	if (this->hudpcoderecv == INVALID_HANDLE_VALUE)
	{
		//closesocket(this->tcptransfer);
		closesocket(this->udpcoderecv);
		return false;
	}

	return true;
}

void SocketSend::stopserver()
{
	closesocket(this->udpcoderecv);
	WaitForSingleObject(this->hudpcoderecv, INFINITE);
	CloseHandle(this->hudpcoderecv);
	return;
}

bool SocketSend::sendfile(const TCHAR* path, const TCHAR* filename)
{
	int reslt;
	pfile_packet psend, pname;

	size_t size_read;
	DWORD dwRead = 0;
	FILE* hFile = NULL;
	HANDLE hEvent[] = {this->hSocketStart, this->hUploadBegin, this->hSocketStop};
	//fd_set mset;

	if (path == NULL || filename == NULL)
	{
		return false;
	}

	if (!this->inittcp())
	{
		return false;
	}

	hFile = _tfopen(path, _T("r+b"));
	if (hFile == NULL)
	{
		return 0;
	}

	reslt = connect(this->tcptransfer, (sockaddr*)&this->tcpaddr, sizeof(this->tcpaddr));
	if (reslt < 0)
	{
		printf("connect failure %d\r\n", GetLastError());
		fclose(hFile);
		return 0;
	}
	printf("请求连接成功\r\n");
	psend = (pfile_packet)malloc(FILE_PACKET_SIZE);
	memset(psend, 0, FILE_PACKET_SIZE);
	psend->type = FILE_UPLOAD_DATA;
	psend->datalen = FILE_PACKET_SIZE - sizeof(file_packet);

	while (true)
	{
		//五秒等待延时;
		reslt = WaitForMultipleObjects(3, hEvent, FALSE, 5000);
		
		switch (reslt)
		{
		case WAIT_OBJECT_0:
			pname = file_packet_alloc();
			pname->datalen = _tcslen(filename)*sizeof(TCHAR);
			pname->type = FILE_NAME;

			memcpy(pname->data, filename, pname->datalen);

			reslt = send(this->tcptransfer, (char*)pname, FILE_PACKET_SIZE, 0);
			//ResetEvent(this->hSocketStart);
			if (reslt < 0)
			{
				free(pname);
				goto mExit;
			}
			free(pname);
			break;
		case WAIT_OBJECT_0 + 1:
			size_read = fread(psend->data, sizeof(char), psend->datalen, hFile);
			
			printf("读取了:%d个字节 %d\r\n", size_read, GetLastError());
			//读完;
			if (size_read < psend->datalen)
			{
				ResetEvent(this->hUploadBegin);
				if (feof(hFile))
				{
					psend->type = FILE_UPLOAD_END;
					psend->datalen = size_read;
				}
				else //出错了
				{
					printf("出错了亲: %d！！！\r\n",GetLastError());
					psend->type = FILE_UPLOAD_ERROR;
					psend->datalen = 0;
				}
				//bover = true;
			}
			else //正常
			{
				psend->type = FILE_UPLOAD_DATA;
				psend->datalen = size_read;		
			}

			//FD_ZERO(&mset);
			//FD_SET(this->tcptransfer, &mset);
			
			//select(this->tcptransfer, NULL, &mset, NULL, 0);
			reslt = send(this->tcptransfer, (char*)psend, FILE_PACKET_SIZE, 0);
			if (reslt < 0)
			{
				printf("发送出错：%d \r\n", GetLastError());
				goto mExit;
			}
			Sleep(1);

			if (psend->type == FILE_UPLOAD_END)
			{
				//传输完成了就跑;
				printf("成功完成任务\r\n");
				closesocket(this->tcptransfer);
				fclose(hFile);
				free(psend);
				return true;
			}
			memset(psend, 0, FILE_PACKET_SIZE);
			psend->type = FILE_UPLOAD_DATA;
			psend->datalen = FILE_PACKET_SIZE - sizeof(file_packet);

			break;
		case WAIT_OBJECT_0 + 2:
			{
				printf("服务端主动断开了传输通道\r\n");
				goto mExit;
			}
			break;
		case WAIT_TIMEOUT:
			printf("等待超时\r\n");
		default:
			goto mExit;
		}

	}

	
mExit:
	closesocket(this->tcptransfer);
	fclose(hFile);
	free(psend);
	return false;
}

DWORD WINAPI SocketSend::udpcoderecvhread(void* param)
{
	SocketSend* server = (SocketSend*)param;
	
	fd_set m_set;
	int reslt;

	char recvbuff[SOCKET_PACKET_SIZE] = {""};
	sockaddr_in _sockaddr;
	int addrlen = sizeof(sockaddr);
	psocket_packet pcode;

	while (true)
	{
		FD_ZERO(&m_set);
		FD_SET(server->udpcoderecv, &m_set);

		reslt = select(server->udpcoderecv + 1, &m_set, NULL, NULL, NULL);

		if (reslt > 0 && FD_ISSET(server->udpcoderecv, &m_set))
		{
			recvfrom(server->udpcoderecv, (char*)recvbuff, sizeof(recvbuff), 0, (sockaddr*)&_sockaddr, &addrlen);
			pcode = (psocket_packet)recvbuff;
			printf("接收到一条消息:%s\r\n", inet_ntoa(_sockaddr.sin_addr));
			if (memcmp(inet_ntoa(_sockaddr.sin_addr), inet_ntoa(server->tcpaddr.sin_addr), sizeof(_sockaddr.sin_addr)))
			{
				printf("消息发送方未知\r\n");
				memset(recvbuff, 0, sizeof(recvbuff));
				memset(&_sockaddr, 0, sizeof(_sockaddr));
				continue;
			}

			switch (pcode->type)
			{
			case SCOKET_START:
				printf("接收方准备完毕\r\n");
				SetEvent(server->hSocketStart);
				break;
			case SOCKET_STOP:
				printf("接收方关闭了连接\r\n");
				SetEvent(server->hSocketStop);
				break;
			case FILE_UPLOAD_BEGIN:
				printf("接收方可以开始接收了\r\n");
				SetEvent(server->hUploadBegin);
			default:
				break;
			}

			memset(recvbuff, 0, sizeof(recvbuff));
			memset(&_sockaddr, 0, sizeof(_sockaddr));
		}

		if (reslt < 0)
		{
			printf("exit the thread\r\n");
			break;
		}
	}
	return 0;
}



bool SendBroadCastMessage()
{
	SOCKET		 BroadCastSocket;
	sockaddr_in  BroadCast;
	int			 optval = 1;
	socket_packet msend;

	BroadCastSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (BroadCastSocket == INVALID_SOCKET) 
	{
		printf("socket function failed with error: %ld\n", GetLastError());
		return false;
	}

	//将socket设置为广播socket;
	if (setsockopt(BroadCastSocket, SOL_SOCKET, SO_BROADCAST, (char*)&optval, sizeof(optval)) < 0)
	{
		printf("setsockopt error: %d\r\n", GetLastError());
		closesocket(BroadCastSocket);
		return false;
	}
	memset(&BroadCast, 0, sizeof(BroadCast));
	BroadCast.sin_family = AF_INET;
	BroadCast.sin_port = htons(BROADCAST_PORT);
	BroadCast.sin_addr.s_addr = INADDR_BROADCAST;

	memset(&msend, 0, sizeof(msend));
	msend.flag = false;
	msend.type = SOCKET_BROADCAST;
	//发送三次广播消息;
	for (int i = 0; i < 3; i++)
	{
		sendto(BroadCastSocket, (char*)&msend, sizeof(msend), 0, (sockaddr*)&BroadCast, sizeof(sockaddr_in));
		printf("send a broadCast message\r\n");
		Sleep(1000);
	}

	closesocket(BroadCastSocket);
	return true;
}