#pragma once

#include <map>
#include <x_types/intxx.h>
#include <x_vs/xvs.h>

#include <x_util/uuid.h>

#include "TfMethod.h"

#include "msg.h"

#include <WinSock2.h>
#include <set>

class VideoSlice;
class TfVideoGroup;
class TfVideoSource
{
public:
	TfVideoSource(const Uuid& signal, const Uuid& stream, const Uuid& block);
	~TfVideoSource(void);

public:
	bool isthis(const Uuid& stream, const Uuid& block);

public:
	int addoutput(u_int32 group_id, const VS_OUTPUT* output, const char info[16], TfMethod* tfm);
	int deloutput(u_int32 group_id, const VS_OUTPUT* output);
	int multicast(u_int32 multi_ipaddr, u_int16 multi_port);
	int unicast();
	int online(const VS_DISP_CHANGE* p, std::set<u_int32>& group_list);
	int offline(const VS_DISP_CHANGE* p);
	int setbase(u_int32 group_id, u_int32 base_id, const timeval* tv);

public:
	int video(const P_VIDEO* video, u_int64 sys_timestamp, u_int32 sys_frameno, int8 replay, u_int32 framenum);

public:
	void timeout();

private:
	void ippp(VideoSlice* p);

private:
	void add(TfVideoGroup* group);
	void del(TfVideoGroup* group);
	TfVideoGroup* get(u_int32 group_id);



private:
	Uuid _signal;
	Uuid _stream;
	Uuid _block;

private:
	std::map<u_int32, TfVideoGroup*> _group_map;

private:
	bool _is_multicast;
	u_int32 _multi_ipaddr;
	u_int16 _multi_port;

private:
	std::list<VideoSlice*>	_video_list;
	std::list<VideoSlice*>  _ipppp_list;

private:
	u_int32 _base_id;
	u_int32 _frame_number;

};

