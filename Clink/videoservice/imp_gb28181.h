#pragma once

#include "Plugin.h"
#include <error/xerror.h>

using namespace device;


#define  CSIP_SDK 1

#if CSIP_SDK
#include "..\\plugin\\GB28181_platform\\tools\\ClientSDK\\ClientSDK_sln\\ClientSDK\\CSIP_SDK.h"
#pragma comment(lib, "..\\plugin\\GB28181_platform\\bin\\tools\\ClientSDK_sln\\Win32\\CSIP_SDK.lib")
#endif



#pragma pack(1)
typedef struct _AudioInfo
{	
	char				mediaName[16];
	char				codecName[16];
	unsigned int		profile_level_id;
	struct timeval		timestamp;

	int				channel;
	unsigned int	samplerate;
	unsigned int	bits_per_samples;
}AudioInfo;

typedef struct _open_param_t
{
	char			ProxyServer[32];
	int				channel;
	char			localIP[32];
	char			userName[32];
	char			passWd[32];
	int				port;	
	char			deviceId[32];
	char			platIP[32];	
	char			platID[32];
	char			longitude[32];
	char			latitude[32];
}PARAMS_DEF_GB28181_t;
#pragma pack()


class Self_GB28181: public Instance
{
public:
	Self_GB28181(const VS_SIGNAL* signal, SigCB cb, void* context, const Uuid& item): Instance(signal, cb, context, item)
		, _is_first_frame(true)
		, _audiodesc(reinterpret_cast<VS_AUDIO_STREAM_DESC*>(buf1))
		, _videodesc(reinterpret_cast<VS_VIDEO_STREAM_DESC*>(buf2))
		, _frameNO(0)
		, _is_audio_valid(false)
		, _is_video_valid(false)		
	{		
		sumObj++;
		curObj = sumObj;
		LOG(LEVEL_INFO, "<GB28181>(%u) Construction success, object's tagNum=%d.\n", this, curObj);

		_audio_stamp.tv_sec = 0;
		_audio_stamp.tv_usec = 0;		

		uiFrameLen = 0;
		memset(frameData, 0, 1000*1024);

#if CSIP_SDK 
		m_bIDRFrame = false;
		GetCurTime(m_ui64RecvTimePre);
#endif
	}

	~Self_GB28181(void)
	{		
		sumObj--;
		LOG(LEVEL_INFO, "<GB28181>@(%u) Deconstruction success, object's tagNum=%d, remAll=%d.\n", this, curObj, sumObj);
	}
	

private:

#if CSIP_SDK
	void threadloop()
	{
		int ret = 0;
		if((ret = SDK_Login()) < 0)
		{
			LOG(LEVEL_ERROR,"SDK_Login() Failed!\n");
			req_error(XVIDEO_PLUG_LOGIN_ERR);
			return;
		}

		if((ret = SDK_Play()) < 0)
		{
			LOG(LEVEL_ERROR,"SDK_Play() Failed!\n");
			req_error(XVIDEO_PLUG_CONNECT_ERR);	
			SDK_Logout();			
			return;
		}	
	
		while(! isexit())
		{			
			if (!CheckData())
			{
				LOG(LEVEL_INFO," if (!CheckData()) .      No Data Received. ");
				req_error(XVIDEO_PLUG_CONNECT_ERR);		
				break;
			}			
			Sleep(30);
		}
		
		SDK_Stop();
		SDK_Logout();			
	}
#endif

private:
	void req_desc(void);

	void req_video(const VS_RESOL& resol, const VS_AREA& valid, bool isiframe, u_int32 timestamp, u_int32 datalen, unsigned char* data);

	void req_audio(u_int32 timestamp, u_int32 datalen, unsigned char* data);
	
private:
	//static int g_opennum;
	//static int g_count;
	
private:
	unsigned int		GetFrameType(unsigned char* data, int size, unsigned int codec);

	unsigned int		_frameNO;	
	
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

private:	

	int	GetCodec(char* mediaName, char* codecName)
	{
		if (mediaName != NULL)
		{
			if (strcmp(mediaName, "audio") == 0 && strcmp(codecName, "MPEG4-GENERIC"))
			{
				return CODEC_AAC;
			}

			if (strcmp(mediaName, "audio") == 0 && strcmp(codecName, "L16"))
			{
				return CODEC_PCM;
			}
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
	//debug
	static int			sumObj;
	int					curObj;	

////////////////////////////////////////////////////////////////////////////////////////////
private:
#pragma pack(push)
#pragma pack(1)
	struct ExtInfSignal
	{
		char			ProxyServer[32];
		int				xSignalID;	
		char			platIP[32];
		char			platID[32];		
		char			deviceId[32];
		char			userName[32];
		char			passWd[32];
	};

#pragma pack(pop)

public:	
	bool			GetInfOfSignal(PARAMS_DEF_GB28181_t& paraDef);	
private:

	PARAMS_DEF_GB28181_t	_paraDef;
	char				frameData[10*1024*1024];
	unsigned int		uiFrameLen;


	/*----------------------- CSIP_SDK ------------------------*/
#if CSIP_SDK 

#define SUB_PORT (0)
	long				_serverHd;
	long				_realHd;
	bool				m_bIDRFrame;

	unsigned __int64	m_ui64RecvTimePre;
public:
	bool		IsIDRFrame();
	void		CheckIDRFrame(char* data, unsigned int datalen);

	int			SDK_Login();
	int			SDK_Logout();
	int			SDK_Play();
	int			SDK_Stop();
	bool		SDK_videoproc(const long realHd, mediaInfo* mI, char* data, int size);
	void		GetCurTime(unsigned __int64 &arg);
	bool		CheckData();

	static void ciCB(const long serverHd, const catalog* cIL, void* parameter);
	static void eCB(const long serverHd, const errorInfo* eI, void* parameter);
	static void mCB(const long realHd, mediaInfo* mI, char* data, int dataLen, void* parameter);

#endif
	/*----------------------- CSIP_SDK ------------------------*/

};//Self_GB28181


void* imp_gb28181_open(const VS_SIGNAL* signal, SigCB cb, void* context, const Uuid& item);
int   imp_gb28181_close(void* handle);
int   imp_gb28181_set_i_frame(void* handle);
int   imp_gb28181_error(void* handle);

