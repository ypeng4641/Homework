#include "imp_rtsplive555.h"
//#include "h264_resol.h"
#include "h264.h"

//#define _THREAD_SAFE_
//#include "SaveFileHelper.h"
//SFHelper sfhp;
#include "put_bits.h"


int get_freq_index(unsigned int frequency)
{
	if (96000 == frequency)		return 0;
	if (88200 == frequency)		return 1;
	if (64000 == frequency)		return 2;
	if (48000 == frequency)		return 3;
	if (44100 == frequency)		return 4;
	if (32000 == frequency)		return 5;
	if (24000 == frequency)		return 6;
	if (22050 == frequency)		return 7;
	if (16000 == frequency)		return 8;
	if (12000 == frequency)		return 9;
	if (11025 == frequency)		return 10;
	if (8000  == frequency)		return 11;
	if (7350  == frequency)		return 12;

	return 15;
}

int Self_RTSP_live555::g_opennum  	    = 0;
int Self_RTSP_live555::g_count		    = 0;
//ReadResolution* ReadResolution::m_once	= NULL;
int	Self_RTSP_live555::sumObj			= 0;

RtspThreadContext g_context[MAX_CHANNEL_NUM];

static pthread_mutex_t g_mutex	= PTHREAD_MUTEX_INITIALIZER;

static long long sub_time(struct timeval tv1, struct timeval tv2)
{
	return (long long)(tv1.tv_sec - tv2.tv_sec) * 1000000 + (tv1.tv_usec - tv2.tv_usec);
}


bool Self_RTSP_live555::connect()
{
	RTSP_PARAMS_DEF*	param = (RTSP_PARAMS_DEF*)(signal()->data);
	strncpy(_rtspParam.url, param->url, MAX_URL_LEN);
	strncpy(_rtspParam.username, param->username, sizeof(_rtspParam.username));
	strncpy(_rtspParam.password, param->password, sizeof(_rtspParam.password));
	_rtspParam.tranProtocol		= param->transportProtocol;
	_timestampMechanism 		= (TimeStampMechanism)param->timestampMechanism;

	_channel = AllocChannel();
	if(_channel < 0)
	{
		return false;
	}

	g_context[_channel].pThis 			= (long)this;
	g_context[_channel].eventLoopVar	= 0;
	g_context[_channel].shutdown		= false;
	LOG(LEVEL_INFO, "CONTEXT:%08X, connect rtsp url:%s, protocol:%d, user:%s, pwd:%s .",
		(unsigned long)this, param->url, (int)param->transportProtocol, param->username, param->password);

	return true;
}

bool Self_RTSP_live555::disconnect()
{

	FreeChannel(_channel);
	LOG(LEVEL_INFO, "CONTEXT %08X: Disconnect", (unsigned long)this);

	LockHelper lock(&g_mutex);
	if(--g_opennum <= 0 && g_count > 64)
	{		
		//_exit(0);
		LOG(LEVEL_ERROR, "ERROR, rtsp over 64 OR negative! g_opennum=%d, g_count=%d.\n", g_opennum, g_count);
	}
	return true;
}

bool Self_RTSP_live555::play()
{
	pthread_create(&_tID, NULL, RtspThread, this);
	{
		LockHelper lock(&g_mutex);

		g_opennum++;
		g_count++;
	}
	LOG(LEVEL_INFO, "CONTEXT %08X: PLAY", (unsigned long)this);
	return true;
}

bool Self_RTSP_live555::stop()
{
	{
		LockHelper lock(&g_context[_channel].mutex);

		g_context[_channel].eventLoopVar	= 1;
	}
	pthread_join(_tID, NULL);
	LOG(LEVEL_INFO, "CONTEXT %08X: STOP", (unsigned long)this);
	return true;
}

void* Self_RTSP_live555::RtspThread(void* param)
{
	Self_RTSP_live555* plugin = (Self_RTSP_live555*)param;

  	TaskScheduler* scheduler = BasicTaskScheduler::createNew();
  	UsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler);

	int channel = plugin->_channel;

    RTSPClient* client = openURL(*env, plugin->_rtspParam, channel, StreamCallback);
	if(!client)
	{
		LOG(LEVEL_ERROR, "CONTEXT %08X: openurl failed, url:%s ", (int)plugin, plugin->_rtspParam.url);
		plugin->req_error(XVIDEO_PLUG_CONNECT_ERR);
		return NULL;
	}
	else
	{
		LOG(LEVEL_INFO, "CONTEXT %08X: openurl success, url:%s ", (int)plugin, plugin->_rtspParam.url);
	}
	
	plugin->_last_time = ::GetTickCount();
	plugin->_client = client;
	plugin->_working = true;
	pthread_t tid;
	pthread_create(&tid, NULL, KeepAliveThread, plugin);

  	env->taskScheduler().doEventLoop(&(g_context[channel].eventLoopVar));

	LOG(LEVEL_INFO, "CONTEXT %08X: EventLoop END, eventLoopVar=%d", (int)plugin, (int)g_context[channel].eventLoopVar);

	plugin->_working = false;
	pthread_join(tid, NULL);
	if(!g_context[channel].shutdown)
	{
		shutdownStream(client);
	}
	
	plugin->req_error(XVIDEO_PLUG_SDK_ERR);
	return NULL;
}

void* Self_RTSP_live555::KeepAliveThread(void* param)
{
	printf("---------- keep_alive thread is running\n");
	Self_RTSP_live555* plugin = (Self_RTSP_live555*)param;


	RTSPClient* client = plugin->_client;

	int count = 0;

	while (plugin->_working && plugin->_client)
	{
		long long now = ::GetTickCount();

		if (now - plugin->_last_time > 3000)
		{
			plugin->req_error(XVIDEO_PLUG_NODATA_ERR);
			break;
		}

		if (count++ % 300 == 2)
		{
			printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
			sendGetParameterCommand(client);
		}

		Sleep(10);	
	}	

	printf("----------- keep_alive thread exit\n");

	return 0;
}

void Self_RTSP_live555::StreamCallback(int channel, int dataSize, char* data, RtspFrameInfo info)
{
	Self_RTSP_live555* plugin	= (Self_RTSP_live555*)g_context[channel].pThis;
	if(plugin && (channel >=0 && channel < MAX_CHANNEL_NUM))
	{
		plugin->_last_time = ::GetTickCount();
		plugin->doStreamCallback(channel, dataSize, data, info);
	}
}

void Self_RTSP_live555::doStreamCallback(int channel, int dataSize, char* data, RtspFrameInfo info)
{
	if (0 == strcmp(info.mediaName, "audio") && 0 == strcmp(info.codecName, "MPEG4-GENERIC"))
	{
		if (!_is_audio_valid || _audiodesc->samples != info.frequency || _audiodesc->channels != info.channels)
		{
			_audio_stream_id.Generate();
			_audio_stamp_id.Generate();

			_audiodesc->id = _audio_stream_id;
			_audiodesc->codec = CODEC_AAC;
			_audiodesc->samples = info.frequency;
			_audiodesc->channels = info.channels;
			_audiodesc->bit_per_samples = 16;
			_audiodesc->grade = 0;

			_is_audio_valid = true;
			req_desc();
		}

		if (!_is_video_valid)
		{
			return;
		}

		if (_audio_stamp.tv_sec != info.timestamp.tv_sec || _audio_stamp.tv_usec != info.timestamp.tv_usec)
		{
			if (_audio_frame.size() > 0)
			{
				long long pts = (long long)_audio_stamp.tv_sec*1000 + _audio_stamp.tv_usec / 1000;
				req_audio(pts, _audio_frame.size(), (unsigned char*)_audio_frame.c_str());
			}

			_audio_frame.clear();
			_audio_stamp = info.timestamp;
		}


		if (_audio_frame.size() <= 0)
		{
			#define	ADTS_HEADER_SIZE 7
			unsigned char buf[ADTS_HEADER_SIZE];

			PutBitContext pb;

			init_put_bits(&pb, buf, ADTS_HEADER_SIZE);

			/* adts_fixed_header */
			put_bits(&pb, 12, 0xfff);   /* syncword */
			put_bits(&pb, 1, 0);        /* ID */
			put_bits(&pb, 2, 0);        /* layer */
			put_bits(&pb, 1, 1);        /* protection_absent */
			put_bits(&pb, 2, info.profile_level_id); /* profile_objecttype */
			put_bits(&pb, 4, get_freq_index(info.frequency));
			put_bits(&pb, 1, 0);        /* private_bit */
			put_bits(&pb, 3, info.channels); /* channel_configuration */
			put_bits(&pb, 1, 0);        /* original_copy */
			put_bits(&pb, 1, 0);        /* home */

			/* adts_variable_header */
			put_bits(&pb, 1, 0);        /* copyright_identification_bit */
			put_bits(&pb, 1, 0);        /* copyright_identification_start */
			put_bits(&pb, 13, ADTS_HEADER_SIZE + dataSize + 0); /* aac_frame_length */
			put_bits(&pb, 11, 0x7ff);   /* adts_buffer_fullness */
			put_bits(&pb, 2, 0);        /* number_of_raw_data_blocks_in_frame */

			flush_put_bits(&pb);

			_audio_frame.append((char*)buf, ADTS_HEADER_SIZE);
		}

		_audio_frame.append(data, dataSize);
		_audio_stamp = info.timestamp;

		//long long pts = (long long)info.timestamp.tv_sec*1000000 + info.timestamp.tv_usec;
		//req_audio(pts, _audio_frame.size(), (unsigned char*)_audio_frame.c_str());

		//_audio_frame.clear();

		return;
	}

	if(0 == strncmp(info.mediaName, "video", 5))// && 0 == strcmp(info.codecName, "H264"))
	{
		//sfhp.SF_write(data, dataSize, NULL, sfhp.SF_open("rtspHead.h264"));

		if(info.isHeader == 1)
		{
			_header.copy(data, dataSize);
			return;
		}

		if(_lastFrameDetail.stamp.tv_sec == 0 && _lastFrameDetail.stamp.tv_usec == 0)
		{
			_lastFrameDetail.stamp                  = info.timestamp;
			_lastFrameDetail.syncUseRtcp			= info.syncUseRTCP;
			_lastFrameDetail.frameBufferDatasize	= dataSize;
			memcpy(_lastFrameDetail.frameBuffer, data, dataSize);
		}
		else if(_lastFrameDetail.stamp.tv_sec == info.timestamp.tv_sec && _lastFrameDetail.stamp.tv_usec == info.timestamp.tv_usec)
		{
			memcpy(_lastFrameDetail.frameBuffer + _lastFrameDetail.frameBufferDatasize, data, dataSize);
			_lastFrameDetail.frameBufferDatasize += dataSize;
			return;
		}
		else
		{
			_frameNO++;

			// 显示时间不同，表明是一个新的包，那就把之前的那个包发送出去。
			int codecType = GetCodec(info.mediaName, info.codecName);
			if(codecType < 0)
			{
				req_error(XVIDEO_PLUG_RECVVIDEO_ERR);
				return;
			}

			_frame.merg(_header.mem(), _lastFrameDetail.frameBuffer, _header.size(), _lastFrameDetail.frameBufferDatasize);

			
			int subtype = GetFrameType((unsigned char*)_frame.mem(), _frame.size(), codecType);
			if(subtype == -1)
			{
				req_error(XVIDEO_PLUG_RECVVIDEO_ERR);
				return;
			}

			//DoVideoStreamDecode(GetCodec(info.mediaName, info.codecName) ,
			//					subtype,
			//					GetPTS(_lastFrameDetail.stamp, _lastFrameDetail.syncUseRtcp) / 1000,
			//					region,
			//					_frame.mem(),
			//					_frame.size()
			//					);
			unsigned int stamp = GetPTS(_lastFrameDetail.stamp, _lastFrameDetail.syncUseRtcp) / 1000;

			
			if (!_is_video_valid)
			{
				/*if(! ReadResolution::FindJPGFileResolution((unsigned char*)_frame.mem(), _frame.size(), (int&)_resol.w, (int&)_resol.h))
				{
					LOG(LEVEL_ERROR, "h264_decode_seq_parameter_set failed!");
					req_error(XVIDEO_PLUG_RECVVIDEO_ERR);
					return;
				}*/
				
				{
					uint16_t w = 0;
					uint16_t h = 0;
					if (!H264Parse::GetResolution((unsigned char*)_frame.mem(), _frame.size(), &w, &h))
					{
						LOG(LEVEL_ERROR, "h264_decode_seq_parameter_set failed!");
						req_error(XVIDEO_PLUG_RECVVIDEO_ERR);
						return;
					}

					_resol.w = w;
					_resol.h = h;
				}

				_video_stream_id.Generate();
				_video_block_id.Generate();
				_video_stamp_id.Generate();

				VS_AREA valid = {0, 0, _resol.w, _resol.h};

				_videodesc->id = _video_stream_id;
				_videodesc->isaudio = 0;
				_videodesc->resol.w = valid.w;
				_videodesc->resol.h = valid.h;
				_videodesc->rows = 1;
				_videodesc->cols = 1;
				_videodesc->grade = grade(_resol.w, _resol.h);
				_videodesc->desc[0].id = _video_block_id;
				_videodesc->desc[0].area = valid;
				_videodesc->desc[0].resol = _resol;
				_videodesc->desc[0].valid = valid;

				_is_video_valid = true;
				_is_first_frame = false;

				req_desc();
			}

			VS_AREA valid = {0, 0, _resol.w, _resol.h};
			req_video(_resol, valid, (0==subtype)?true:false, stamp, _frame.size(), (unsigned char*)_frame.mem());

			//sfhp.SF_write((char*)_frame.mem(), _frame.size(), NULL, sfhp.SF_open("rtspResol.h264"));


			//新一帧的第一个包，保存起来
			_lastFrameDetail.stamp                  = info.timestamp;
			_lastFrameDetail.syncUseRtcp			= info.syncUseRTCP;
			_lastFrameDetail.frameBufferDatasize	= dataSize;
			memcpy(_lastFrameDetail.frameBuffer, data, dataSize);
		}
	}
}

unsigned int Self_RTSP_live555::GetPTS(struct timeval stamp, bool syncUseRTCP)
{
	TimeStampMechanism	mechanism	= _timestampMechanism;
	if(mechanism == UseRtpTimeStamp)
	{
			long long pts = (long long)stamp.tv_sec*1000000 + stamp.tv_usec;

			if(_startStamp == 0)
			{
				_startStamp 	= pts;
				_lastStamp		= pts;
				_syncUseRTCP	= syncUseRTCP;
				gettimeofday(&_startTime, NULL);
			}

			if(_syncUseRTCP != syncUseRTCP)
			{
				_syncUseRTCP	= syncUseRTCP;

				struct timeval nowtime;
				gettimeofday(&nowtime, NULL);
				long long d = sub_time(nowtime, _startTime);
				_startStamp		+= pts - _lastStamp - d / _frameNO;
			}
			else if(pts - _lastStamp > 2000000 || pts - _lastStamp < -2000000)
			{
				printf("stamp error, diff = %lld\n", pts - _lastStamp);
				struct timeval nowtime;
				gettimeofday(&nowtime, NULL);
				long long d = sub_time(nowtime, _startTime);
				_startStamp		+= pts - _lastStamp - d / _frameNO;
			}

			_lastStamp 			= pts;

			return pts - _startStamp;
	}
	else if(mechanism == UseFrameRate)
	{
		long long diff		= 0;

		if(_timeRecord == NULL)
		{
			_timeRecord	= new struct timeval[TIME_RECORD_CAPACITY];
		}

		struct timeval nowtime;
		gettimeofday(&nowtime, NULL);
		_timeRecord[_frameNO % TIME_RECORD_CAPACITY] = nowtime;

		if(_frameNO == 0) // first frame
		{
			_curStamp	= 0;
		}
		else if(_frameNO < TIME_RECORD_CAPACITY)
		{
			diff = sub_time(nowtime, _timeRecord[0]);
			_curStamp	+= diff / _frameNO;
		}
		else
		{
			diff = sub_time(nowtime, _timeRecord[(_frameNO + 1) % TIME_RECORD_CAPACITY]);
			_curStamp	+= diff / (TIME_RECORD_CAPACITY - 1);
		}

		return _curStamp;
	}
	else
	{
		return 0;
	}
}

unsigned int Self_RTSP_live555::GetFrameType(unsigned char* data, int size, unsigned int codec)
{
	int loopsize = 0;

	if(codec == CODEC_MPEG4)
	{
		while(size-->5)
		{
			loopsize++;

			if(*((unsigned int*)data) == 0XB6010000)
			{
				switch(data[4] & 0xC0)
				{
				case 0x00:
					return 0;//STREAM_IFRAME;
				case 0x40:
					return 1;//STREAM_PFRAME;
				case 0x80:
					return 2;//STREAM_BFRAME;
				default:
					return 2;//STREAM_UNTYPE;
				}
			}
			else if(loopsize > 1024)
			{
				break;
			}

			data++;
		}
	}
	else if(codec == CODEC_H264)
	{
		while(size-->5)
		{
			loopsize++;

			if((*((unsigned int*)data)&0xFFFFFF) == 0X010000)
			{
				switch(data[3] & 0x1F)
				{
				case 1:
					return 1;//STREAM_PFRAME;
				case 5:
					return 0;//STREAM_IFRAME;
				case 7:
				case 8:
					break;
				default:
					break;
				}
			}
			else if(loopsize > 1024)
			{
				break;
			}

			data++;
		}

		return 0;//STREAM_IFRAME;
	}

	return -1;//STREAM_UNTYPE;
}

	void Self_RTSP_live555::req_desc(void)
	{/*
		_audiodesc->id = _audio_stream_id;
		_audiodesc->codec = CODEC_AAC;
		_audiodesc->samples = A_SAMPLE;
		_audiodesc->channels = A_CHANNEL;
		_audiodesc->bit_per_samples = A_BIT;
		_audiodesc->grade = 0;

		_videodesc->id = _video_stream_id;
		_videodesc->isaudio = 0;
		_videodesc->resol.w = valid.w;
		_videodesc->resol.h = valid.h;
		_videodesc->rows = 1;
		_videodesc->cols = 1;
		_videodesc->grade = grade(resol.w, resol.h);
		_videodesc->desc[0].id = _video_block_id;
		_videodesc->desc[0].area = valid;
		_videodesc->desc[0].resol = resol;
		_videodesc->desc[0].valid = valid;
		Instance::req_desc(1, &_audiodesc, 1, &_videodesc);
	*/	
		if (_is_audio_valid && _is_video_valid)
		{
			Instance::req_desc(1, &_audiodesc, 1, &_videodesc);
			return;
		}

		if (_is_audio_valid)
		{
			//Instance::req_desc(1, &_audiodesc, 0, NULL);
			return;
		}

		if (_is_video_valid)
		{
			Instance::req_desc(0, NULL, 1, &_videodesc);
			return;
		}
	}

	void Self_RTSP_live555::req_audio(u_int32 timestamp, u_int32 datalen, unsigned char* data)
	{
		AudioData adata;
		adata.stream = _audio_stream_id;
		adata.stampId = _audio_stamp_id;
		adata.timestamp = timestamp;
		adata.codec = _audiodesc->codec;//CODEC_AAC;
		adata.samples = _audiodesc->samples;//16000;
		adata.channels = _audiodesc->channels;//1;
		adata.bit_per_samples = _audiodesc->bit_per_samples;//16;
		adata.datalen = datalen;
		adata.data = (char*)data;

		Instance::req_audio(&adata);
	}

	void Self_RTSP_live555::req_video(const VS_RESOL& resol, const VS_AREA& valid, bool isiframe, u_int32 timestamp, u_int32 datalen, unsigned char* data)
	{
		VideoData vdata;

		vdata.stream = _video_stream_id;
		vdata.block = _video_block_id;
		vdata.stampId = _video_stamp_id;
		vdata.area = valid;
		vdata.frametype = isiframe ? 0 : 1;
		vdata.resol = resol;
		vdata.valid = valid;
		vdata.codec = CODEC_H264;
		vdata.timestamp = timestamp;
		vdata.datalen = datalen;
		vdata.data = (char*)data;

		Instance::req_video(&vdata);

	}


void* imp_rtsplive555_open(const VS_SIGNAL* signal, SigCB cb, void* context, const Uuid& item)
{
	Self_RTSP_live555* i = new Self_RTSP_live555(signal, cb, context, item);
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
int   imp_rtsplive555_close(void* handle)
{
	Self_RTSP_live555* i = reinterpret_cast<Self_RTSP_live555*>(handle);
	i->exit();
	return 0l;
}

int   imp_rtsplive555_set_i_frame(void* handle)
{
	Self_RTSP_live555* i = reinterpret_cast<Self_RTSP_live555*>(handle);
	i->set_i_frame();
	return 0l;
}

int   imp_rtsplive555_error(void* handle)
{
	Self_RTSP_live555* i = reinterpret_cast<Self_RTSP_live555*>(handle);
	return i->geterror();
}
