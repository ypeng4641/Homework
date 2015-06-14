#pragma once

#include <vector>

#include <x_vs/xvs.h>
#include <x_types/intxx.h>
#include <x_util/uuid.h>
#include <x_util/iphelper.h>

#include <pthread.h>

#include <x_log/msglog.h>
#include <event2/event.h>

#include <network/network.h>
#include "utils/time.h"

#include "optimizer.h"

using namespace x_util;

namespace device
{

enum 
{
	RES_ERROR,
	RES_DESC,
	RES_AUDIO_DATA,
	RES_VIDEO_DATA,
};

struct Error
{
	int			error;
};

struct Desc {
    u_int32                 audio_num; 
    u_int32                 video_num; 
	VS_AUDIO_STREAM_DESC**	audios;
	VS_VIDEO_STREAM_DESC**	videos;
};

struct AudioData
{
	Uuid		stream;
	Uuid		stampId;
	u_int64		timestamp;
	u_int8		codec;
	u_int32		samples;
	u_int8		channels;
	u_int8		bit_per_samples;
	u_int32		datalen;
	char*		data;
};

struct VideoData
{
	Uuid		 stream;		//
	Uuid		 block;			//
	Uuid		 stampId;
	u_int64		 timestamp;		//
	bool		 useVirtualStamp;//使用平均帧率算法
	VS_AREA		 area;
	VS_RESOL	 resol;
	VS_AREA		 valid;
	u_int8		 codec;
	u_int8		 frametype;		// 0:I帧， 1:P帧
	u_int32		 datalen;
	char*		 data;

};

struct Result
{
	unsigned int type;
	const void*	 ptr;
};

typedef int (*SigCB)(const Result* m, void* context, const Uuid& item);

class Plugin
{
public:
	virtual ~Plugin()
	{
	}

public:
	virtual const char* Name() = 0;
	virtual u_int32		Type() = 0;
	virtual int			Init() = 0;
	virtual int			Deinit() = 0;
	virtual void*		Open(const VS_SIGNAL* signal, SigCB cb, void* context, const Uuid& item) = 0;
	virtual int			Close(void* handle) = 0;
	virtual int			SetIFrame(void* handle) = 0;
	virtual int			Error(void* handle) = 0; 

};

class Instance
{
public:
	Instance(const VS_SIGNAL* signal, SigCB cb, void* context, const Uuid& item)
	{
		unsigned int len = sizeof(VS_SIGNAL) + signal->datalen;

		_signal = reinterpret_cast<VS_SIGNAL*>(new char[len]);
		memcpy(_signal, signal, len);
		_cb = cb;
		_context = context;
		_item = item;

		_isexit = false;
		_iframe = false;
		_error = 0;

		evutil_socketpair(AF_INET, SOCK_STREAM, 0, _exit1_pipe);
		evutil_socketpair(AF_INET, SOCK_STREAM, 0, _exit2_pipe);

	}

	virtual ~Instance()
	{
		evutil_closesocket(_exit1_pipe[0]);
		evutil_closesocket(_exit1_pipe[1]);
		evutil_closesocket(_exit2_pipe[0]);
		evutil_closesocket(_exit2_pipe[1]);

		delete[] reinterpret_cast<char*>(_signal);
	}

public:
	const VS_SIGNAL* signal() const
	{
		return _signal;
	}

	u_int32 ipaddr()
	{
		return _signal->ipaddr;
	}

	u_int32 port()
	{
		return _signal->port;
	}

	void req_error(int err)
	{
		//
		_error = err;

		//
		Error data;
		data.error = err;

		Result res;
		res.type = RES_ERROR;
		res.ptr = &data;

		if(_cb)
		{
			_cb(&res, _context, _item);
		}

	}

	void req_desc(u_int32 audio_num, VS_AUDIO_STREAM_DESC**	audios, u_int32 video_num, VS_VIDEO_STREAM_DESC**	videos)
	{
		Desc data;
		data.audio_num = audio_num;
		data.audios = audios;
		data.video_num = video_num;
		data.videos = videos;

		Result res;
		res.type = RES_DESC;
		res.ptr = &data;
		//LOG(LEVEL_INFO, "20150104 req_desc, audio_num=%u, video_num=%u.\n", audio_num, video_num);
		if(_cb)
		{
			_cb(&res, _context, _item);
		}
	}

	void req_audio(const AudioData* p)
	{
		Result res;
		res.type = RES_AUDIO_DATA;
		res.ptr = p;
		//LOG(LEVEL_INFO, "20150104 req_audio, dataLen=%d.\n");
		if(_cb)
		{
			_cb(&res, _context, _item);
		}
	}

	void req_video(const VideoData* p)
	{
#ifdef OPT_MEM_TO_PTR
		VideoData vdp;
		memcpy(&vdp, p, sizeof(VideoData));

		char* readyMem = new char[vdp.datalen];
		memcpy(readyMem, vdp.data, vdp.datalen);

		OPT_MEM omem = {readyMem, vdp.datalen};
		vdp.data = (char*)&omem;
		vdp.datalen = sizeof(OPT_MEM);

		Result res;
		res.type = RES_VIDEO_DATA;
		res.ptr = &vdp;
#else
		Result res;
		res.type = RES_VIDEO_DATA;
		res.ptr = p;
		
		//struct timeval tv;
		//gettimeofday(&tv, NULL);

		//ULONGLONG timestamp = ((ULONGLONG)tv.tv_sec * 1000 + tv.tv_usec/1000);
		//LOG(LEVEL_INFO, "20150104 req_video, stamp=%llu, dataLen=%u, frametype=%x, curTime=%llu, dif=%llu.\n", p->timestamp, p->datalen, p->frametype, timestamp, timestamp-_preTime);
		//_preTime = timestamp;

#if 0
		char buf[256] = {0};
		sprintf(buf, "D:/%ux%u.h264", p->resol.w, p->resol.h);
		FILE* fp = fopen(buf, "ab");
		if(fp != NULL)
		{
			fwrite(p->data, sizeof(char), p->datalen, fp);
			fclose(fp);
		}
		else
		{
			LOG(LEVEL_ERROR, "Can't open file(%s).\n", buf);
		}
#endif
#endif//OPT_MEM_TO_PTR

		if(_cb)
		{
			_cb(&res, _context, _item);
		}
	}

	bool isexit()
	{
		return _isexit;
	}

public:
	void exit()
	{
		//
		char flag = 0x0;
		send(_exit1_pipe[0], &flag, sizeof(flag), 0);
		send(_exit2_pipe[0], &flag, sizeof(flag), 0);

		//
		_isexit = true;
		LOG(LEVEL_INFO, "Plugin(%u) exit signal.###\n", this);
	}

	void set_i_frame()
	{
		LOG(LEVEL_INFO, "set i frame");
		_iframe = true;
	}

	int geterror()
	{
		return _error;
	}

public:
	void wait_exit(fd_set* fdset)
	{
		FD_SET(_exit1_pipe[1], fdset);
	}

	bool wait_i_frame()
	{
		fd_set fdset;
		struct timeval tv = {0, 50000};

		FD_ZERO(&fdset);
		FD_SET(_exit2_pipe[1], &fdset);

		int nfds = Network::Select (FD_SETSIZE, &fdset, NULL, NULL, &tv);
		if( nfds > 0)
		{
			return false;
		}
		
		bool ret = _iframe;
		_iframe = false;
		return ret;

	}

public:
	bool newthread()
	{
		pthread_t thread;
		if(pthread_create(&thread, NULL, staticloop, this) != 0)
		{
			return false;
		}

		pthread_detach(thread);
		return true;
	}

	void waiting()
	{
		while(true)
		{
			fd_set fdset;
			struct timeval tv = {0, 100000};

			FD_ZERO(&fdset);
			wait_exit(&fdset);

			int nfds = Network::Select (FD_SETSIZE, &fdset, NULL, NULL, &tv);
			if( nfds > 0)
			{
				break;
			}
		}

		//必须等这个标注为true， 
		//否则"在执行exit()过程中", wait()就返回导致delete instance提前与exit()完成。
		u_int32 cnt = 1;
		while(!isexit())
		{
			if(cnt++ %1000 == 0)
			{
				LOG(LEVEL_WARNING, "Already waiting %u*10 ms!!", cnt);
				break;
			}
			Sleep(10);
		}
	}

private:
	static void* staticloop(void* arg)
	{
		//
		Instance* instance = reinterpret_cast<Instance*>(arg);
		instance->threadloop();
		instance->waiting();
		//
		delete instance;

		//
		return NULL;
	}

	virtual void threadloop() = 0;

private:
	VS_SIGNAL* _signal;
	SigCB _cb;
	void* _context;
	Uuid _item;

	bool _isexit;
	bool _iframe;
	int  _error;

	//debug
	ULONGLONG _preTime;

private:
	evutil_socket_t _exit1_pipe[2];
	evutil_socket_t _exit2_pipe[2];

};

};
