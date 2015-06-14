#pragma once

#include <list>
#include <x_types/intxx.h>
#include <x_vs/xvs.h>
#include <x_util/uuid.h>
#include <set>
#include <WinSock2.h>
#include "TfMethod.h"
#include "msg.h"

using namespace x_util;

class TfAudioOutput
{
public:
	TfAudioOutput(
		const Uuid& signal_id, 
		const Uuid& stream_id,
		const char info[16], 
		u_int32 group_id, 
		const VS_OUTPUT* output, 
		u_int32 multi_ipaddr, 
		u_int16 multi_port, 
		TfMethod* tfm);

	TfAudioOutput(
		const Uuid& signal_id, 
		const Uuid& stream_id,
		const char info[16], 
		u_int32 group_id, 
		const VS_OUTPUT* output, 
		TfMethod* tfm);

	~TfAudioOutput(void);

public:
	bool isthis(const VS_OUTPUT* output);

public:
	int multicast(u_int32 multi_ipaddr, u_int16 multi_port);
	int unicast();
	int online(const VS_DISP_CHANGE* p, std::set<u_int32>& group_list);
	int offline(const VS_DISP_CHANGE* p);
	int setbase(u_int32 group_id, u_int32 base_id, const timeval* tv);
	int packet(AUTOMEM* mem);

private:
	void init(const char info[16]);

private:
	Uuid _signal_id;
	Uuid _stream_id;

private:
	u_int32 _group_id;

private:
	VS_OUTPUT	_output;

private:
	bool _is_multicast;
	u_int32 _multi_ipaddr;
	u_int16 _multi_port;

private:
	u_int64 _time;

private:
	TfMethod* _tfm;

};

