#include <memory.h>
#include <fcntl.h>
#include "SocketBroadCast.h"

SocketBroadCast::SocketBroadCast()
{

}

SocketBroadCast::~SocketBroadCast()
{

}

bool SocketBroadCast::startserver()
{
	if (!this->init(false, BROADCAST_PORT))
	{
		return false;
	}
	this->registryhandle(RecvCallBackProc,  CALLBACK_BROADCAST);
	this->server_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)SocketBroadCast::server_thread_proc, this, 0, NULL);
	if (this->server_thread == INVALID_HANDLE_VALUE)
	{
		closesocket(this->socket_id);
		return false;
	}

	return true;
}

void SocketBroadCast::stopserver()
{
	closesocket(this->socket_id);
	WaitForSingleObject(this->server_thread, INFINITE);
	CloseHandle(this->server_thread);
	return;
}

DWORD WINAPI SocketBroadCast::server_thread_proc(void* param)
{
	SocketBroadCast* server = (SocketBroadCast*)param;
	fd_set m_set;
	int reslt;
	timeval m_time;

	m_time.tv_sec = 3;
	m_time.tv_usec = 0;
	while (true)
	{
		FD_ZERO(&m_set);
		FD_SET(server->socket_id, &m_set);

		reslt = select(server->socket_id + 1, &m_set, NULL, NULL, &m_time);

		if (reslt > 0 && FD_ISSET(server->socket_id, &m_set))
		{
			printf("recive the broadcast message \r\n");
			EnterCriticalSection(&server->m_cs);
			if (server->recv_callback[CALLBACK_BROADCAST])
			{
				server->recv_callback[CALLBACK_BROADCAST](server->socket_id, param);
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


void SocketBroadCast::getserverid(sockaddr_in* param)
{
	memcpy(param, &this->server_id, sizeof(sockaddr_in));
}

void RecvCallBackProc(SOCKET sock, void* param)
{
	SocketBroadCast* server = (SocketBroadCast*)param;
	sockaddr_in server_id;
	
	BYTE recvbuff[SOCKET_PACKET_SIZE] = { "" };
	socket_packet  socketrequest;
	psocket_packet socketrecv;
	SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	int socklen = sizeof(sockaddr_in);

	memset(&server_id, 0, sizeof(server_id));
	recvfrom(sock, (char*)recvbuff, sizeof(recvbuff), 0, (sockaddr*)&server_id, &socklen);

	socketrecv = (psocket_packet)recvbuff;
	if (socketrecv->type == SOCKET_BROADCAST && !(s < 0))
	{
		printf("the broadcast message comes from %s \r\n", inet_ntoa(server_id.sin_addr));
		server_id.sin_port = htons(BROADCAST_REQUEST_PORT);
		
		socketrequest.type = SOCKET_ADDR;
		socketrequest.flag = false;
		sendto(s, (char*)&socketrequest, sizeof(socket_packet), 0, (sockaddr*)&server_id, socklen);
	}
	closesocket(s);
	return;
}