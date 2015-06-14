#pragma once

#include <x_pattern/netpump.h>
#include <x_pattern/controler.h>
#include <x_util/uuid.h>
#include <x_vs/xvs.h>

#include "TfMethod.h"

#include "msg.h"

#include "Stamp2Showtime.h"

using namespace x_util;

class TfAudioSource;
class TfVideoSource;
class TfSignal
{
public:
	TfSignal(const Uuid& id);
	~TfSignal(void);

public:
	int addoutput(const Uuid& stream_id, const Uuid& block_id, u_int32 group_id, const VS_OUTPUT* output, const char info[16], TfMethod* tfm);
	int deloutput(const Uuid& stream_id, const Uuid& block_id, u_int32 group_id, const VS_OUTPUT* output);
	int multicast(const Uuid& stream_id, const Uuid& block_id, u_int32 multi_ipaddr, u_int16 multi_port);
	int unicast(const Uuid& stream_id, const Uuid& block_id);

	int addoutput(const Uuid& stream_id, u_int32 group_id, const VS_OUTPUT* output, const char info[16], TfMethod* tfm);
	int deloutput(const Uuid& stream_id, u_int32 group_id, const VS_OUTPUT* output);	
	int multicast(const Uuid& stream_id, u_int32 multi_ipaddr, u_int16 multi_port);	
	int unicast(const Uuid& stream_id);

	int setbase(u_int32 group_id);
	int online(const VS_DISP_CHANGE* p);
	int offline(const VS_DISP_CHANGE* p);
	int desc(const VS_SIGNAL_DESC* p);
	int audio(const P_AUDIO* p);
	int video(const P_VIDEO* p);

public:
	void timeout();

private:
	void newbase();
	void setbase(u_int32 group_id, u_int32 base_id, const timeval* basetime);

private:
	TfAudioSource* get(const Uuid& stream);
	TfVideoSource* get(const Uuid& stream, const Uuid& block);

private:
	Stamp2Showtime* gets2s(const Uuid& stampId);

private:
	Uuid _id;

private:
	u_int32 _base_id;
	struct timeval _base_time;

private:
	std::list<TfAudioSource*> _audio_list;
	std::list<TfVideoSource*> _video_list;

private:
	std::list<Stamp2Showtime*> _s2s_list;

//虚屏时间戳同步
private:
	u_int64 _nowBlockStamp;
	u_int64 _nowSysTime;

	u_int64 _preSysTime;
};

