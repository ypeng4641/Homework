#pragma once

#include "Plugin.h"
#include "vtron_hipc.h"
#include <error/xerror.h>
using namespace device;

void* imp_uhi01_open(const VS_SIGNAL* signal, SigCB cb, void* context, const Uuid& item);
int   imp_uhi01_close(void* handle);
int   imp_uhi01_set_i_frame(void* handle);
int   imp_uhi01_error(void* handle);


class Self_HIPC_RGB: public Instance
{
private:
	enum 
	{
		CMD_PORT		= 5001,
		REQ_PORT		= 5002,
		WEB_CMD_PORT	= 5003,
	};

	enum 
	{
		MAX_BUF = 10 * 1024 * 1024,
		//FIRST_STREAM_ID = 1,
		//LAST_STREAM_ID = 3,
	};

	enum
	{
		//STREAM_NUM = (LAST_STREAM_ID+1 - FIRST_STREAM_ID),
	};
	int FIRST_STREAM_ID;
	int LAST_STREAM_ID;
#define STREAM_NUM (LAST_STREAM_ID+1 - FIRST_STREAM_ID)

public:
	Self_HIPC_RGB(const VS_SIGNAL* signal, SigCB cb, void* context, const Uuid& item): Instance(signal, cb, context, item),
		_req_fd(-1), _audio_fd(-1), /*_video_fd(-1),*/
		/*_bRes(false),*/ _get_audio_suc(false),
		/*_is_first_frame(true),*/
		_audiodesc(reinterpret_cast<VS_AUDIO_STREAM_DESC*>(buf1))/*,
		_videodesc(reinterpret_cast<VS_VIDEO_STREAM_DESC*>(buf2))*/
		, vFrameNum(0)
		, aFrameNum(0)
		, FIRST_STREAM_ID(0), LAST_STREAM_ID(0)
		, _missV(0), _preV(0)
		, _stmCnt(0)
	{
		//paramInit();

		_preV = GetTickCount();
		if(!getchannel(_channel, _stream))
		{
			_channel = 0;
			_stream = 1;
		}
		
		//
		sumObj = ::InterlockedIncrement((unsigned int*)&sumObj);
		curObj = sumObj;
		LOG(LEVEL_INFO, "ypeng@ uhi01(%u),Construction success, object's tagNum=%d, STREAM_NUM=%d.\n", this, curObj, STREAM_NUM);
	}

	~Self_HIPC_RGB()
	{
		//paramDeinit();
		
		//
		sumObj = ::InterlockedDecrement((unsigned int*)&sumObj);
		LOG(LEVEL_INFO, "ypeng@ uhi01(%u),Deconstruction success, object's tagNum=%d, remAll=%d.\n", this, curObj, sumObj);
	}
	
private:
	static void* setparam(void* arg)
	{
		Self_HIPC_RGB* pthis = reinterpret_cast<Self_HIPC_RGB*>(arg);
		pthis->setparam();

		return NULL;
	}

	void setparam();

private:
	void paramInit(void)
	{
		_video_fd_list.resize(STREAM_NUM, -1);
		_is_first_frame_list.resize(STREAM_NUM, true);
		_firstIFrame_list.resize(STREAM_NUM, false);
		_resol_list.resize(STREAM_NUM);
		_video_stream_id_list.resize(STREAM_NUM);
		_video_block_id_list.resize(STREAM_NUM);
		_video_stamp_id_list.resize(STREAM_NUM);
		_video_buf_list.resize(STREAM_NUM);
		for(int i = 0; i < STREAM_NUM; i++)
		{
			char* b = new buf2;
			memset(b, 0, sizeof(buf2));
			_videodesc_list.push_back(reinterpret_cast<VS_VIDEO_STREAM_DESC*>(b));
		}
	}

	void paramDeinit(void)
	{
		for(int i = 0; i < STREAM_NUM; i++)
		{
			delete[] (char*)_videodesc_list[i];
			_videodesc_list[i] = NULL;
		}
		_videodesc_list.clear();
	}

	bool getchannel(int& channel, int& stream)
	{
		const VS_SIGNAL* p = reinterpret_cast<const VS_SIGNAL*>(signal());
		if(p->datalen != sizeof(PARAMS_DEF))
		{
			return false;
		}

		//int result = *reinterpret_cast<const int*>(p->data);
		PARAMS_DEF* result = (PARAMS_DEF*) (p->data);

		if(result->channel < 0 || result->streamID < 0)//(result != 0 && result != 1)
		{
			return false;
		}

		channel = result->channel;
		stream = result->streamID;
		LOG(LEVEL_INFO, "ypeng@ Get channel=%d, stream=%d.\n", channel, stream);
		return true;
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
			for(int i = 0; i < STREAM_NUM; i++)
			{
				if(_video_fd_list[i] > 0)
				{
					FD_SET(_video_fd_list[i], &fdset);
				}
			}
			if(_get_audio_suc)
			{
				FD_SET(_audio_fd, &fdset);
			}

			int nfds = Network::Select (FD_SETSIZE, &fdset, NULL, NULL, &tv);
			if( nfds > 0)
			{
				for(int i = 0; i < STREAM_NUM; i++)
				{
					if(_video_fd_list[i] > 0 && FD_ISSET(_video_fd_list[i], &fdset)) 
					{
						if(! videoproc(i + FIRST_STREAM_ID))
						{
							req_error(XVIDEO_PLUG_RECVVIDEO_ERR);
							//break;
							goto FAILURE;
						}
						else
						{
							_missV = 0;
							_preV = GetTickCount();
						}
					}
				}
				
				if(_get_audio_suc && FD_ISSET(_audio_fd, &fdset)) 
				{
					if(!audioproc())
					{
						req_error(XVIDEO_PLUG_RECVAUDIO_ERR);
						//break;
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

		disconnect();
	}

	bool connect();

	void disconnect();

	bool videoproc(int streamID);

	bool audioproc();

	void req_desc(const Uint8 streamID, const VS_RESOL& resol, const VS_AREA& valid);

	void req_video(const Uint8 streamID, const VS_RESOL& resol, const VS_AREA& valid, bool isiframe, u_int32 timestamp, u_int32 datalen, unsigned char* data);

	void req_audio(u_int32 timestamp, u_int32 datalen, unsigned char* data);

private:
	bool get_video_rate(int sockfd, Uint8 streamID, int& rate);

	bool get_video_res(int sockfd, Uint8 streamID);

	bool get_audio_res(int sockfd);

	bool get_video_port(int sockfd, Uint8 streamID, u_int16& port);

	bool get_audio_port(int sockfd, Uint8 streamID, u_int16& port);

	void disconnect_video(int sockfd, Uint8 streamID);

	void disconnect_audio(int sockfd, Uint8 streamID);

	bool make_i_frame(int sockfd, Uint8 streamID);

private:
	//bool openAudioEncoder(Uint8 sample_rate, Uint8 sample_bit, Uint8 sample_chan);

	//int encodeAudio();

	//bool closeAudioEncoder();
	
private:
	bool check(HIPC_V_PACKET* p)
	{
		if(p->sync != 0x12345678)
		{
			return false;
		}

		if(p->packet_last != 0 && p->packet_last != 1)
		{
			return false;
		}

		if(p->valid_x + p->valid_w > p->all_w || p->valid_y + p->valid_h > p->all_h)
		{
			return false;
		}

		return true;

	}

	bool check(HIPC_A_PACKET* p)
	{
		if(p->sync != 0x87654321)
		{
			LOG(LEVEL_ERROR, "ypeng@ sync error, sync=%x\n", p->sync);
			return false;
		}

		if(p->packet_last != 0 && p->packet_last != 1)
		{
			return false;
		}

		return true;
	}

	int GetHeader(int sockfd, HIPC_V_PACKET *header)
	{
		u_int32       readlen;

		if ( (readlen =	Network::Readn(sockfd, header, sizeof(HIPC_V_PACKET))) != sizeof(HIPC_V_PACKET))
		{
			LOG(LEVEL_ERROR, "Readn from sockfd(%d) is error!\n", sockfd);
			return RET_ERROR;
		}

		if(!check(header) )
		{
			LOG(LEVEL_ERROR,"Check header's data is ERROR!\n");
			return RET_ERROR;
		}

		//LOG(LEVEL_INFO, "ypeng@ GetHeader video success!\n");
		return RET_SUCCESS;

	}

	int GetHeader(int sockfd, HIPC_A_PACKET *header)
	{
		u_int32       readlen;

		if ( (readlen =	Network::Readn(sockfd, header, sizeof(HIPC_A_PACKET))) != sizeof(HIPC_A_PACKET))
		{
			LOG(LEVEL_ERROR, "Readn from sockfd(%d) is error!\n", sockfd);
			return RET_ERROR;
		}

		if(!check(header))
		{
			LOG(LEVEL_ERROR,"Check header's data is ERROR!\n");
			return RET_ERROR;
		}

		//LOG(LEVEL_INFO, "ypeng@ GetHeader audio success!\n");
		return RET_SUCCESS;

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

	void new_id()
	{
		for(int i = 0; i < STREAM_NUM; i++)
		{
			_video_stream_id_list[i].Generate();
			_video_block_id_list[i].Generate();
			_video_stamp_id_list[i].Generate();
		}
		_audio_stream_id.Generate();
		_audio_stamp_id.Generate();
	}

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
	int _channel;
	int _stream;

	int _req_fd;
	int _audio_fd;
	std::vector<int> _video_fd_list;
	bool _get_audio_suc;

	int _stmCnt;

private:
	//bool _bRes;
	std::vector<bool> _is_first_frame_list;
	std::vector<VS_RESOL> _resol_list;

	char buf1[sizeof(VS_AUDIO_STREAM_DESC)];
	VS_AUDIO_STREAM_DESC* _audiodesc;
	typedef char buf2[sizeof(VS_VIDEO_STREAM_DESC) + sizeof(VS_BLOCK_DESC)];
	std::vector<VS_VIDEO_STREAM_DESC*> _videodesc_list;

	typedef	std::vector<Uint8> _video_buf;
	std::vector<_video_buf> _video_buf_list;
	std::vector<u_int8> _audio_buf;
	
private:
	pthread_t _thread;

private:
	std::vector<Uuid> _video_stream_id_list;
	std::vector<Uuid> _video_block_id_list;
	std::vector<Uuid> _video_stamp_id_list;
	Uuid _audio_stream_id;
	Uuid _audio_stamp_id;

	std::vector<bool> _firstIFrame_list;

	Uint32 _missV;
	//bool _hasV;
	Uint32 _preV;

private://just for debugMsg
	static int	sumObj;
	int			curObj;
	Uint32		vFrameNum;
	Uint32		aFrameNum;


};//Self_HIPC_RGB
