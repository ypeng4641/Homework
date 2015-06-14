#pragma once

#include "Plugin.h"
#include "vtron_hipc.h"
#include <error/xerror.h>
#include "myRTSPClient.h"
#include "ResizeMem.h"
#include "utils/time.h"

//#pragma comment(lib, "Ws2_32.lib")
//#pragma comment(lib, "pthreadVC2.lib")
//#pragma comment(lib, "ipmscodec.lib")
//#pragma comment(lib, "ipms_log.lib")
#pragma comment(lib, "liveMedia.lib")
#pragma comment(lib, "BasicUsageEnvironment.lib")
#pragma comment(lib, "groupsock.lib")
#pragma comment(lib, "UsageEnvironment.lib")

using namespace device;

#define MAX_URL_LEN				256

#define MEDIA_TYPE_VIDEO		1
#define MEDIA_TYPE_AUDIO		2

#define VIDEO_CODER_TYPE_H264	1
#define VIDEO_CODER_TYPE_MPEG2	2
#define VIDEO_CODER_TYPE_MPEG4	3
#define VIDEO_CODER_TYPE_MJPEG	4
#define VIDEO_CODER_TYPE_H263	5

#define TIME_RECORD_CAPACITY	30

#define MAX_CHANNEL_NUM		256
#define MAX_PACKET_LEN		24*1024

extern RtspThreadContext g_context[MAX_CHANNEL_NUM];
extern pthread_mutex_t g_mutex;

#include <string>

#pragma pack(1)
typedef struct _PARAMS_DEF
{
	char			ProxyServer[32];
	int				channel;
	char			url[MAX_URL_LEN];
	char			username[32];
	char			password[32];
	char			transportProtocol;
	char			timestampMechanism;
}RTSP_PARAMS_DEF;

typedef struct _frame_info
{
	int				mediaType;
	int				codeType;
	int				frameNo;
	int				keyFrame;
	unsigned int	pts;
}FrameInfo;

typedef struct _packet_header
{
	unsigned int	totalLength;
	unsigned int 	currentLength;
	unsigned int 	sequenceNum;
	unsigned int 	frameFlag;
	FrameInfo		frameInfo;
}PacketHeader;

#pragma pack()


enum TimeStampMechanism
{
	UseNoStamp		= 0,
	UseRtpTimeStamp	= 1,
	UseFrameRate	= 2
};


struct frameDetail
{
	struct timeval 		stamp;
	char*				frameBuffer;
	unsigned int		frameBufferDatasize;
	bool				syncUseRtcp;
};

class LockHelper
{
public:
    LockHelper(pthread_mutex_t* lock):_lock(lock)
    {
		while(pthread_mutex_lock(_lock) != 0 && errno == EINTR);
    }
    
    ~LockHelper()
    {
		while(pthread_mutex_unlock(_lock) != 0 && errno == EINTR);
    }
private:
    pthread_mutex_t* _lock;
};

class Self_RTSP_live555: public Instance
{
public:
	Self_RTSP_live555(const VS_SIGNAL* signal, SigCB cb, void* context, const Uuid& item): Instance(signal, cb, context, item)
		, _is_first_frame(true)
		, _audiodesc(reinterpret_cast<VS_AUDIO_STREAM_DESC*>(buf1))
		, _videodesc(reinterpret_cast<VS_VIDEO_STREAM_DESC*>(buf2))
		, _frameNO(0)
		, _startStamp(0), _lastStamp(0), _syncUseRTCP(false), _timeRecord(NULL)
		, _working(false), _client(NULL), _frame_num(0)
		, _is_audio_valid(false)
		, _is_video_valid(false)
		, _last_time(::GetTickCount())
	{
		memset(&_lastFrameDetail, 0, sizeof(_lastFrameDetail));
		_lastFrameDetail.frameBuffer = new char[4*1024*1024];
		
		sumObj++;
		curObj = sumObj;
		LOG(LEVEL_INFO, "ypeng@ rtsp(%u) Construction success, object's tagNum=%d.\n", this, curObj);

		_audio_stamp.tv_sec = 0;
		_audio_stamp.tv_usec = 0;
	}

	~Self_RTSP_live555(void)
	{
		if(_lastFrameDetail.frameBuffer)
		{
			delete [] _lastFrameDetail.frameBuffer;
		}
		if(_timeRecord)
		{
			delete [] _timeRecord;
		}
		
		sumObj--;
		LOG(LEVEL_INFO, "ypeng@ rtsp(%u) Deconstruction success, object's tagNum=%d, remAll=%d.\n", this, curObj, sumObj);
	}
	
public:
	RtspParam			GetRtspParam()
	{
		return _rtspParam;
	}
	int					GetChannel()
	{
		return _channel;
	}

private:
	static void* setparam(void* arg)
	{
		Self_RTSP_live555* pthis = reinterpret_cast<Self_RTSP_live555*>(arg);
		pthis->setparam();

		return NULL;
	}
	
	void setparam()
	{
	}

private:
	void threadloop()
	{
		if(!connect())
		{
			req_error(XVIDEO_PLUG_CONNECT_ERR);
			return;
		}

		if(!play())
		{
			disconnect();
			req_error(XVIDEO_PLUG_OPEN_ERR);
			return;
		}

		while(! isexit())
		{
			Sleep(10);
		}

		stop();
		disconnect();
	}

private:
	bool connect(void);

	bool disconnect(void);

	bool play(void);

	bool stop(void);

	void req_desc(void);

	void req_video(const VS_RESOL& resol, const VS_AREA& valid, bool isiframe, u_int32 timestamp, u_int32 datalen, unsigned char* data);

	void req_audio(u_int32 timestamp, u_int32 datalen, unsigned char* data);
	
private:
	static int g_opennum;
	static int g_count;
	
private:
	static void*		RtspThread(void* param);
	static void			StreamCallback(int channel, int dataSize, char* data, RtspFrameInfo info);
	void                doStreamCallback(int channel, int dataSize, char* data, RtspFrameInfo info);
	unsigned int        GetPTS(struct timeval stamp, bool syncUseRTCP);
	unsigned int		GetFrameType(unsigned char* data, int size, unsigned int codec);

	unsigned int		_frameNO;
	//char				_packetData[26*1024];
	//unsigned int		_headerLen;
	//char*				_headerData;

	struct frameDetail	_lastFrameDetail;
	
	
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

	void new_id()
	{
		_video_stream_id.Generate();
		_video_block_id.Generate();
		_video_stamp_id.Generate();
		_audio_stamp_id.Generate();
		_audio_stream_id.Generate();
	}

//RTSP
private:
	int AllocChannel()
	{
		int channel = -1;

		LockHelper lock(&g_mutex);

		for(int i = 0; i < MAX_CHANNEL_NUM; i++)
		{
			if(!g_context[i].used)
			{
				g_context[i].used = true; 
				pthread_mutex_init(&g_context[i].mutex, NULL);
				channel = i;
				break;
			}
		}

		return channel;
	}

	void FreeChannel(int channel)
	{
		LockHelper lock(&g_mutex);

		if(channel >= 0 && channel < MAX_CHANNEL_NUM)
		{
			g_context[channel].used 		= false;
			g_context[channel].shutdown		= false;
			g_context[channel].pThis		= NULL;
			g_context[channel].eventLoopVar	= 0;
			pthread_mutex_destroy(&g_context[channel].mutex);
		}	
	}
	

	int	GetCodec(char* mediaName, char* codecName)
	{
		if (strcmp(mediaName, "audio") == 0 && strcmp(codecName, "MPEG4-GENERIC"))
		{
			return CODEC_AAC;
		}

		if (strcmp(mediaName, "audio") == 0 && strcmp(codecName, "L16"))
		{
			return CODEC_PCM;
		}

		int		codec 	= -1;

		if(strncmp(codecName, "H264", 4) == 0)
		{
			codec		= CODEC_H264;
		}
		else if(strncmp(codecName, "MP4V-ES", 7) == 0)
		{
			codec		= CODEC_MPEG4;
		}
		else if(strncmp(codecName, "JPEG", 4) == 0)
		{
		}
		else if(strncmp(codecName, "H263", 4) == 0)
		{
		}
		else
		{
		}

		return codec;
	}

	static void* KeepAliveThread(void* param);
	
private:
	bool _is_first_frame;
	VS_RESOL _resol;

	bool	_is_audio_valid;
	bool	_is_video_valid;
	std::string		_audio_frame;
	struct timeval	_audio_stamp;

	char buf1[sizeof(VS_AUDIO_STREAM_DESC)];
	VS_AUDIO_STREAM_DESC* _audiodesc;

	char buf2[sizeof(VS_VIDEO_STREAM_DESC) + sizeof(VS_BLOCK_DESC)];
	VS_VIDEO_STREAM_DESC* _videodesc;

	Uuid _video_stream_id;
	Uuid _video_block_id;
	Uuid _video_stamp_id;

	Uuid _audio_stream_id;
	Uuid _audio_stamp_id;

//RTSP
private:
	RtspParam			_rtspParam;
	pthread_t			_tID;
	int					_channel;
 	TimeStampMechanism	_timestampMechanism;
	
	ResizeMem			_header;
	ResizeMem			_frame;

	// variables for calc stamp by rtp stamp
	bool				_syncUseRTCP;
	struct timeval		_startTime;
	long long			_startStamp;
	long long	 		_lastStamp;

	// variables for calc stamp by frame rate
	struct timeval*		_timeRecord;
	int					_timeRecordSize;
	long long			_curStamp;  // us

	bool				_working;

	RTSPClient*			_client;

	int					_frame_num;

	//no data controler
	pthread_mutex_t		_dataMutex;
	u_int32_t			_missVideo;

	//debug
	static int			sumObj;
	int					curObj;
	//read h264 resol
	//ReadResolution		_rr264;

	long long			_last_time;

};//Self_RTSP_live555


void* imp_rtsplive555_open(const VS_SIGNAL* signal, SigCB cb, void* context, const Uuid& item);
int   imp_rtsplive555_close(void* handle);
int   imp_rtsplive555_set_i_frame(void* handle);
int   imp_rtsplive555_error(void* handle);