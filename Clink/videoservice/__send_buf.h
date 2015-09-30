#pragma once

#include <WinSock2.h>
#include <x_types/intxx.h>
#include "datsvr.h"
#include "utils/time.h"
#include "pthread.h"

#include "optimizer.h"

extern unsigned short SERVICE_PORT;
extern long g_dif;

inline void __send_buf(SOCKET s, u_int32 ipaddr, u_int16 port, unsigned int datalen, const char* data)
{
	post_view_data(SERVICE_PORT + 1030, data, datalen);

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(ipaddr);
	addr.sin_port = htons(port);
	
//#ifdef OPT_DEBUG_OUT
if((g_dbgClass & opt_send_buf) != 0)
{
	VS_VIDEO_PACKET* packet = (VS_VIDEO_PACKET*)data;

	if(packet->slicecnt > 3 || g_dif < 0)
	{
		union
		{
			unsigned char v[4];
			unsigned int  value;
		}ip;

		ip.value = ipaddr;
	
		struct timeval tv;
		gettimeofday(&tv, NULL);

		ULONGLONG timestamp = ((ULONGLONG)tv.tv_sec * 1000 + tv.tv_usec/1000);

		g_dif = packet->timestamp - timestamp;
		pthread_t self_id = pthread_self();
		LOG(LEVEL_INFO, "thread=%d:%u, SEND >> addr=%d.%d.%d.%d, port=%d, size=%d. stamp=%llu, framenum=%u, slicecnt=%u, slicenum=%u,nowtimestamp=%llu, dif=%ld.\n"
			, (int)self_id.p, self_id.x
			, ip.v[3], ip.v[2], ip.v[1], ip.v[0], port
			, datalen, packet->timestamp, packet->framenum, packet->slicecnt, packet->slicenum, timestamp, g_dif);
	}
}
//#endif//OPT_DEBUG_OUT

	::sendto(s, data, datalen, 0, (struct sockaddr*)&addr, sizeof(addr));

}
