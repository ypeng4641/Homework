#include "imp_vi04.h"
#include "vtron_box.h"

#include <sys/types.h>
#include <sys/timeb.h>

#define A_SAMPLE	16000
#define A_BIT		16
#define A_CHANNEL	1

#define A_BIT_RATE_8k	8000
#define A_BIT_RATE_16k	16000
#define A_BIT_RATE_32k	32000
#define A_BIT_RATE_64k	64000
#define A_BIT_RATE_128k	128000

bool Self_VI04::getchannel(int& channel)
{
	const VS_SIGNAL* p = reinterpret_cast<const VS_SIGNAL*>(signal());
	if(p->datalen != sizeof(PARAMS_DEF))
	{
		LOG(LEVEL_ERROR, "ypeng@ datalen error, type=%d, datalen=%d\n", p->type, p->datalen);
		return false;
	}

	//int result = *reinterpret_cast<const int*>(p->data);
	PARAMS_DEF* result = (PARAMS_DEF*) (p->data);
	if(result->channel < 0 || result->channel > 3)
	{
		LOG(LEVEL_ERROR, "ypeng@ channel error, result=%d\n", result->channel);
		return false;
	}

	channel = result->channel;
	LOG(LEVEL_INFO, "ypeng@ signal get channel success, channel=%d\n", channel);
	return true;
}

	bool Self_VI04::connect()
	{
		_cmd_fd = Network::MakeTCPClient(ipaddr(), CMD_PORT);
		if(_cmd_fd < 0)
		{
			goto FAILED;
		}

		_req_fd = Network::MakeTCPClient(ipaddr(), REQ_PORT);
		if(_req_fd < 0)
		{ 
			goto FAILED;
		} 

		// 
		Network::DefaultStream(_cmd_fd);
		Network::DefaultStream(_req_fd);

		u_int16 video_port = 0;
		if(!get_video_port(_req_fd, video_port))
		{
			LOG(LEVEL_ERROR, "Get video port failed");
			goto FAILED;
		}

		u_int16 audio_port = 0;
		if(!get_audio_port(_req_fd, audio_port))
		{
			LOG(LEVEL_ERROR, "Get audio port failed");
			goto FAILED;
		}
		else
		{
			_get_audio_suc = true;
		}

		_video_fd = Network::MakeTCPClient(ipaddr(), video_port);
		if( _video_fd < 0)
		{
			LOG(LEVEL_ERROR, "Connect video data service failed");
			goto FAILED;
		} 

		_audio_fd = Network::MakeTCPClient(ipaddr(), audio_port);
		if(_audio_fd < 0)
		{
			LOG(LEVEL_ERROR, "Connect audio data service failed");
			goto FAILED;
		} 

		Network::DefaultStream(_video_fd);
		Network::DefaultStream(_audio_fd);
		LOG(LEVEL_INFO, "vi04(%d) MakeClient, _video_fd=%d, _audio_fd=%d.", this, _video_fd, _audio_fd);
		goto SUCCESS;

FAILED:
		// 
		if(_req_fd >= 0)
		{
			if(_get_audio_suc)
			{
				disconnect_audio(_req_fd);
			}
			disconnect_video(_req_fd);
		}

		if(_cmd_fd >= 0)
		{
			Network::Close(_cmd_fd);
		}

		if(_req_fd >= 0)
		{
			Network::Close(_req_fd);
		}

		if(_audio_fd >= 0)
		{
			Network::Close(_audio_fd);
		}

		if(_video_fd >= 0)
		{
			Network::Close(_video_fd);
		}

		return false;

SUCCESS:
		return true;

	}

	bool Self_VI04::disconnect()
	{
		disconnect_audio(_req_fd);
		disconnect_video(_req_fd);

		Network::Close(_cmd_fd);
		Network::Close(_req_fd);
		Network::Close(_audio_fd);
		Network::Close(_video_fd);

		return true;
	}

	bool Self_VI04::get_video_port(int sockfd, u_int16& port)
	{
		T_IPC_VIDEO_CONNECT_REQ req;
		memset(&req, 0, sizeof(T_IPC_VIDEO_CONNECT_REQ) );
		req.head.msgType = 0x8203;
		req.channel = _channel;
		req.mode = 2;
		local(sockfd, sizeof(req.ipAddress), (char*)req.ipAddress);
		
		if(Network::Sendn(sockfd, &req, sizeof(req)) != sizeof(req))
		{
			return false;
		}
		
		fd_set fs;
		struct timeval tv = {1, 0};
		FD_ZERO(&fs);
		FD_SET(sockfd, &fs);
		if(Network::Select(FD_SETSIZE, &fs, NULL, NULL, &tv) <= 0)
		{
			LOG(LEVEL_ERROR, "failed, no respond.\n");
			return false;
		}

		if(Network::Recvn(sockfd, &req, sizeof(req.head)) != sizeof(req.head))
		{
			LOG(LEVEL_ERROR, "Get Packet failed");
			return false;
		}

		if(req.head.msgType != 0x8204 || req.head.errCode != 0 || req.head.len <= 0)// || req.port == 0 )
		{
			LOG(LEVEL_ERROR, "Wrong header! msgType(%#x), err(%d), len(%d).", req.head.msgType, req.head.errCode, req.head.len);
			return false;
		} 
		else
		{
			if(Network::Recvn(sockfd, &req.channel, req.head.len) != req.head.len)
			{
				LOG(LEVEL_ERROR, "Recv Body failed.");
				return false;
			}
		}

		if(req.port <= 0)
		{
			LOG(LEVEL_ERROR, "Wrong msg! port(%d).", req.port);
			return false;
		}
		else
		{
			port  = req.port;
			LOG(LEVEL_INFO, "ypeng@ video_port(%d).\n", port);
		}
		return true;
	}

	bool Self_VI04::get_audio_port(int sockfd, u_int16& port)
	{
		T_IPC_AUDIO_CONNECT_REQ	req;
		memset(&req, 0, sizeof(T_IPC_AUDIO_CONNECT_REQ) );
		req.head.msgType = 0x8300;
		req.channel = _channel;
		req.mode = 2;
		local(sockfd, sizeof(req.ipAddress), (char*)req.ipAddress);

		if(Network::Sendn(sockfd, &req, sizeof(req)) != sizeof(req))
		{
			return false;
		}

		if(Network::Recvn(sockfd, &req, sizeof(req)) != sizeof(req))
		{
			return false;
		}

		if(req.head.msgType != 0x8301 || req.head.errCode != 0 || req.port == 0)
		{
			return false;
		} 

		port  = req.port;
		LOG(LEVEL_INFO, "ypeng@ audio_port(%d)\n", port);
		
#ifdef A_CODEC
		openAudioEncoder(A_SAMPLE, A_BIT, A_CHANNEL);
#endif
		return true;
	}

	void Self_VI04::disconnect_video(int sockfd)
	{
		T_IPC_VIDEO_DISCONNECT_REQ req;
		req.head.msgType = 0x8205;
		req.channel = _channel;
		req.mode = 2;
		local(sockfd, sizeof(req.multAddress), (char*)req.multAddress);

		if(Network::Sendn(sockfd, &req, sizeof(req)) != sizeof(req))
		{
		}

		if(Network::Recvn(sockfd, &req, sizeof(req)) != sizeof(req))
		{
		}

		if(req.head.msgType != 0x8206 || req.head.errCode != 0)
		{
		} 
		
#ifdef A_CODEC
		closeAudioEncoder();
#endif

	}

	void Self_VI04::disconnect_audio(int sockfd)
	{
		T_IPC_AUDIO_DISCONNECT_REQ req;
		req.head.msgType = 0x8302;
		req.channel	= _channel;
		req.mode = 2;
		local(sockfd, sizeof(req.ipAddress), (char*)req.ipAddress);

		if(Network::Sendn(sockfd, &req, sizeof(req)) != sizeof(req))
		{
		}

		if(Network::Recvn(sockfd, &req, sizeof(req)) != sizeof(req))
		{
		}

		if(req.head.msgType != 0x8303 || req.head.errCode != 0)
		{
		} 

	}

	bool Self_VI04::make_i_frame(int sockfd)
	{
		T_IPC_KEYFRAME_REQ req;
		req.head.msgType = 0x8207;
		req.channel = _channel;

		if(Network::Sendn(sockfd, &req, sizeof(req)) != sizeof(req))
		{
			LOG(LEVEL_ERROR, "Send Packet failed");
			return false;
		}

		if(Network::Recvn(sockfd, &req, sizeof(req)) != sizeof(req))
		{
			LOG(LEVEL_ERROR, "Get Packet failed");
			return false;
		}

		if(req.head.msgType != 0x8208 || req.head.errCode != 0)
		{
			LOG(LEVEL_ERROR, "Return packet error, type = %d, errorcode = %d", req.head.msgType, req.head.errCode);
			return false;
		} 

		LOG(LEVEL_INFO, "Make I Frame Success!");

		return true;
	}

	void Self_VI04::req_desc(const VS_RESOL& resol, const VS_AREA& valid)
	{
		_audiodesc->id = _audio_stream_id;
#ifdef A_CODEC
		_audiodesc->codec = CODEC_AAC;
		_audiodesc->samples = A_SAMPLE;
		_audiodesc->channels = A_CHANNEL;
#else
		_audiodesc->codec = CODEC_PCM;
		_audiodesc->samples = 16000;
		_audiodesc->channels = 1;
#endif
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

	}

	void Self_VI04::req_video(const VS_RESOL& resol, const VS_AREA& valid, bool isiframe, u_int64 timestamp, u_int32 datalen, unsigned char* data)
	{
		VideoData vdata;

		vdata.stream = _video_stream_id;
		vdata.block = _video_block_id;
		vdata.stampId = _video_stamp_id;
		vdata.useVirtualStamp = true;
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

	void Self_VI04::req_audio(u_int32 timestamp, u_int32 datalen, unsigned char* data)
	{
		AudioData adata;
		adata.stream = _audio_stream_id;
		adata.stampId = _audio_stamp_id;
		adata.timestamp = timestamp;
#ifdef A_CODEC
		adata.codec = CODEC_AAC;
		adata.samples = A_SAMPLE;
		adata.channels = A_CHANNEL;
#else
		adata.codec = CODEC_PCM;
		adata.samples = 16000;
		adata.channels = 1;
#endif
		adata.bit_per_samples = A_BIT;
		adata.datalen = datalen;
		adata.data = (char*)data;

		Instance::req_audio(&adata);
	}

	bool Self_VI04::videoproc()
	{
		V_PACKET header;
		if(GetHeader(_video_fd, &header) != RET_SUCCESS)
		{
			return false;
		}

		unsigned int offset = _video_buf.size();
		_video_buf.resize(offset + header.packet_size);

		memcpy(&_video_buf[offset], header.frame_data, header.packet_size);

		if(_video_buf.size() >= MAX_BUF)
		{ 
			return false;
		} 

		if(header.packet_last == 1)
		{
			if(_video_buf.size() == header.frame_size)
			{
				VS_AREA valid;
				valid.x = header.valid_x;
				valid.y = header.valid_y;
				valid.w = header.valid_w;
				valid.h = header.valid_h;

				if(_is_first_frame)
				{
					//
					_resol.w = header.all_w;
					_resol.h = header.all_h;

					//
					new_id();
					req_desc(_resol, valid);

					//
					_is_first_frame = false;

				}				

				if(_resol.w != header.all_w || _resol.h != header.all_h)
				{
					return false;
				}

				req_video(_resol, valid, header.frame_type == 1 ? true : false, header.timestamp, _video_buf.size(), &_video_buf[0]);

			}

			_video_buf.clear();

		}

		return true;
	}

	bool Self_VI04::audioproc()
	{
		A_PACKET header;
		if(GetHeader(_audio_fd, &header) != RET_SUCCESS)
		{
			LOG(LEVEL_ERROR, "ypeng@ GetHeader failed!\n");
			return false;
		}

		unsigned int offset = _audio_buf.size();
		_audio_buf.resize(offset + header.packet_size);

		memcpy(&_audio_buf[offset], header.frame_data, header.packet_size);

		if(_audio_buf.size() >= MAX_BUF)
		{ 
			return false;
		} 

		if(header.packet_last == 1)
		{
			if(_audio_buf.size() == header.frame_size)
			{
				struct _timeb tb;
				_ftime(&tb);
				u_int64 curSysTime = ((u_int64)tb.time) * 1000 + tb.millitm;
				aFrameNum++;
				if(aFrameNum%200 == 1)
				//if(aFrameNum > 400)
				{
					LOG(LEVEL_INFO, "ypeng@ req_audio, aFrameNum=%u, timestamp=%u, stampDif=%d, size=%d, sysTimeDif=%llu.\n",
						aFrameNum, header.timestamp, header.timestamp-preA_Stamp, _audio_buf.size(), curSysTime-preSysTime);
				}
				preSysTime = curSysTime;
#ifdef A_CODEC
				encodeAudio(header.timestamp);
#else
				req_audio(header.timestamp, _audio_buf.size(), &_audio_buf[0]);
#endif
			}

			_audio_buf.clear();

		}

		return true;
	}

#ifdef A_CODEC
	bool Self_VI04::openAudioEncoder(u_int32 sample_rate, u_int8 sample_bit, u_int8 sample_chan)
	{
		av_register_all();
		
		const char* filename = "test.acc";
		fmt = guess_format("adts", NULL, NULL);//(NULL, filename, NULL);
		avFmtCnt = av_alloc_format_context();
		avFmtCnt->oformat = fmt;
		snprintf(avFmtCnt->filename, sizeof(avFmtCnt->filename), "%s", filename);
		audio_st = NULL;
		avCdCnt = NULL;
		
		if (fmt->audio_codec != CODEC_ID_NONE)
		{
			audio_st = av_new_stream(avFmtCnt, 1);
			avCdCnt = audio_st->codec;
			avCdCnt->codec_id = fmt->audio_codec;
			avCdCnt->codec_type = CODEC_TYPE_AUDIO;
			int orgBR = (sample_rate * sample_bit * sample_chan)/8;
			//avCdCnt->bit_rate = A_BIT_RATE;
			avCdCnt->bit_rate = (orgBR > 128000) ? (A_BIT_RATE_128k) : (A_BIT_RATE_8k);
			avCdCnt->sample_rate = sample_rate;//44100;
			avCdCnt->channels = sample_chan;//2;
		}
		if (av_set_parameters(avFmtCnt, NULL) < 0)
		{
			return false;
		}
		dump_format(avFmtCnt, 0, filename, 1);
		if (avCdCnt)
		{
			AVCodec* codec = NULL;
			codec = avcodec_find_encoder(avCdCnt->codec_id);
			avcodec_open(avCdCnt, codec);
			audio_outbuf_size = 10000;
			audio_outbuf = (uint8_t*)av_malloc(audio_outbuf_size);
			if (avCdCnt->frame_size <= 1)
			{
				audio_input_frame_size = audio_outbuf_size / avCdCnt->channels;
				switch (audio_st->codec->codec_id)
				{
				case CODEC_ID_PCM_S16LE:
				case CODEC_ID_PCM_S16BE:
				case CODEC_ID_PCM_U16LE:
				case CODEC_ID_PCM_U16BE:
					audio_input_frame_size >>= 1;
					break;
				default:
					break;
				}
			}
			else
			{
				audio_input_frame_size = avCdCnt->frame_size;
			}
			samples = (int16_t*)av_malloc(audio_input_frame_size*2*avCdCnt->channels);
		}
		
		if (!(!fmt->flags & AVFMT_NOFILE))
		{
			LOG(LEVEL_INFO, "ypeng@ Open decoder failed##########!\n");
			return false;
		}

		LOG(LEVEL_INFO, "ypeng@ Open decoder success!\n");
		return true;
	}

	int Self_VI04::encodeAudio(u_int32 timestamp)
	{
#if 0
			static FILE* aeFp_src = fopen("vi04.pcm", "wb");
			static int aeCnt_src = 0;
			if(NULL == aeFp_src)
			{
				goto SAVESRC_FAIL;
			}
			aeCnt_src++;
			if(aeCnt_src < 600)
			{
				fwrite(&_audio_buf[0], 1, _audio_buf.size(), aeFp_src);
			}
			else
			{
				fflush(aeFp_src);
				fclose(aeFp_src);
			}
SAVESRC_FAIL:
#endif
		//_audio_buf.size(), &_audio_buf[0]
		//memcpy(samples, &_audio_buf[0], _audio_buf.size());
		//±àÂë
		int dataSize = avcodec_encode_audio(avCdCnt, audio_outbuf, audio_outbuf_size, (short*)&_audio_buf[0]/*samples*/);
		if(dataSize <= 0)
		{
			LOG(LEVEL_ERROR, "avcodec_encode_audio2 failed!\n");
			return -1;
		}
		if(aFrameNum % 200 == 1)
		{
			LOG(LEVEL_INFO, "ypeng@ Audio decoder output! aFrameNum=%u, timestamp=%u, stampDif=%u, dataSize=%d\n", aFrameNum, timestamp, timestamp-preA_Stamp, dataSize);
		}
#ifdef A_READFILE
			char* aacData = NULL;
			int aacSize = 0;
			_aacHelper->getNext(&aacData, aacSize);
			if(NULL != aacData && 0 != aacSize)
			{
				req_audio(timestamp, aacSize, (u_int8*)aacData);
				if(aFrameNum % 200 == 1)
				{
					LOG(LEVEL_INFO, "ypeng@ ReadFile output! aFrameNum=%u, timestamp=%u, stampDif = %d, aacSize=%d.\n", aFrameNum, timestamp, timestamp-preA_Stamp, aacSize);
				}
			}
#else
			req_audio(timestamp, dataSize, audio_outbuf);
#endif
			
#if 0//def A_READFILE
			static FILE* aeFp = fopen("vi04.aac", "wb");
			static int aeCnt = 0;
			if(NULL == aeFp)
			{
				goto SAVE_FAIL;
			}
			aeCnt++;
			if(aeCnt < 100000)
			{
				fwrite(audio_outbuf, sizeof(char), dataSize, aeFp);
			}
			else
			{
				fflush(aeFp);
				fclose(aeFp);
				aeFp = NULL;
			}
SAVE_FAIL:
#endif

		preA_Stamp = timestamp;
		return (dataSize);
	}

	bool Self_VI04::closeAudioEncoder()
	{
		if (audio_st)
		{
			avcodec_close(audio_st->codec);
			av_free(samples);
			av_free(audio_outbuf);
		}
		for (int i=0; i<avFmtCnt->nb_streams; i++)
		{
			av_freep(&avFmtCnt->streams[i]->codec);
			av_freep(&avFmtCnt->streams[i]);
		}
		av_free(avFmtCnt);

		LOG(LEVEL_INFO, "ypeng@ Close audio decoder!\n");
		return true;
	}
#endif

int	Self_VI04::sumObj = 0;

void* imp_vi04_open(const VS_SIGNAL* signal, SigCB cb, void* context, const Uuid& item)
{
	Self_VI04* i = new Self_VI04(signal, cb, context, item);
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
int   imp_vi04_close(void* handle)
{
	Self_VI04* i = reinterpret_cast<Self_VI04*>(handle);
	i->exit();
	return 0l;
}

int   imp_vi04_set_i_frame(void* handle)
{
	Self_VI04* i = reinterpret_cast<Self_VI04*>(handle);
	i->set_i_frame();
	return 0l;
}

int   imp_vi04_error(void* handle)
{
	Self_VI04* i = reinterpret_cast<Self_VI04*>(handle);
	return i->geterror();
}



