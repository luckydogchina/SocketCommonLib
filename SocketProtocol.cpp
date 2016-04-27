#include "SocketProtocol.h"

extern psocket_packet socket_paket_alloc()
{
	psocket_packet _packet = (psocket_packet)malloc(SOCKET_PACKET_SIZE + 1);
	memset(_packet, 0, SOCKET_PACKET_SIZE + 1);
	return _packet;
}
extern pfile_packet file_packet_alloc()
{
	pfile_packet _packet = (pfile_packet)malloc(FILE_PACKET_SIZE + 1);
	memset(_packet, 0, FILE_PACKET_SIZE + 1);
	return _packet;
}

extern int _closesocket(SOCKET sock)
{
	int reslt;
	if (sock != INVALID_SOCKET)
	{
		reslt = ::closesocket(sock);
		sock = INVALID_SOCKET;
	}
	return reslt;
}