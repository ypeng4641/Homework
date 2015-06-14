#pragma once

#include <list>
#include <x_types/intxx.h>
#include <x_vs/xvs.h>
#include <x_util/uuid.h>

#include <set>
#include <WinSock2.h>

#include "TfMethod.h"

using namespace x_util;

class VideoSlice;
class TfVideoOutput
{
public:
	TfVideoOutput(
		const Uuid& signal_id, 
		const Uuid& stream_id, 
		const char info[16], 
		u_int32 group_id, 
		const VS_OUTPUT* output, 
		u_int32 multi_ipaddr, 
		u_int16 multi_port, 
		const std::list<VideoSlice*>& ippp_list, 
		TfMethod* tfm);

	TfVideoOutput(
		const Uuid& signal_id, 
		const Uuid& stream_id,
		const char info[16], 
		u_int32 group_id, 
		const VS_OUTPUT* output, 
		const std::list<VideoSlice*>& ippp_list, 
		TfMethod* tfm);

	~TfVideoOutput(void);

public:
	bool isthis(const VS_OUTPUT* output);

public:
	int multicast(u_int32 multi_ipaddr, u_int16 multi_port);
	int unicast();
	int online(const VS_DISP_CHANGE* p, std::set<u_int32>& group_list, const std::list<VideoSlice*>& ippp_list);
	int offline(const VS_DISP_CHANGE* p);
	int setbase(u_int32 group_id, u_int32 base_id, const timeval* tv);
	int packet(VideoSlice* p);

public:
	void timeout();

public:
	void clear();
	void init(const std::list<VideoSlice*>& ippp_list);
	u_int64 diff(const struct timeval* tv);

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
	bool _is_ippp;

private:
	std::list<VideoSlice*> _video_list;

private:
	u_int64 _time;

private:
	bool _is_first;
	struct timeval _prev_time;

private:
	TfMethod* _tfm;

};

