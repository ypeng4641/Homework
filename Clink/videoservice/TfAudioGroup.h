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

class TfAudioOutput;
class TfAudioGroup
{
public:
	TfAudioGroup(const Uuid& signal, const Uuid& stream, u_int32 group_id, u_int32 multi_ipaddr, u_int16 multi_port);
	TfAudioGroup(const Uuid& signal, const Uuid& stream, u_int32 group_id);
	~TfAudioGroup(void);

public:
	u_int32 id();
	bool isdelete();

public:
	int addoutput(const VS_OUTPUT* output, const char info[16], TfMethod* tfm);
	int deloutput(const VS_OUTPUT* output);
	int multicast(u_int32 multi_ipaddr, u_int16 multi_port);
	int unicast();

	int online(const VS_DISP_CHANGE* p, std::set<u_int32>& group_list);
	int offline(const VS_DISP_CHANGE* p);
	int setbase(u_int32 group_id, u_int32 base_id, const timeval* tv);
	int packet(AUTOMEM* mem);

private:
	Uuid _signal;
	Uuid _stream;
	u_int32 _id;

private:
	bool _is_multicast;
	u_int32 _multi_ipaddr;
	u_int16 _multi_port;

private:
	std::list<TfAudioOutput*> _output_list;

};

