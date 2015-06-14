#pragma once

#include "Plugin.h"
#include <vector>
#include <x_log/msglog.h>
#include <error/xerror.h>
using namespace device;

void* imp_vi04_open(const VS_SIGNAL* signal, SigCB cb, void* context, const Uuid& item);
int   imp_vi04_close(void* handle);
int   imp_vi04_set_i_frame(void* handle);
int   imp_vi04_error(void* handle);


//#define A_CODEC
#ifdef A_CODEC
extern "C"
{
#include "libavcodec\avcodec.h"
#include "libavformat\avformat.h"
#include "libswscale\swscale.h"
};

#pragma comment (lib,"avcodec.lib")
#pragma comment (lib,"avformat.lib")
#pragma comment (lib,"avutil.lib")
#pragma comment (lib,"avdevice.lib")
#pragma comment (lib,"avfilter.lib")
#pragma comment (lib,"swscale.lib")

#endif

class Self_VI04: public Instance
{
private:
	enum 
	{
		CMD_PORT = 5001,
		REQ_PORT = 5002,
	};

	enum 
	{
		MAX_BUF = 10 * 1024 * 1024,
	};

public:
	Self_VI04(const VS_SIGNAL* signal, SigCB cb, void* context, const Uuid& item): Instance(signal, cb, context, item),
		_req_fd(-1),
		_cmd_fd(-1),
		_audio_fd(-1),
		_video_fd(-1),
		_is_first_frame(true),
		_get_audio_suc(false),
		_audiodesc(reinterpret_cast<VS_AUDIO_STREAM_DESC*>(buf1)),
		_videodesc(reinterpret_cast<VS_VIDEO_STREAM_DESC*>(buf2))
		, _missV(0), _preV(0)
	{
		_preV = GetTickCount();
		if(!getchannel(_channel))
		{
			_channel = 0;
		}
		sumObj++;
		curObj = sumObj;
		LOG(LEVEL_INFO, "ypeng@ vi04(%u),Construction success, _channel=%d, object's tagNum=%d\n", this, _channel, curObj);
	}

	~Self_VI04()
	{
		sumObj--;
		LOG(LEVEL_INFO, "ypeng@ vi04(%u),Deconstruction success, object's tagNum=%d, remAll=%d.\n", this, curObj, sumObj);
	}

	void new_id()
	{
		_video_stream_id.Generate();
		_video_block_id.Generate();
		_audio_stream_id.Generate();
		_video_stamp_id.Generate();
		_audio_stamp_id.Generate();

	}

private:
	bool getchannel(int& channel);

private:
	static void* setparam(void* arg)
	{
		Self_VI04* pthis = reinterpret_cast<Self_VI04*>(arg);
		pthis->setparam();

		return NULL;
	}

	void setparam()
	{
		while(!isexit())
		{
			if(wait_i_frame())
			{
				if(!make_i_frame(_req_fd))
				{
					req_error(XVIDEO_PLUG_MAKEIFRAME_ERR);
					break;
				}
			}
		}
	}

private:
	void threadloop()
	{
		if(!connect())
		{
			req_error(XVIDEO_PLUG_CONNECT_ERR);
			return;
		}

		if(pthread_create(&_thread, NULL, setparam, this) != 0)
		{
			disconnect();
			req_error(XVIDEO_PLUG_NEWTHREAD_ERR);
			return;
		}
		
		while(!isexit())
		{
			fd_set fdset;
			struct timeval tv = {5, 0};

			FD_ZERO(&fdset);
			wait_exit(&fdset);
			FD_SET(_video_fd, &fdset);
			FD_SET(_audio_fd, &fdset);

			int nfds = Network::Select (FD_SETSIZE, &fdset, NULL, NULL, &tv);
			if( nfds > 0)
			{
				if(FD_ISSET(_video_fd, &fdset)) 
				{
					if(!videoproc())
					{
						req_error(XVIDEO_PLUG_VIDEOPROC_ERR);
						break;
					}
					else
					{
						_missV = 0;
						_preV = GetTickCount();
					}
				}
				
				if(FD_ISSET(_audio_fd, &fdset)) 
				{
					if(!audioproc())
					{
						req_error(XVIDEO_PLUG_AUDIOPROC_ERR);
						break;
					}
					else
					{
						_missV = GetTickCount() - _preV;
						if(_missV > 5*1000)
						{//5s无视频，输出无信号图像
							_missV = 0;
							req_error(XVIDEO_PLUG_NODATA_ERR);
						}
					}
				}
			}
			else if(nfds == 0)
			{
				req_error(XVIDEO_PLUG_NODATA_ERR);
			}

		}
FAILURE:
		//
		pthread_join(_thread, NULL);

		//
		disconnect();
	}


private:
	bool local(int sock, u_int32& ipaddr)
	{ 
		struct sockaddr_in inaddr; 
		socklen_t len = sizeof(inaddr);

		if(getsockname(sock, (struct sockaddr*)&inaddr, &len) == 0)
		{ 
			ipaddr = ntohl(inaddr.sin_addr.S_un.S_addr);
			return true;
		}
		else
		{
			return false;
		}
	}

	bool local(int sock, int size, char* ipaddr)
	{ 
		struct sockaddr_in inaddr; 
		socklen_t len = sizeof(inaddr);

		if(getsockname(sock, (struct sockaddr*)&inaddr, &len) == 0)
		{ 
			host_ultoa(ntohl(inaddr.sin_addr.S_un.S_addr), size, ipaddr); 
			return true;
		}
		else
		{
			return false;
		}
	}

private:
	bool connect();

	bool disconnect();

private:
	bool get_video_port(int sockfd, u_int16& port);

	bool get_audio_port(int sockfd, u_int16& port);

	void disconnect_video(int sockfd);

	void disconnect_audio(int sockfd);

	bool make_i_frame(int sockfd);

private:
	void req_desc(const VS_RESOL& resol, const VS_AREA& valid);

	void req_video(const VS_RESOL& resol, const VS_AREA& valid, bool isiframe, u_int64 timestamp, u_int32 datalen, unsigned char* data);

	void req_audio(u_int32 timestamp, u_int32 datalen, unsigned char* data);

private:
	bool videoproc();

	bool audioproc();


private:
	u_int8 grade(u_int32 w, u_int32 h)
	{
		if(w <= 720)
		{
			return 6;
		}
		else
		{
			return 100;
		}
	}

private:
#ifdef A_CODEC
	bool openAudioEncoder(u_int32 sample_rate, u_int8 sample_bit, u_int8 sample_chan);

	int encodeAudio(u_int32 timestamp);

	bool closeAudioEncoder();
#endif


private:
	int _req_fd;
	int _cmd_fd;
	int _audio_fd;
	int _video_fd;
	bool _get_audio_suc;

private:
	std::vector<u_int8> _video_buf;
	std::vector<u_int8> _audio_buf;

private:
	bool _is_first_frame;
	VS_RESOL _resol;

	char buf1[sizeof(VS_AUDIO_STREAM_DESC)];
	VS_AUDIO_STREAM_DESC* _audiodesc;
	char buf2[sizeof(VS_VIDEO_STREAM_DESC) + sizeof(VS_BLOCK_DESC)];
	VS_VIDEO_STREAM_DESC* _videodesc;

private:
	pthread_t _thread;

private:
	int _channel;
	u_int32 _missV;
	//bool _hasV;
	u_int32 _preV;

private:
	Uuid _video_stream_id;
	Uuid _video_block_id;
	Uuid _audio_stream_id;
	Uuid _video_stamp_id;
	Uuid _audio_stamp_id;
	
private://just for debugMsg
	static int	sumObj;
	int			curObj;
	u_int32		vFrameNum;
	u_int32		aFrameNum;
	u_int32		preA_Stamp;
	u_int32		preV_Stamp;
	u_int64		preSysTime;

private://for audio encoder
#ifdef A_CODEC
	int16_t *samples;
	uint8_t *audio_outbuf;
	int audio_outbuf_size;
	int audio_input_frame_size;
	AVOutputFormat *fmt;
	AVFormatContext *avFmtCnt;
	AVStream * audio_st;
	AVCodecContext* avCdCnt;
#endif

};