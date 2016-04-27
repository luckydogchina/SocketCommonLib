#ifndef __SOCKET_PROTOCOL_H__
#define __SOCKET_PROTOCOL_H__

#include<stdio.h>
#include<stdlib.h>
#include<tchar.h>
#include<windows.h>
#include <winsock.h>

#ifndef __cplusplus
extern "C"{
#endif

typedef struct
{
	unsigned int type;
	bool		 flag; //trueΪtcp,flaseΪudp;
	sockaddr_in	 sock_id;
}socket_packet, *psocket_packet;

typedef struct 
{
	unsigned int  type;
	unsigned int  datalen;
	unsigned char data[0];	
}file_packet, *pfile_packet;


#define SOCKET_PACKET_SIZE (sizeof(socket_packet))
#define FILE_PACKET_SIZE   (10*1024*sizeof(BYTE)) //过大的字节数会导致包的分片,从而降低传输速度,tcp最大为1460,udp最大为548;

typedef void(*RECV_CALLBACK)(SOCKET sock, void* param);

#define SOCKET_BASE_0		0x0340L
#define SOCKET_ACK			SOCKET_BASE_0 + 1
#define SOCKET_SYN			SOCKET_BASE_0 + 2
#define SCOKET_START		SOCKET_BASE_0 + 3
#define SOCKET_STOP			SOCKET_BASE_0 + 4
#define SOCKET_BROADCAST	SOCKET_BASE_0 + 5
#define SOCKET_ADDR			SOCKET_BASE_0 + 6

#define FILE_BASE_0			0x0780L
#define FILE_NAME			FILE_BASE_0 + 1
#define FILE_UPLOAD_BEGIN	FILE_BASE_0 + 2
#define FILE_UPLOAD_END		FILE_BASE_0 + 3
#define FILE_UPLOAD_DATA	FILE_BASE_0 + 4
#define FILE_UPLOAD_ERROR   FILE_BASE_0 + 5 //这个标志是代表客户端读取文件数据出错;
#define FILE_TRANSFER_OVER  FILE_BASE_0 + 6	//这个标志代表文件传输socket断开,可能是正常断开，也可能是出现错误;

#define BROADCAST_PORT			8384
#define BROADCAST_REQUEST_PORT	8387
#define FILE_RECV_PORT			8385 //这个是tcp传输数据用的端口;
#define CODE_RECV_PORT			8386 //这个是udp接收命令的端口;

//记得要释放;
extern psocket_packet socket_paket_alloc();
//记得要释放;
extern pfile_packet file_packet_alloc();

extern int _closesocket(SOCKET sock);

#ifndef __cplusplus
}
#endif

#endif