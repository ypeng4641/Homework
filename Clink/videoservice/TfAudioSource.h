#pragma once

#include <map>
#include <x_types/intxx.h>
#include <x_vs/xvs.h>

#include <x_util/uuid.h>

#include "TfMethod.h"

#include "msg.h"

#include <WinSock2.h>
#include "msg.h"

#include <list>
#include <set>

class AudioSlice;
class TfAudioGroup;
class TfAudioSource
{
public:
	TfAudioSource(const Uuid& signal, const Uuid& stream);
	~TfAudioSource(void);

public:
	bool isthis(const Uuid& stream);

public:
	int addoutput(u_int32 group_id, const VS_OUTPUT* output, const char info[16], TfMethod* tfm);
	int deloutput(u_int32 group_id, const VS_OUTPUT* output);
	int multicast(u_int32 multi_ipaddr, u_int16 multi_port);
	int unicast();
	int online(const VS_DISP_CHANGE* p, std::set<u_int32>& group_list);
	int offline(const VS_DISP_CHANGE* p);
	int setbase(u_int32 group_id, u_int32 base_id, const timeval* tv);

public:
	int audio(const P_AUDIO* audio, u_int64 timestamp);

public:
	void timeout();

private:
	void add(TfAudioGroup* group);
	void del(TfAudioGroup* group);
	TfAudioGroup* get(u_int32 group_id);

private:
	Uuid _signal;
	Uuid _stream;

private:
	std::map<u_int32, TfAudioGroup*> _group_map;

private:
	bool _is_multicast;
	u_int32 _multi_ipaddr;
	u_int16 _multi_port;

private:
	std::list<AudioSlice*>	_audio_list;

private:
	u_int32 _base_id;
	u_int32 _frame_number;

};

