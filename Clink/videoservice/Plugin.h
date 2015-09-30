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
#include <queue>
#include <map>

using namespace x_util;

namespace device
{
//buffer
#define WAVE_TOP(a)			(a + (a>>1))
#define WAVE_FLR(a)			(a - (a>>1))
#define NORMAL_TOP(a)		(a + (a>>2))
#define NORMAL_FLR(a)		(a - (a>>2))

#define POOL_SIZE			(100)

//
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

//
struct PluginBuf
{
public:
	PluginBuf(const VideoData* d, OPT_MEM* om, u_int32 p)
	{
		isV = true;
		data.v = &vd;
		if(d != NULL)
		{
			OPT_MEM* omem = new OPT_MEM(*om);

			//
			memcpy(data.v, d, sizeof(VideoData));
			data.v->data = (char*)omem;
			data.v->datalen = sizeof(OPT_MEM);
		}
		pts = p;
	}
	PluginBuf(const AudioData* d, u_int32 p)
	{
		isV = false;
		data.a = &ad;
		if(d != NULL)
		{
			memcpy(data.a, d, sizeof(AudioData));
			char* ad = new char[d->datalen];
			memcpy(ad, d->data, d->datalen);
			data.a->data = ad;
			data.a->datalen = d->datalen;
		}
		pts = p;
	}

	~PluginBuf(void)
	{
		if(isV)
		{
			OPT_MEM* om = (OPT_MEM*)data.v->data;
			delete om;
		}
		else
		{
			delete[] data.a->data;
		}
	}
		
public:
	union VA
	{
		VideoData* v;
		AudioData* a;
	} data;
	u_int32 pts;

private:
	VideoData vd;
	AudioData ad;
	bool isV;
};

struct StreamBuf
{
public:
	enum BufType
	{
		VIDEO_BUF,
		AUDIO_BUF,
	};

	struct V_PARAM
	{
		u_int32 _orgBufCnt;
		int		_adjust;
		bool	_isFstV;
		u_int32 _nowtimePool[POOL_SIZE];
	};

	struct A_PARAM
	{
		u_int32 _preTimeA;
	};

	union VA_PARAM
	{
		V_PARAM vp;
		A_PARAM ap;
	};

public:
	StreamBuf(BufType bt): _vaType(bt)
		, _readyB(NULL), _isBaseSet(false), _curTime(0), _frameCnt(0)
	{
		pthread_mutex_init(&_bufMtx, NULL);

		if(VIDEO_BUF == _vaType)
		{
			param.vp._adjust = 0;
			param.vp._isFstV = true;
			param.vp._orgBufCnt = 0;
		}
		else
		{
			param.ap._preTimeA = 0;
		}
	}
	~StreamBuf(void)
	{
		pthread_mutex_destroy(&_bufMtx);

		while(! _bufCan.empty())
		{
			delete _bufCan.front();
			_bufCan.pop();
		}
	}

public:
	//commont
	BufType		_vaType;
	PluginBuf*	_readyB;
	bool		_isBaseSet;
	//u_int32		_timeout;
	u_int32		_curTime;
	u_int32		_frameCnt;

	std::queue<PluginBuf*>	_bufCan;
	pthread_mutex_t			_bufMtx;

	VA_PARAM	param;
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
	Instance(const VS_SIGNAL* signal, SigCB cb, void* context, const Uuid& item, u_int32 timeout = 0): _preTime(0)
		, _timeout(timeout)
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
				
		BufRcl();
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
#ifdef OPT_PLUGIN_BUF
		for(int i = 0; i < audio_num; i++)
		{
			Uuid id(audios[i]->id);
			StreamBuf* sb = new StreamBuf(StreamBuf::BufType::AUDIO_BUF);
			std::pair<std::map<std::string, StreamBuf*>::iterator, bool> ret;
			ret = _buffers.insert(std::make_pair(id.string(), sb));
			if(! ret.second)
			{
				LOG(LEVEL_WARNING, "Can't insert buffer! audio, ID(%s).", id.string().c_str());
				delete sb;
			}
			else
			{
				LOG(LEVEL_INFO, "Success insert buffer! audio, ID(%s).", id.string().c_str());
			}
		}
		for(int i = 0; i < video_num; i++)
		{
			Uuid id(videos[i]->id);
			StreamBuf* sb = new StreamBuf(StreamBuf::VIDEO_BUF);
			std::pair<std::map<std::string, StreamBuf*>::iterator, bool> ret;
			ret = _buffers.insert(std::make_pair(id.string(), sb));
			if(! ret.second)
			{
				LOG(LEVEL_WARNING, "Can't insert buffer! video, ID(%s).", id.string().c_str());
				delete sb;
			}
			else
			{
				LOG(LEVEL_INFO, "Success insert buffer! video, ID(%s).", id.string().c_str());
			}
		}
#endif

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
		if(isexit())
		{
			return;
		}

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
		if(isexit())
		{
			return;
		}

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
#endif//OPT_MEM_TO_PTR

		if(_cb)
		{
			_cb(&res, _context, _item);
		}

//#ifdef OPT_DEBUG_OUT
if((g_dbgClass & opt_Plugin) != 0)
{
		struct timeval tv;
		gettimeofday(&tv, NULL);

		u_int32 nowtime = ((u_int32)tv.tv_sec * 1000 + tv.tv_usec/1000);

		long l_dif = nowtime - g_app_preTime;
		g_app_preTime = nowtime;

		int ival = 0;
		g_plugin_cnt++;
		if(g_plugin_cnt < 30)
		{
			g_plugin_nowtime[g_plugin_cnt%30] = nowtime;
		}
		else
		{
			g_plugin_nowtime[g_plugin_cnt%30] = nowtime;
			ival = (g_plugin_nowtime[g_plugin_cnt%30] - g_plugin_nowtime[(g_plugin_cnt+1)%30]) /30;
		}

		if(l_dif > 100 || g_dif < 0 || ival > 35)
		{	
			pthread_t self_id = pthread_self();
			LOG(LEVEL_INFO, "thread=%d:%u, SEND >> data=%d, size=%d. stamp=%llu, nowtime=%u, interval=%ld, 1/fps=%d.\n"
				, (int)self_id.p, self_id.x
				, (int)omem.memData, omem.memLen, p->timestamp, nowtime, l_dif, ival);
		}
}
//#endif//OPT_DEBUG_OUT
	}

	void req_audio_toBuf(const AudioData* p)
	{
		if(isexit())
		{
			return;
		}

		StreamBuf* sb = NULL;
		std::map<std::string, StreamBuf*>::iterator it = _buffers.find(p->stream.string());
		if(it != _buffers.end())
		{
			 sb = it->second;
		}
		else
		{
			LOG(LEVEL_ERROR, "Can't find buffer! audio, id(%s).", p->stream.string().c_str());
			return;
		}

		//
		struct timeval tv;
		gettimeofday(&tv, NULL);
		u_int64 nowtime = ((u_int64)tv.tv_sec * 1000 + tv.tv_usec/1000);
		//if(0 == sb->param.ap._preTimeA)
		//{
		//	sb->param.ap._preTimeA = nowtime;
		//}
		LOG(LEVEL_INFO, "audio, frameno(%u), nowtime(%llu), stamp(%llu), n-s(%lld)."
			, sb->_frameCnt, nowtime, p->timestamp, nowtime-p->timestamp);

		struct PluginBuf* buf = new struct PluginBuf(p, nowtime);//(nowtime - sb->param.ap._preTimeA));
		//sb->param.ap._preTimeA = nowtime;

		//
		pthread_mutex_lock(&sb->_bufMtx);
		sb->_bufCan.push(buf);
		pthread_mutex_unlock(&sb->_bufMtx);

		sb->_frameCnt++;

		if(! sb->_isBaseSet)
		{
			sb->_isBaseSet = true;

			//
			//struct timeval tv;
			//gettimeofday(&tv, NULL);
			//u_int32 nowtime = ((u_int32)tv.tv_sec * 1000 + tv.tv_usec/1000);

			SetBase(sb, nowtime);
			LOG(LEVEL_WARNING, "NOW, audio buffer(%s) base is set! curTime=%u, timeout=%u.", p->stream.string().c_str(), sb->_curTime, _timeout);
		}
	}

	void req_video_toBuf(const VideoData* p)
	{
		if(isexit())
		{
			return;
		}

		StreamBuf* sb = NULL;
		std::map<std::string, StreamBuf*>::iterator it = _buffers.find(p->stream.string());
		if(it != _buffers.end())
		{
			 sb = it->second;
		}
		else
		{
			LOG(LEVEL_ERROR, "Can't find buffer! video, id(%s).", p->stream.string().c_str());
			return;
		}

		char* readyMem = new char[p->datalen];
		memcpy(readyMem, p->data, p->datalen);

		//OPT_MEM* omem = new OPT_MEM();
		OPT_MEM* omem = &OPT_MEM();
		omem->memData = readyMem;
		omem->memLen = p->datalen;
		
		//
		struct timeval tv;
		gettimeofday(&tv, NULL);
		u_int64 nowtime = ((u_int64)tv.tv_sec * 1000 + tv.tv_usec/1000);

		LOG(LEVEL_INFO, "video, frameno(%u), nowtime(%llu), stamp(%llu), n-s(%lld)."
			, sb->_frameCnt, nowtime, p->timestamp, nowtime-p->timestamp);

		struct PluginBuf* buf = new struct PluginBuf(p, omem, GetInterval(sb));
		//buf->data.v->data = (char*)omem;
		//buf->data.v->datalen = sizeof(OPT_MEM);

		//
		pthread_mutex_lock(&sb->_bufMtx);
		int bufSize = sb->_bufCan.size();
		sb->_bufCan.push(buf);
		pthread_mutex_unlock(&sb->_bufMtx);

		if(! sb->_isBaseSet)
		{
			sb->_isBaseSet = true;

			SetBase(sb, nowtime);
			LOG(LEVEL_WARNING, "NOW, video buffer(%s) base is set! curTime=%u, timeout=%u.", p->stream.string().c_str(), sb->_curTime, _timeout);
		}
	}

	u_int32 GetInterval(StreamBuf* sb)
	{
		u_int32 ival = 0;
		struct timeval tv;
		gettimeofday(&tv, NULL);
		u_int32 nowtime = ((u_int32)tv.tv_sec * 1000 + tv.tv_usec/1000);

		if(sb->_frameCnt < POOL_SIZE)
		{
			sb->param.vp._nowtimePool[sb->_frameCnt] = nowtime;
			if(sb->_frameCnt > 0)
			{
				ival = (sb->param.vp._nowtimePool[sb->_frameCnt] - sb->param.vp._nowtimePool[0]) /sb->_frameCnt;
			}
		}
		else
		{
			sb->param.vp._nowtimePool[sb->_frameCnt%POOL_SIZE] = nowtime;
			ival = (sb->param.vp._nowtimePool[sb->_frameCnt%POOL_SIZE] - sb->param.vp._nowtimePool[(sb->_frameCnt+1)%POOL_SIZE]) /POOL_SIZE;
		}

		sb->_frameCnt++;
		return ival;
	}

	//第一帧发送的延时，不支持临时调整
	//req_video_toBuf之前调用
	//或初始化时设置
	void SetTimeout(int ms)
	{
		_timeout = ms;
	}

	//返回发送帧数的数量
	int CheckTimeSend(int sleep_ms)
	{
		int vc = 0;
		int ac = 0;
		for(std::map<std::string, StreamBuf*>::iterator it = _buffers.begin();
			it != _buffers.end(); it++)
		{
			if(StreamBuf::VIDEO_BUF == it->second->_vaType)
			{
				vc = VideoCheckTime(it->second);
			}
			else
			{
				ac = AudioCheckTime(it->second);
			}
		}

		if(sleep_ms != 0)
		{
			Sleep(sleep_ms);
		}
		return (vc + ac);
	}

	int VideoCheckTime(StreamBuf* sb)
	{
		//
		if(NULL == sb->_readyB)
		{
			pthread_mutex_lock(&sb->_bufMtx);
			if(! sb->_bufCan.empty())
			{
				sb->_readyB = sb->_bufCan.front();
				sb->_bufCan.pop();

				//for debug
//#ifdef OPT_DEBUG_OUT
if((g_dbgClass & opt_Plugin) != 0)
{
				sb->_readyB->data.v->timestamp = sb->_bufCan.size();
}
//#endif//OPT_DEBUG_OUT
			}
			pthread_mutex_unlock(&sb->_bufMtx);

			if(sb->_readyB != NULL)
			{
				u_int32 ival = sb->_readyB->pts;
				sb->_readyB->pts = GetPTS(sb, sb->_readyB->pts);
				//LOG(LEVEL_INFO, "$$$$$$$$$$$$$ inter=%u, pts=%u.", ival, _readyV->pts);
			}
			else
			{
				return 0;
			}
		}
		
		struct timeval tv;
		gettimeofday(&tv, NULL);
		u_int32 nowtime = ((u_int32)tv.tv_sec * 1000 + tv.tv_usec/1000);
		if(sb->_readyB->pts <= nowtime)
		{
			Result res;
			res.type = RES_VIDEO_DATA;
			res.ptr = sb->_readyB->data.v;

			//
			if(_cb)
			{
				_cb(&res, _context, _item);
			}

//#ifdef OPT_DEBUG_OUT
			if((g_dbgClass & opt_Plugin) != 0 && g_dif < 0//(_adjust != 0)
//#else
			|| 
			(g_dbgClass & opt_Plugin) == 0 && (sb->_frameCnt %200 == 1))
//#endif//OPT_DEBUG_OUT
			{
				LOG(LEVEL_INFO, "buffer@(%d) video_cb(): _orgBufCnt=%d, _frameCnt=%u, _adjust=%d, curPTS=%u, _timeout=%u, nowtime=%u, outtime_dif=%llu, stamp(buf_size)=%llu."
					, (int)this, sb->param.vp._orgBufCnt, sb->_frameCnt, sb->param.vp._adjust, sb->_curTime, _timeout, nowtime, (ULONGLONG)nowtime - _preTime, sb->_readyB->data.v->timestamp);
			}
			_preTime = nowtime;

			//delete (OPT_MEM*)sb->_readyB->data.v->data;
			delete sb->_readyB;
			sb->_readyB = NULL;

			//
			if(sb->param.vp._isFstV)
			{
				sb->param.vp._isFstV = false;

				pthread_mutex_lock(&sb->_bufMtx);
				int bufSize = sb->_bufCan.size();
				pthread_mutex_unlock(&sb->_bufMtx);
				SetCnt(sb, bufSize);
			}

			return 1;
		}

		return 0;
	}

	int AudioCheckTime(StreamBuf* sb)
	{
		//
		if(NULL == sb->_readyB)
		{
			pthread_mutex_lock(&sb->_bufMtx);
			if(! sb->_bufCan.empty())
			{
				sb->_readyB = sb->_bufCan.front();
				sb->_bufCan.pop();

				//for debug
//#ifdef OPT_DEBUG_OUT
if((g_dbgClass & opt_Plugin) != 0)
{
				sb->_readyB->data.a->timestamp = sb->_bufCan.size();
}
//#endif//OPT_DEBUG_OUT
			}
			pthread_mutex_unlock(&sb->_bufMtx);

			if(sb->_readyB != NULL)
			{
				//sb->_curTime += sb->_readyB->pts;
				//sb->_readyB->pts = sb->_curTime + _timeout;//GetPTS(_readyA->pts);
				sb->_readyB->pts += _timeout +200;
			}
			else
			{
				return 0;
			}
		}
		
		struct timeval tv;
		gettimeofday(&tv, NULL);
		u_int32 nowtime = ((u_int32)tv.tv_sec * 1000 + tv.tv_usec/1000);
		if(sb->_readyB->pts <= nowtime)
		{
			Result res;
			res.type = RES_AUDIO_DATA;
			res.ptr = sb->_readyB->data.a;

			//
			if(_cb)
			{
				_cb(&res, _context, _item);
			}

			if(sb->_frameCnt %200 == 1)
			{
				LOG(LEVEL_INFO, "buffer@ audio_cb(): curPTS=%u, _timeout=%u, nowtime=%u, stamp(buf_size)=%llu."
					, sb->_curTime, _timeout, nowtime, sb->_readyB->data.a->timestamp);
			}

			//delete[] sb->_readyB->data.a->data;
			delete sb->_readyB;
			sb->_readyB = NULL;
			return 1;
		}

		return 0;
	}

private:
	//设置第一帧的发送时间
	void SetBase(StreamBuf* sb, u_int32 now_ms)
	{
		sb->_curTime = now_ms;
	}

	//设置缓存的稳定数量，发送第一帧时调用
	void SetCnt(StreamBuf* sb, int bufSize)
	{
		sb->param.vp._orgBufCnt = bufSize;
	}

	//每帧数据调用一次获取发送时间
	u_int32 GetPTS(StreamBuf* sb, int ival)
	{
		if(sb->param.vp._orgBufCnt == 0)
		{
			goto _UNINITIALIZED;
		}

		pthread_mutex_lock(&sb->_bufMtx);
		u_int32 bufSize = sb->_bufCan.size();
		pthread_mutex_unlock(&sb->_bufMtx);

		//
		if(sb->param.vp._adjust < 0 || bufSize > WAVE_TOP(sb->param.vp._orgBufCnt))
		{
			sb->param.vp._adjust = 0 - (ival>>3);
		}
		else if(sb->param.vp._adjust > 0 || bufSize < WAVE_FLR(sb->param.vp._orgBufCnt))
		{
			sb->param.vp._adjust = ival>>3;
		}
		
		if(sb->param.vp._adjust < 0 && bufSize < NORMAL_TOP(sb->param.vp._orgBufCnt)
			||
			sb->param.vp._adjust > 0 && bufSize > NORMAL_FLR(sb->param.vp._orgBufCnt)
			)
		{
			sb->param.vp._adjust = 0;
		}

_UNINITIALIZED:
		int inc = ival + sb->param.vp._adjust;
		sb->_curTime += inc;
		return (sb->_curTime + _timeout);
	}

	void BufRcl(void)
	{
		for(std::map<std::string, StreamBuf*>::iterator it = _buffers.begin();
			it != _buffers.end();
			it++)
		{
			StreamBuf* sb = it->second;

			int dcnt = 0;
			int dlen = 0;
			//
			PluginBuf* del = NULL;
			int cnt = 0;

			//待发送缓存回收
			if(sb->_readyB != NULL)
			{
				PluginBuf* pb = sb->_readyB;
				sb->_readyB = NULL;
				
				if(StreamBuf::VIDEO_BUF == sb->_vaType)
				{
					OPT_MEM* omem = (OPT_MEM*)pb->data.v->data;
					dcnt++;
					dlen += omem->memLen;

					delete[] omem->memData;
					omem->memData = NULL;
					omem->memLen = 0;
					//delete omem;
				}
				else
				{
					dcnt++;
					dlen += pb->data.a->datalen;
				}
				delete pb;
				pb = NULL;
			}

			//缓存队列回收
			do
			{
				pthread_mutex_lock(&sb->_bufMtx);
				cnt = sb->_bufCan.size();
				if(cnt > 0)
				{
					del = sb->_bufCan.front();
					sb->_bufCan.pop();
					cnt -= 1;
				}
				pthread_mutex_unlock(&sb->_bufMtx);

				if(del != NULL)
				{
					if(StreamBuf::VIDEO_BUF == sb->_vaType)
					{
						OPT_MEM* omem = (OPT_MEM*)del->data.v->data;
						dcnt++;
						dlen += omem->memLen;

						delete[] omem->memData;
						omem->memData = NULL;
						omem->memLen = 0;
						//delete omem;
					}
					else
					{
						dcnt++;
						dlen += del->data.a->datalen;
					}
					delete del;
					del = NULL;
				}
			}
			while(cnt > 0);

			if(dcnt > 0 && dlen > 0)
			{
				if(StreamBuf::VIDEO_BUF == sb->_vaType)
				{
					LOG(LEVEL_INFO, "stream(%s), RECYCLE buffer memory. _timeout=%u, _orgBufCnt=%u, _frameCnt=%u, _adjust=%d, rclCnt=%d, rclLen=%d.\n"
						, it->first.c_str(), _timeout, sb->param.vp._orgBufCnt, sb->_frameCnt, sb->param.vp._adjust, dcnt, dlen);
				}
				else
				{
					LOG(LEVEL_INFO, "stream(%s), RECYCLE buffer memory. _timeout=%u, _frameCnt=%u, rclCnt=%d, rclLen=%d.\n"
						, it->first.c_str(), _timeout, sb->_frameCnt, dcnt, dlen);
				}
			}

			delete sb;
			sb = NULL;
		}

		_buffers.clear();
	}

public:
	bool isexit()
	{
		return _isexit;
	}

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

	//buffer
	u_int32		_timeout;
	std::map<std::string/*UUID*/, StreamBuf*> _buffers;

private:
	evutil_socket_t _exit1_pipe[2];
	evutil_socket_t _exit2_pipe[2];

};

};
