#include "imp_ipms_replay.h"
#include "IpmsNetSDK.h"
#include "IpmsNetSDK.h"
#include <error/xerror.h>
#include <list>
#include <map>
#include <math.h>

class Self_IPMS_Replay: public Instance
{
private:
#pragma pack(push)
#pragma pack(1)
	struct DefExt
	{
		char	proxy_for_xlan[32];
		u_int32	chan_for_xlan;
		char	name[32];
		char	passwd[32];
		Uuid	channel;
		Uuid	stream;
		u_int64	begintime;
		u_int64	endtime;
	};

#pragma pack(pop)

public:
	Self_IPMS_Replay(const VS_SIGNAL* signal, SigCB cb, void* context, const Uuid& item): Instance(signal, cb, context, item),
		_buf1(NULL),
		_buf2(NULL)
	{
		if(!getext(_defext))
		{
			memset(&_defext, 0, sizeof(_defext));
		}

	}

	~Self_IPMS_Replay()
	{
		if(_buf1)
		{
			delete[] _buf1;
		}

		if(_buf2)
		{
			delete[] _buf2;
		}

	}

private:
	bool getext(DefExt& ext)
	{
		//
		const VS_SIGNAL* p = reinterpret_cast<const VS_SIGNAL*>(signal());
		if(p->datalen != sizeof(DefExt))
		{
			return false;
		}

		// 
		memcpy(&ext, p->data, p->datalen);
		ext.name[sizeof(ext.name) - 1] = '\0';
		ext.passwd[sizeof(ext.passwd) - 1] = '\0';
		return true;

	}

private:
	void new_id()
	{
		_video_stream_id.Generate();
		_video_stamp_id.Generate();
		_audio_stamp_id.Generate();

		_stream2id.clear();
	}

	Uuid new_id(int stream)
	{
		Uuid id;
		id.Generate();

		// 
		_stream2id.insert(std::make_pair(stream, id));

		// 
		return id;
	}

	bool get_id(int stream, Uuid& id)
	{
		std::map<int, Uuid>::iterator it = _stream2id.find(stream);
		if(it != _stream2id.end())
		{
			id = it->second;
			return true;
		}

		return false;
	}

private:
	void get(int stream_num, IPMS_STREAM_t streams[], std::list<IPMS_STREAM_t>& audio_list, std::list<IPMS_STREAM_t>& video_list)
	{
		for(int i = 0; i < stream_num; i++)
		{
			if(streams[i].is_video == 0)
			{
				if(streams[i].format == IPMS_MEDIA_FMT_AAC)
				{
					audio_list.push_back(streams[i]);
				}
			}
			else
			{
				if(streams[i].format == IPMS_MEDIA_FMT_H264 || streams[i].format == IPMS_MEDIA_FMT_MPEG4)
				{
					video_list.push_back(streams[i]);
				}
			}
		}
	}

	bool get(const std::list<IPMS_STREAM_t>& video_list, VS_RESOL& resol, u_int8& rows, u_int8& cols, u_int8& grade)
	{
		// 
		u_int8 r = 0;
		u_int8 c = 0;

		if(video_list.size() == 0)
		{
			return false;
		}

		IPMS_STREAM_t base = video_list.front();
		for(std::list<IPMS_STREAM_t>::const_iterator it = video_list.begin();
			it != video_list.end();
			it++)
		{
			if(base.video.region.w != it->video.region.w || 
			   base.video.region.h != it->video.region.h ||
			   base.video.width    != it->video.width ||
			   base.video.height   != it->video.height)
			{
				return false;
			}
		}

		r = (u_int8)(1 / base.video.region.h + 0.5);
		c = (u_int8)(1 / base.video.region.w + 0.5);

		if(r * c != video_list.size())
		{
			return false;
		}		

		resol.w = (u_int32)(c * base.video.width * base.video.clip.w + 0.5);
		resol.h = (u_int32)(r * base.video.height * base.video.clip.h + 0.5);

		rows = r;
		cols = c;

		grade = get(base.video.width, base.video.height);

		return true;
	}

	void get(IPMS_STREAM_t& desc, u_int8& row, u_int8& col)
	{
		row = (u_int8)(desc.video.region.x / desc.video.region.w + 0.5);
		col = (u_int8)(desc.video.region.y / desc.video.region.h + 0.5);

	}

	u_int8 get(u_int32 w, u_int32 h)
	{
		if(w <= 720)
		{
			return 6;
		}
		else if(w <= 1280)
		{
			return 16;
		}
		else if(w <= 1920)
		{
			return 33;
		}
		else
		{
			return 100;
		}

	}


private:
	void videoproc(IPMS_FRAME_t* frame)
	{
		//
		Uuid id;
		if(!get_id(frame->stream_id, id))
		{
			return;
		}

		//
		VideoData vdata;

		vdata.stream = _video_stream_id;
		vdata.block = id;
		vdata.stampId = _video_stamp_id;

		vdata.area.x = (u_int32)(_resol.w * frame->video.region.x + 0.5);
		vdata.area.y = (u_int32)(_resol.h * frame->video.region.y + 0.5);
		vdata.area.w = (u_int32)(_resol.w * frame->video.region.w + 0.5);
		vdata.area.h = (u_int32)(_resol.h * frame->video.region.h + 0.5);

		vdata.frametype = frame->video.frame_type == IPMS_FRAME_IFRAME ? 0 : 1;

		vdata.resol.w = frame->video.width;
		vdata.resol.h = frame->video.height;	

		vdata.valid.x = (u_int32)(frame->video.width * frame->video.clip.x + 0.5);
		vdata.valid.y = (u_int32)(frame->video.height * frame->video.clip.y + 0.5);
		vdata.valid.w = (u_int32)(frame->video.width * frame->video.clip.w + 0.5);
		vdata.valid.h = (u_int32)(frame->video.height * frame->video.clip.h + 0.5);

		vdata.codec = frame->media_fmt == IPMS_MEDIA_FMT_H264 ? CODEC_H264 : CODEC_MPEG4;

		vdata.timestamp = frame->timestamp;
		vdata.useVirtualStamp = true;

		vdata.datalen = frame->frame_size;
		vdata.data = (char*)frame->frame_data;

		//
		Instance::req_video(&vdata);


	}

	void audioproc(IPMS_FRAME_t* frame)
	{
		//
		Uuid id;
		if(!get_id(frame->stream_id, id))
		{
			return;
		}

		// 
		AudioData adata;
		adata.stream = id;
		adata.stampId = _audio_stamp_id;
		adata.timestamp = frame->timestamp;
		adata.codec = CODEC_AAC;
		adata.samples = frame->audio.sample_rate;
		adata.channels = frame->audio.channels;
		adata.bit_per_samples = frame->audio.bits_per_sample;
		adata.datalen = frame->frame_size;
		adata.data = (char*)frame->frame_data;

		//
		Instance::req_audio(&adata);

	}

private:
	static void __stdcall OnStream(IPMS_UUID_t* id /*preview.channel | replay.stream*/, int stream_num, IPMS_STREAM_t streams[], void* context)
	{
		Self_IPMS_Replay* i = reinterpret_cast<Self_IPMS_Replay*>(context);
		i->OnStream(stream_num, streams);

	}

	void OnStream(int stream_num, IPMS_STREAM_t streams[])
	{		
		// 
		if(_buf1)
		{
			delete[] _buf1;
			_buf1 = NULL;
		}

		// 
		if(_buf2)
		{
			delete[] _buf2;
			_buf2 = NULL;
		}

		// 获取有效的流描述
		std::list<IPMS_STREAM_t> audio_list, video_list;
		get(stream_num, streams, audio_list, video_list);
		
		// 计算IPMS通道描述细节， 不完整的话不上报码流描述
		if(!get(video_list, _resol, _rows, _cols, _grade))
		{
			return;
		}

		//
		new_id();
		
		// 分配描述所使用的内存，当前IPMS只有一个码流，但可能有多个块
		unsigned int len1 = audio_list.size() * sizeof(VS_AUDIO_STREAM_DESC);
		unsigned int len2 = sizeof(VS_VIDEO_STREAM_DESC) + video_list.size() * sizeof(VS_BLOCK_DESC);

		_buf1 = new char[len1];
		_buf2 = new char[len2];

		// 填充音频结构
		unsigned int audio_index = 0;
		VS_AUDIO_STREAM_DESC* p1 = reinterpret_cast<VS_AUDIO_STREAM_DESC*>(_buf1);
		for(std::list<IPMS_STREAM_t>::iterator it = audio_list.begin();
			it != audio_list.end();
			it++)
		{
			//
			p1[audio_index].id = new_id(it->stream_id);
			p1[audio_index].codec = CODEC_G711;
			p1[audio_index].samples = it->audio.sample_rate;
			p1[audio_index].channels = it->audio.channels;
			p1[audio_index].bit_per_samples = it->audio.bits_per_sample;
			p1[audio_index].grade = 0;

			//
			audio_index++;
		}
		
		// 填充视频结构
		VS_VIDEO_STREAM_DESC* p2 = reinterpret_cast<VS_VIDEO_STREAM_DESC*>(_buf2);
		p2->id = _video_stream_id;
		p2->isaudio = 0;
		p2->resol = _resol;
		p2->rows = _rows;
		p2->cols = _cols;
		p2->grade = _grade;
		
		for(std::list<IPMS_STREAM_t>::iterator it = video_list.begin();
			it != video_list.end();
			it++)
		{
			u_int8 row = 0;
			u_int8 col = 0;

			get(*it, row, col);

			VS_BLOCK_DESC* p = &p2->desc[row * _cols + col];

			//
			p->id = new_id(it->stream_id);

			//
			p->area.x = (u_int32)(_resol.w * it->video.region.x + 0.5);
			p->area.y = (u_int32)(_resol.h * it->video.region.y + 0.5);
			p->area.w = (u_int32)(_resol.w * it->video.region.w + 0.5);
			p->area.h = (u_int32)(_resol.h * it->video.region.h + 0.5);

			//
			p->resol.w = it->video.width;
			p->resol.h = it->video.height;

			//
			p->valid.x = (u_int32)(p->resol.w * it->video.clip.x + 0.5);
			p->valid.y = (u_int32)(p->resol.h * it->video.clip.y + 0.5);
			p->valid.w = (u_int32)(p->resol.w * it->video.clip.w + 0.5);
			p->valid.h = (u_int32)(p->resol.h * it->video.clip.h + 0.5);

		}

		Instance::req_desc(audio_list.size(), &p1, 1, &p2);
		
	}

	static void __stdcall OnFrame(IPMS_FRAME_t* frame, void* context)
	{
		Self_IPMS_Replay* i = reinterpret_cast<Self_IPMS_Replay*>(context);
		i->OnFrame(frame);
	}

	void OnFrame(IPMS_FRAME_t* frame)
	{
		if(frame->is_video != 0)		// 视频
		{
			videoproc(frame);
		}
		else						// 音频
		{
			audioproc(frame);
		}
		
	}

	static void __stdcall OnError(IPMS_UUID_t* channel, int error, const char* description, const char* addr, unsigned short port, void* context)
	{
		Self_IPMS_Replay* i = reinterpret_cast<Self_IPMS_Replay*>(context);
		i->OnError(error, description);
	}

	void OnError(int error, const char* description)
	{
		LOG(LEVEL_ERROR, "error = %d, description = %s", error, description);
		req_error(XVIDEO_PLUG_SDK_ERR);
	}

private:
	void threadloop()
	{
		char ip[64];
		host_ultoa(ipaddr(), sizeof(ip), ip);

		int e0;
		_user = IPMS_NET_Login(ip, port(), _defext.name, _defext.passwd, -1, &e0);
		if (!_user || e0 != IPMS_ERROR_SUCCESS)
		{
			req_error(XVIDEO_PLUG_LOGIN_ERR);
			return;
		}

		// 设置相关回调
		int e1 = IPMS_NET_SetStreamListener(_user, OnStream, this);
		int e2 = IPMS_NET_SetFrameCB(_user, OnFrame, this);
		int e3 = IPMS_NET_SetErrorCB(_user, OnError, this);

		if(e1 || e2 || e3)
		{
			IPMS_NET_Logout(_user);
			req_error(XVIDEO_PLUG_SETCB_ERR);
			return;
		}

		// 开始请求码流
		IPMS_REPLAY_PERIOD_t period;
		period.begin_time = _defext.begintime;
		period.end_time = _defext.endtime;
		int e4 = IPMS_NET_OpenStream2(_user, (IPMS_UUID_t*)&_defext.channel, IPMS_NET_TCP, NULL, IPMS_REPLAY, &period, (IPMS_UUID_t*)&_defext.stream);
		if(e4)
		{
			IPMS_NET_Logout(_user);
			req_error(XVIDEO_PLUG_OPEN_ERR);
			return;
		}

		// 等待退出指令
		while(!isexit())
		{
			fd_set fdset;
			struct timeval tv = {0, 100000};

			FD_ZERO(&fdset);
			wait_exit(&fdset);

			Network::Select(FD_SETSIZE, &fdset, NULL, NULL, &tv);
		}

		// 关闭码流
		IPMS_NET_CloseStream(_user);

		// 退出登录
		IPMS_NET_Logout(_user);
	}


private:
	IPMS_USER_t _user;

private:
	VS_RESOL _resol;
	u_int8 _rows;
	u_int8 _cols;
	u_int8 _grade;

	char* _buf1;
	char* _buf2;

private:
	DefExt _defext;

private:
	Uuid _video_stream_id;
	Uuid _video_stamp_id;
	Uuid _audio_stamp_id;

private:
	std::map<int, Uuid> _stream2id;
};

void* imp_ipms_replay_open(const VS_SIGNAL* signal, SigCB cb, void* context, const Uuid& item)
{
	Self_IPMS_Replay* i = new Self_IPMS_Replay(signal, cb, context, item);
	if(!i)
	{
		return NULL;
	}

	if(!i->newthread())
	{
		delete i;
		return NULL;
	}

	return i;
}

/* must be quick */
int   imp_ipms_replay_close(void* handle)
{
	Self_IPMS_Replay* i = reinterpret_cast<Self_IPMS_Replay*>(handle);
	i->exit();
	return 0l;
}

int   imp_ipms_replay_set_i_frame(void* handle)
{
	Self_IPMS_Replay* i = reinterpret_cast<Self_IPMS_Replay*>(handle);
	i->set_i_frame();
	return 0l;
}

int   imp_ipms_replay_error(void* handle)
{
	Self_IPMS_Replay* i = reinterpret_cast<Self_IPMS_Replay*>(handle);
	return i->geterror();
}



