#include "imp_ipms_play.h"
#include "IpmsNetSDK.h"
#include <x_log/msglog.h>
#include <error/xerror.h>
#include <list>
#include <map>
#include <math.h>
#include <vector>
#include "h264.h"

#include "optimizer.h"

class Self_IPMS_Play: public Instance
{
private:
#pragma pack(push)
#pragma pack(1)
	struct DefExt
	{
		char	proxy_for_xlan[32];
		u_int32 chan_for_xlan;
		char	name[32];
		char	passwd[32];
		Uuid	channel;
	};

#pragma pack(pop)
	
#define TEST_TIMEOUT		(200)
#define VIRTUAL_STAMP		(false)

public:
	Self_IPMS_Play(const VS_SIGNAL* signal, SigCB cb, void* context, const Uuid& item): Instance(signal, cb, context, item, TEST_TIMEOUT),
		_cNoData(0)
		, _buf1(NULL), _buf2(NULL)
		//, m_vDesc(NULL), m_aDesc(NULL)
	{
		pthread_mutex_init(&_stream_mutex, NULL);
		
		if(!getext(_defext))
		{
			memset(&_defext, 0, sizeof(_defext));
		}
		_curVideo_list.clear();
		_curAudio_list.clear();
		
		//
		sumObj = ::InterlockedIncrement((unsigned int*)&sumObj);
		curObj = sumObj;
		LOG(LEVEL_INFO, "ypeng@ ipms_play(%u),Construction success, object's tagNum=%d.\n", this, curObj);
	}

	~Self_IPMS_Play()
	{
		pthread_mutex_destroy(&_stream_mutex);

		if(_buf1)
		{
			delete[] _buf1;
		}

		if(_buf2)
		{
			delete[] _buf2;
		}
		
		//
		sumObj = ::InterlockedDecrement((unsigned int*)&sumObj);
		LOG(LEVEL_INFO, "ypeng@ ipms_play(%u),Deconstruction success, object's tagNum=%d, remAll=%d.\n", this, curObj, sumObj);
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

		_video_stream2id.clear();
		_audio_stream2id.clear();
	}

	bool new_id(std::list<IPMS_STREAM_t>& video_list, std::list<IPMS_STREAM_t>& audio_list)
	{
		//检查音视频信息是否有所改变，不改变则不产生新的uuid
		//bool newVID = false;
		//bool newAID = false;
		m_newVID = false;
		m_newAID = false;

		if(_curVideo_list.empty())
		{
			if(! video_list.empty())
			{
				m_newVID = true;
			}
		}
		else if(_curVideo_list.size() == video_list.size())
		{
			for(std::list<IPMS_STREAM_t>::iterator it1 = video_list.begin(), it2 = _curVideo_list.begin();
				video_list.end() != it1 && _curVideo_list.end() != it2;
				it1++, it2++)
			{
				if(0 != memcmp(&(*it1), &(*it2), sizeof(IPMS_STREAM_t)))
				{
					m_newVID = true;
					break;
				}
			}
		}
		else
		{
			m_newVID = true;
		}
		if(m_newVID)
		{
			_curVideo_list.clear();
			for(std::list<IPMS_STREAM_t>::iterator it = video_list.begin();
				video_list.end() != it;
				it++)
			{
				_curVideo_list.push_back(*it);
			}
		}
		else
		{
			LOG(LEVEL_INFO, "VIDEO, IPMS_STREAM_t is same as before! Won't make new uuid!\n");
		}

		if(_curAudio_list.empty())
		{
			if(! audio_list.empty())
			{
				m_newAID = true;
			}
		}
		else if(_curAudio_list.size() == audio_list.size())
		{
			for(std::list<IPMS_STREAM_t>::iterator it1 = audio_list.begin(), it2 = _curAudio_list.begin();
				audio_list.end() != it1 && _curAudio_list.end() != it2;
				it1++, it2++)
			{
				if(0 != memcmp(&(*it1), &(*it2), sizeof(IPMS_STREAM_t)))
				{
					m_newAID = true;
					break;
				}
			}
		}
		else
		{
			m_newAID = true;
		}
		if(m_newAID)
		{
			_curAudio_list.clear();
			for(std::list<IPMS_STREAM_t>::iterator it = audio_list.begin();
				audio_list.end() != it;
				it++)
			{
				_curAudio_list.push_back(*it);
			}
		}
		else
		{
			LOG(LEVEL_INFO, "AUDIO, IPMS_STREAM_t is same as before! Won't make new uuid!\n");
		}

		if(!m_newVID && !m_newAID)
		{
			LOG(LEVEL_INFO, "All IPMS_STREAM_t are same as before! Won't req_desc!\n");
			return false;
		}
		else
		{
			
			if(m_newVID)
			{
				_video_stream_id.Generate();
				_video_stamp_id.Generate();

				_video_stream2id.clear();
				LOG(LEVEL_INFO, "IPMS_video, streamID(%s), stampID(%s)", _video_stream_id.string().c_str(), _video_stamp_id.string().c_str());
			}
			if(m_newAID)
			{
				_audio_stream2id.clear();
				_audio_stamp_id.Generate();
				LOG(LEVEL_INFO, "IPMS_audio, stampID(%s).", _audio_stamp_id.string().c_str());
			}
		}

		return true;
	}

	Uuid new_id(bool audio, int stream)
	{
		Uuid id;
		id.Generate();

		// 
		if (audio)
		{
			_audio_stream2id.insert(std::make_pair(stream, id));
			LOG(LEVEL_INFO, "IPMS_audio, stream(%d), ID(%s)", stream, id.string().c_str());
		}
		else
		{
			_video_stream2id.insert(std::make_pair(stream, id));
			LOG(LEVEL_INFO, "IPMS_video, stream(%d), ID(%s)", stream, id.string().c_str());
		}

		// 
		return id;
	}

	bool get_id(int stream, Uuid& id)
	{
		std::map<int, Uuid>::iterator it = _video_stream2id.find(stream);
		if(it != _video_stream2id.end())
		{
			id = it->second;
			return true;
		}

		it = _audio_stream2id.find(stream);
		if(it != _audio_stream2id.end())
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
		col = (u_int8)(desc.video.region.x / desc.video.region.w + 0.5);
		row = (u_int8)(desc.video.region.y / desc.video.region.h + 0.5);

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

		
#if 1 /*本段代码用来解决IPMS没有通知分辨率变化的BUG    add by chen @ 2015-5-21*/
		if ((frame->video.frame_type == IPMS_FRAME_IFRAME) && (frame->media_fmt == IPMS_MEDIA_FMT_H264))
		{
			uint16_t w = 0;
			uint16_t h = 0;
			if (H264Parse::GetResolution(frame->frame_data, frame->frame_size, &w, &h))
			{
				bool changed = false;
				pthread_mutex_lock(&_stream_mutex);
				for (int i = 0; i < _streams.size(); i++)
				{
					if (_streams[i].stream_id == frame->stream_id)
					{
						if ((w != _streams[i].video.width) || (h != _streams[i].video.height))
						{
							_streams[i].video.width  = w;
							_streams[i].video.height = h;
							changed = true;
						}
						break;
					}
				}
				pthread_mutex_unlock(&_stream_mutex);

				//printf("%dx%d == %dx%d\n", w, h, frame->video.width, frame->video.height);
				if (changed && _streams.size())
				{
					printf("#$######################################################\n");

					pthread_mutex_lock(&_stream_mutex);
					std::vector<IPMS_STREAM_t> streams = _streams;
					pthread_mutex_unlock(&_stream_mutex);

					OnStream(streams.size(), &streams[0]);
					return;
				}
			}
		}
#endif

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
		vdata.useVirtualStamp = VIRTUAL_STAMP;

		vdata.datalen = frame->frame_size;
		vdata.data = (char*)frame->frame_data;

		//
		vFrameNum++;
		if(vFrameNum % 200 == 1)
		{
			LOG(LEVEL_INFO, "ypeng@ req_video, vFrameNum=%u, frameType=%u, timestamp=%u, size=%d.\n",
				vFrameNum, vdata.frametype, vdata.timestamp, vdata.datalen);
			//LOG(LEVEL_INFO, "vHeader's INFO: sync=%x, len=%u, frame_num=%u, frame_size=%u, stream_id=%u, ");
		}
#ifdef OPT_PLUGIN_BUF
		Instance::req_video_toBuf(&vdata);
#else
		Instance::req_video(&vdata);
#endif//OPT_PLUGIN_BUF


	}

	void audioproc(IPMS_FRAME_t* frame)
	{
		//
		Uuid id;
		if(!get_id(frame->stream_id, id))
		{
			LOG(LEVEL_INFO, "[CHEN] #### audio id not found, %d\n", frame->stream_id);
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
		//static FILE* fp = fopen("D:\\test.aac", "w+b");
		//fwrite(adata.data, 1, adata.datalen, fp);
		//fflush(fp);
		aFrameNum++;
		if(aFrameNum % 200 == 1)
		{
			LOG(LEVEL_INFO, "ypeng@ req_audio, aFrameNum=%u, timestamp=%u, size=%d.\n",
				aFrameNum, adata.timestamp, adata.datalen);
		}
#ifdef OPT_PLUGIN_BUF
		Instance::req_audio_toBuf(&adata);
#else
		Instance::req_audio(&adata);
#endif//OPT_PLUGIN_BUF

	}

private:
	static void __stdcall OnStream(IPMS_UUID_t* id /*preview.channel | replay.stream*/, int stream_num, IPMS_STREAM_t streams[], void* context)
	{
		Self_IPMS_Play* i = reinterpret_cast<Self_IPMS_Play*>(context);
		i->OnStream(stream_num, streams);

	}

	void OnStream(int stream_num, IPMS_STREAM_t streams[])
	{
#if 1
		{
			pthread_mutex_lock(&_stream_mutex);
			_streams.clear();
			for (int i = 0; i < stream_num; i++)
			{
				_streams.push_back(streams[i]);
			}
			pthread_mutex_unlock(&_stream_mutex);
		}
#endif

		// 获取有效的流描述
		std::list<IPMS_STREAM_t> audio_list, video_list;
		get(stream_num, streams, audio_list, video_list);
		
		// 计算IPMS通道描述细节， 不完整的话不上报码流描述
		if(!get(video_list, _resol, _rows, _cols, _grade))
		{
			return;
		}

		//判断音视频描述是否发生变化，无变化不重新生成UUID
		if(! new_id(video_list, audio_list))
		{
			LOG(LEVEL_ERROR, "new_id failed!!");
			return;
		}
		
		// 分配描述所使用的内存，当前IPMS只有一个码流，但可能有多个块
		unsigned int len1 = audio_list.size() * sizeof(VS_AUDIO_STREAM_DESC);
		unsigned int len2 = sizeof(VS_VIDEO_STREAM_DESC) + video_list.size() * sizeof(VS_BLOCK_DESC);
		
		VS_AUDIO_STREAM_DESC* p1 = NULL;
		VS_VIDEO_STREAM_DESC* p2 = NULL;

		if(m_newAID)
		{
			if(_buf1)
			{
				delete[] _buf1;
				_buf1 = NULL;
			}
			_buf1 = new char[len1];

			// 填充音频结构
			unsigned int audio_index = 0;
			p1 = reinterpret_cast<VS_AUDIO_STREAM_DESC*>(_buf1);
			for(std::list<IPMS_STREAM_t>::iterator it = audio_list.begin();it != audio_list.end();it++)
			{
				//
				p1[audio_index].id = new_id(true, it->stream_id);
				p1[audio_index].codec = CODEC_G711;
				p1[audio_index].samples = it->audio.sample_rate;
				p1[audio_index].channels = it->audio.channels;
				p1[audio_index].bit_per_samples = it->audio.bits_per_sample;
				p1[audio_index].grade = 0;

				//
				audio_index++;
			}
		}
		else
		{
			p1 = reinterpret_cast<VS_AUDIO_STREAM_DESC*>(_buf1);
		}

		if(m_newVID)
		{
			if(_buf2)
			{
				delete[] _buf2;
				_buf2 = NULL;
			}
			_buf2 = new char[len2];

			//LOG(LEVEL_INFO, "videoINFO, stream_id:%s, resol:%u*%u", _video_stream_id.string().c_str(), _resol.w, _resol.h);
			// 填充视频结构
			p2 = reinterpret_cast<VS_VIDEO_STREAM_DESC*>(_buf2);
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
				p->id = new_id(false, it->stream_id);

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
		}
		else
		{
			p2 = reinterpret_cast<VS_VIDEO_STREAM_DESC*>(_buf2);
		}


		Instance::req_desc(audio_list.size(), &p1, 1, &p2);
		
	}

	static void __stdcall OnFrame(IPMS_FRAME_t* frame, void* context)
	{
		Self_IPMS_Play* i = reinterpret_cast<Self_IPMS_Play*>(context);
		i->OnFrame(frame);
	}

	void OnFrame(IPMS_FRAME_t* frame)
	{
		_cNoData = 0;

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
		Self_IPMS_Play* i = reinterpret_cast<Self_IPMS_Play*>(context);
		i->OnError(error, description);
	}

	void OnError(int error, const char* description)
	{
		LOG(LEVEL_ERROR, "error = %d, description = %s", error, description);
		req_error(error);
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
			LOG(LEVEL_ERROR, "imp_ipms_play IPMS_NET_Login failed! ip=%s, port=%d, user=%s, passwd=%s.\n", ip, port(), _defext.name, _defext.passwd);
			req_error(XVIDEO_PLUG_LOGIN_ERR);
			return;
		}

		// 设置相关回调
		int e1 = IPMS_NET_SetStreamListener(_user, OnStream, this);
		int e2 = IPMS_NET_SetFrameCB(_user, OnFrame, this);
		int e3 = IPMS_NET_SetErrorCB(_user, OnError, this);

		if(e1 != IPMS_ERROR_SUCCESS || e2 != IPMS_ERROR_SUCCESS || e3 != IPMS_ERROR_SUCCESS)
		{
			LOG(LEVEL_ERROR, "imp_ipms_play IPMS_NET_Set failed! ");
			IPMS_NET_Logout(_user);
			req_error(XVIDEO_PLUG_SETCB_ERR);
			return;
		}

		// 开始请求码流
		int e4 = IPMS_NET_OpenStream(_user, (IPMS_UUID_t*)&_defext.channel, IPMS_NET_TCP, IPMS_PREVIEW);
		if(e4 != IPMS_ERROR_SUCCESS)
		{
			LOG(LEVEL_ERROR, "imp_ipms_play IPMS_NET_OpenStream failed! error=%d, uuid=%s.\n", e4, _defext.channel.string().c_str());
			IPMS_NET_Logout(_user);
			req_error(XVIDEO_PLUG_OPEN_ERR);
			return;
		}

		// 等待退出指令
		while(!isexit())
		{
			fd_set fdset;
			struct timeval tv = {0, 10000};

			FD_ZERO(&fdset);
			wait_exit(&fdset);

			if(Network::Select(FD_SETSIZE, &fdset, NULL, NULL, &tv) == 0)
			{
				_cNoData++;
				if(_cNoData >= 800)
				{
					_cNoData = 0;
					req_error(XVIDEO_PLUG_NODATA_ERR);
				}

#ifdef OPT_PLUGIN_BUF
				Instance::CheckTimeSend(0);
#endif//OPT_PLUGIN_BUF
			}
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
	u_int8   _rows;
	u_int8   _cols;
	u_int8   _grade;
	int     _cNoData;

	char*    _buf1;
	char*    _buf2;

private:
	DefExt _defext;

private:
	Uuid _video_stream_id;
	Uuid _video_stamp_id;
	Uuid _audio_stamp_id;

private:
	std::map<int, Uuid> _audio_stream2id;
	std::map<int, Uuid> _video_stream2id;

	std::list<IPMS_STREAM_t> _curAudio_list;
	std::list<IPMS_STREAM_t> _curVideo_list;

	//是否需要新生成ID
	bool m_newVID;
	bool m_newAID;
	//VS_VIDEO_STREAM_DESC* m_vDesc;
	//VS_AUDIO_STREAM_DESC* m_aDesc;

	std::vector<IPMS_STREAM_t>	_streams;
	pthread_mutex_t				_stream_mutex;
	
private://just for debugMsg
	static int	sumObj;
	int			curObj;
	int			vFrameNum;
	int			aFrameNum;
};

int	Self_IPMS_Play::sumObj = 0;

void* imp_ipms_play_open(const VS_SIGNAL* signal, SigCB cb, void* context, const Uuid& item)
{
	Self_IPMS_Play* i = new Self_IPMS_Play(signal, cb, context, item);
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
int   imp_ipms_play_close(void* handle)
{
	Self_IPMS_Play* i = reinterpret_cast<Self_IPMS_Play*>(handle);
	i->exit();
	return 0l;
}

int   imp_ipms_play_set_i_frame(void* handle)
{
	Self_IPMS_Play* i = reinterpret_cast<Self_IPMS_Play*>(handle);
	i->set_i_frame();
	return 0l;
}

int   imp_ipms_play_error(void* handle)
{
	Self_IPMS_Play* i = reinterpret_cast<Self_IPMS_Play*>(handle);
	return i->geterror();
}

