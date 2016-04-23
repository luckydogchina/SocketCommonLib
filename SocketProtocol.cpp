#include "SocketProtocol.h"

extern psocket_packet socket_paket_alloc()
{
	psocket_packet _packet = (psocket_packet)malloc(SOCKET_PACKET_SIZE);
	memset(_packet, 0, SOCKET_PACKET_SIZE);
	return _packet;
}
extern pfile_packet file_packet_alloc()
{
	pfile_packet _packet = (pfile_packet)malloc(FILE_PACKET_SIZE);
	memset(_packet, 0, FILE_PACKET_SIZE);
	return _packet;
}