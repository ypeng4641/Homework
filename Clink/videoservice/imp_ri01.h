#pragma once

#include "Plugin.h"
#include <vector>
#include <error/xerror.h>
using namespace device;

//#define A_READFILE
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

void* imp_ri01_open(const VS_SIGNAL* signal, SigCB cb, void* context, const Uuid& item);
int   imp_ri01_close(void* handle);
int   imp_ri01_set_i_frame(void* handle);
int   imp_ri01_error(void* handle);


class AAC_FileTool
{
public:
	AAC_FileTool(const char* filePath): fp(NULL), bHasFile(false), headI(0)
	{
		if(NULL != filePath)
		{
			fp = fopen(filePath, "rb");
			if(NULL != fp)
			{
				bHasFile = true;
			}
			if(!findHead(0, headI))
			{
				bHasFile = false;
			}
		}
	}
	virtual ~AAC_FileTool(void)
	{
	}

	bool getNext(char** data, int& len)
	{
		if(!bHasFile)
		{
			return false;
		}

		int nxtHeadI = 0;
		if(! findHead(headI+1, nxtHeadI))
		{
			//seekI = headI;
			headI = nxtHeadI;
			return false;
		}

		len = nxtHeadI - headI;
		fseek(fp, headI, SEEK_SET);
		if(len != fread(_data, sizeof(char), len, fp))
		{
			*data = NULL;
			len = 0;
			return false;
		}

		*data = _data;
		headI = nxtHeadI;
		return true;

	}

private:
	bool findHead(int curI, int &nxtHeadI)
	{
		if(! bHasFile)
		{
			return false;
		}

		char buf;
		fseek(fp, curI, SEEK_SET);
		while(true)
		{
			nxtHeadI = ftell(fp);
			if(1 != fread(&buf, sizeof(char), 1, fp))
			{
				if(0 != feof(fp))
				{
					LOG(LEVEL_WARNING, "ypeng@ File end! curI=%d.\n", curI);
					nxtHeadI = 0;
					fseek(fp, 0, SEEK_SET);
					clearerr(fp);
					return false;
				}
			}
			if('\xFF' == buf)
			{
				if(1 != fread(&buf, sizeof(char), 1, fp))
				{
					if(0 != feof(fp))
					{
						LOG(LEVEL_WARNING, "ypeng@ File end! curI=%d.\n", curI);
						nxtHeadI = 0;
						fseek(fp, 0, SEEK_SET);
						clearerr(fp);
						return false;
					}
				}
				if('\xF9' == buf)
				//if('\xF1' == buf)
				{
					break;
				}
				else
				{
					continue;
				}
			}
			else
			{
				continue;
			}
		}
		return true;
	}

private:
	FILE*	fp;
	bool	bHasFile;
	int		headI;
	char	_data[1024];

};//AAC_FileTool

class Self_RI01: public Instance
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
	Self_RI01(const VS_SIGNAL* signal, SigCB cb, void* context, const Uuid& item): Instance(signal, cb, context, item),
		_req_fd(-1),
		_cmd_fd(-1),
		_audio_fd(-1),
		_video_fd(-1),
		_get_audio_suc(false),
		_is_first_frame(true),
		_audiodesc(reinterpret_cast<VS_AUDIO_STREAM_DESC*>(buf1)),
		_videodesc(reinterpret_cast<VS_VIDEO_STREAM_DESC*>(buf2))
		, curObj(0)
		, vFrameNum(0)
		, aFrameNum(0)
		, preA_Stamp(0)
		, preV_Stamp(0)
		, preSysTime(0)
		, _missV(0), _preV(0)
	{
		_preV = GetTickCount();
	//	if(!getchannel(_channel))	//ri01盒子的DRGB与ARGB通道选择由另外的工具决定，vwas不再传输该参数
		{
			_channel = 0;
		}

		_bRes = false;

#ifdef A_READFILE
		_aacHelper = new AAC_FileTool("D:/tdjm.aac");//("D:/write0.aac");
#endif
		
		//
		sumObj = ::InterlockedIncrement((unsigned int*)&sumObj);
		curObj = sumObj;
		LOG(LEVEL_INFO, "ypeng@ ri01(%u),Construction success, object's tagNum=%d.\n", this, curObj);
	}

	~Self_RI01()
	{
#ifdef A_READFILE
		delete _aacHelper;
#endif
		//
		sumObj = ::InterlockedDecrement((unsigned int*)&sumObj);
		LOG(LEVEL_INFO, "ypeng@ ri01(%u),Deconstruction success, object's tagNum=%d, remAll=%d.\n", this, curObj, sumObj);
	}

private:
	bool getchannel(int& channel);

	void new_id()
	{
		_video_stream_id.Generate();
		_video_block_id.Generate();
		_audio_stream_id.Generate();
		_video_stamp_id.Generate();
		_audio_stamp_id.Generate();
	}

private:
	static void* setparam(void* arg)
	{
		Self_RI01* pthis = reinterpret_cast<Self_RI01*>(arg);
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
			if(_get_audio_suc)
			{
				FD_SET(_audio_fd, &fdset);
			}

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

				if(_get_audio_suc && FD_ISSET(_audio_fd, &fdset)) 
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
	bool select(int sockfd, bool isdrgb);

	bool get_video_port(int sockfd, u_int16& port);

	bool get_audio_port(int sockfd, u_int16& port);

	void disconnect_video(int sockfd);

	void disconnect_audio(int sockfd);

	bool make_i_frame(int sockfd);

private:
	void req_desc(const VS_RESOL& resol, const VS_AREA& valid);

	void req_video(const VS_RESOL& resol, const VS_AREA& valid, bool isiframe, u_int32 timestamp, u_int32 datalen, unsigned char* data);

	void req_audio(u_int32 timestamp, u_int32 datalen, unsigned char* data);

private:
	bool videoproc();

	bool audioproc();
	
private:
#ifdef A_CODEC
	bool openAudioEncoder(u_int32 sample_rate, u_int8 sample_bit, u_int8 sample_chan);

	int encodeAudio(u_int32 timestamp);

	bool closeAudioEncoder();
#endif

private:
	u_int8 grade(u_int32 w, u_int32 h)
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
	int _req_fd;
	int _cmd_fd;
	int _audio_fd;
	int _video_fd;
	bool _get_audio_suc;
	u_int32 _missV;
	//bool _hasV;
	u_int32 _preV;

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
	int  _channel;
	bool _bRes;
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

private://read aacFile to test
#ifdef A_READFILE
	AAC_FileTool *_aacHelper;
#endif

};

