#include "NetHelper.h"

int NetHelper::Return(Netpump* netpump, const Uuid& item, u_int32 sequence, int code, const char* desc)
{
	VS_STATUS status;
	memset(&status, 0, sizeof(status));

	status.code = code;
	strncpy_s(status.desc, desc, sizeof(status.desc));

	Packet ret(VS_RETURN, VS_RET_STATUS, sizeof(status), sequence, &status); 

	netpump->SendPacket(item, &ret);

	return 0l;
}

int NetHelper::Return(Netpump* netpump, const Uuid& item, u_int32 sequence, unsigned int len, const VS_SIGNAL_DESC* p)
{
	Packet ret(VS_RETURN, VS_RET_SIGNAL_DESC, len, sequence, p); 
	LOG(LEVEL_INFO, "ypeng@ audio_num=%u, video_num=%u.\n", p->audio_num, p->video_num);
	netpump->SendPacket(item, &ret);

	return 0l;
}

int NetHelper::Message(Netpump* netpump, const Uuid& item, u_int32 sequence, unsigned int len, const VS_SIGNAL_DESC* p)
{
	Packet ret(VS_MESSAGE, VS_MSG_SIGNAL_DESC_CHANGE, len, sequence, p); 

	netpump->SendPacket(item, &ret);

	return 0l;
}

int NetHelper::Keepalive(Netpump* netpump, const Uuid& item, u_int32 sequence)
{
	Packet ret(VS_MESSAGE, VS_MSG_KEEPALIVE, 0, sequence, NULL); 

	netpump->SendPacket(item, &ret);

	return 0l;
}

int NetHelper::Message(Netpump* netpump, const Uuid& item, u_int32 sequence, const VS_JOIN* p)
{
	Packet ret(VS_MESSAGE, VS_MSG_JOIN, sizeof(VS_JOIN), sequence, p); 

	netpump->SendPacket(item, &ret);

	return 0l;
}

int NetHelper::Message(Netpump* netpump, const Uuid& item, u_int32 sequence, const VS_SETBASE* p)
{
	Packet ret(VS_MESSAGE, VS_MSG_SETBASE, sizeof(VS_SETBASE), sequence, p); 

	netpump->SendPacket(item, &ret);

	return 0l;
}

int NetHelper::Message(Netpump* netpump, const Uuid& item, u_int32 sequence, const VS_MULTICAST* p)
{
	Packet ret(VS_MESSAGE, VS_MSG_MULTICAST, sizeof(VS_MULTICAST), sequence, p); 

	netpump->SendPacket(item, &ret);

	return 0l;
}


int NetHelper::Message(Netpump* netpump, const Uuid& item, u_int32 sequence, const VS_UNICAST* p)
{
	Packet ret(VS_MESSAGE, VS_MSG_UNICAST, sizeof(VS_UNICAST), sequence, p); 

	netpump->SendPacket(item, &ret);

	return 0l;
}