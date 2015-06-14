#ifndef NETHELPER_INC
#define NETHELPER_INC

#include <x_vs/xvs.h>
#include <x_pattern/netpump.h>

using namespace x_pattern;

class NetHelper
{
public:
	static int Return(Netpump* netpump, const Uuid& item, u_int32 sequence, int code, const char* desc);
	static int Return(Netpump* netpump, const Uuid& item, u_int32 sequence, unsigned int len, const VS_SIGNAL_DESC* p);
	static int Message(Netpump* netpump, const Uuid& item, u_int32 sequence, unsigned int len, const VS_SIGNAL_DESC* p);
	static int Keepalive(Netpump* netpump, const Uuid& item, u_int32 sequence);
	static int Message(Netpump* netpump, const Uuid& item, u_int32 sequence, const VS_JOIN* p);
	static int Message(Netpump* netpump, const Uuid& item, u_int32 sequence, const VS_SETBASE* p);
	static int Message(Netpump* netpump, const Uuid& item, u_int32 sequence, const VS_MULTICAST* p);
	static int Message(Netpump* netpump, const Uuid& item, u_int32 sequence, const VS_UNICAST* p);
};

#endif
