#include "imp_uhi01.h"

#define A_SAMPLE	44100
#define A_BIT		16
#define A_CHANNEL	2

#define A_BIT_RATE_8k	8000
#define A_BIT_RATE_16k	16000
#define A_BIT_RATE_32k	32000
#define A_BIT_RATE_64k	64000
#define A_BIT_RATE_128k	128000

#define PARTICULAR_IP ("172.16.132.118")

	void Self_HIPC_RGB::setparam()
	{
		while(!isexit())
		{
			if(wait_i_frame())
			{
				for(int i = 0; i < STREAM_NUM; i++)
				{
					if(_videodesc_list[i]->resol.w != 0 && _videodesc_list[i]->resol.h != 0)
						if(! make_i_frame(_req_fd, i + FIRST_STREAM_ID))
						{
							LOG(LEVEL_ERROR, "ypeng@ make_i_frame failed!socket(%d), stream(%d)\n", _req_fd, i + FIRST_STREAM_ID);
							req_error(XVIDEO_PLUG_MAKEIFRAME_ERR);
							//break;
							return;
						}
				}
			}
		}
	}


	bool Self_HIPC_RGB::connect()
	{
		_req_fd = Network::MakeTCPClient(ipaddr(), REQ_PORT);

		in_addr inIP;
		inIP.S_un.S_addr = ntohl(ipaddr());
		char* ipStr = inet_ntoa(inIP);
		if(_req_fd < 0)
		{ 
			LOG(LEVEL_ERROR, "ypeng@ Fail to connect to ip(%s):REQ_PORT(%d).\n", ipStr, REQ_PORT);
			return false;
		}

		//特殊IP只获取第0路码流
		//if(strcmp(ipStr, PARTICULAR_IP) == 0)
		//{
		//	FIRST_STREAM_ID = 0;
		//	LAST_STREAM_ID = 0;
		//}

		Network::DefaultStream(_req_fd);

		//判断是否需求0码流
		//需求0码流，帧率若为30fps只获取0码流，否则获取1,3码流
		int fr = 0;
		if((_stream != 0)
			||
			(! get_video_rate(_req_fd, 0, fr) || fr != 30))
		{
			FIRST_STREAM_ID = 1;
			LAST_STREAM_ID = 3;
			LOG(LEVEL_WARNING, "Stream0 is not 30fps. Take stream 1,3 instead!");
		}
		else
		{
			LOG(LEVEL_INFO, "Get stream0 frame_rate(%d).", fr);
		}
		paramInit();

		//
		std::vector<u_int16> video_port_list;
		video_port_list.resize(STREAM_NUM, 0);

		new_id();

		bool hasone = false;
		for(int i = 0; i < STREAM_NUM; i++)
		{
			//不获取第3路1080P，20fps（streamID == 2）
			if(2 == (i + FIRST_STREAM_ID))
			{
				continue;
			}

			if(!get_video_res(_req_fd, i + FIRST_STREAM_ID))
			{
				//goto FAILED;
				continue;
			}

			if(!get_video_port(_req_fd, i + FIRST_STREAM_ID, video_port_list[i]))
			{
				//goto FAILED;
				continue;
			}

			_video_fd_list[i] = Network::MakeTCPClient(ipaddr(), video_port_list[i]);
			if( _video_fd_list[i] < 0)
			{
				//goto FAILED;
				continue;
			}
			else
			{
				_stmCnt++;
				hasone = true;
				Network::DefaultStream(_video_fd_list[i]);
				LOG(LEVEL_INFO, "ypeng@ Video TCP connect success, stream(%d):port(%d):resol(%d, %d).\n",
					i+FIRST_STREAM_ID, video_port_list[i], _videodesc_list[i]->resol.w, _videodesc_list[i]->resol.h);
			}
		}
		if(! hasone)
		{//3路均不成功，报错
			goto FAILED;
		}
		
		if(! get_audio_res(_req_fd))
		{
			//goto FAILED;
			_get_audio_suc = false;
		}
		else
		{
			_get_audio_suc = true;
		}

		u_int16 audio_port = 0;
		if(_get_audio_suc)
		{
			if(! get_audio_port(_req_fd, FIRST_STREAM_ID, audio_port))
			{
				_get_audio_suc = false;
			}
		}
		
		if(_get_audio_suc)
		{
			_audio_fd = Network::MakeTCPClient(ipaddr(), audio_port);
			if(_audio_fd < 0)
			{
				//goto FAILED;
				_get_audio_suc = false;
			}
			Network::DefaultStream(_audio_fd);
			LOG(LEVEL_INFO, "ypeng@ Audio TCP connect success, stream(%d):port(%d)\n", FIRST_STREAM_ID, audio_port);
		}

		//if(_get_audio_suc)
		//{
		//	Instance::req_desc(1, &_audiodesc, STREAM_NUM, &(_videodesc_list[0]));
		//	_bRes = true;
		//}
		//else
		//{
		//	Instance::req_desc(0, NULL, STREAM_NUM, &(_videodesc_list[0]));
		//}

		//LOG(LEVEL_INFO, "ypeng@ first req_desc success! getaudio(%d), STREAM_NUM(%d).\n", _get_audio_suc==true?1:0, STREAM_NUM);
		
		//LOG(LEVEL_INFO, "uhi01(%d) MakeClient, _video_fd0=%d, _video_fd1=%d, _video_fd2=%d, _audio_fd=%d.", this, _video_fd_list[0], _video_fd_list[1], _video_fd_list[2], _audio_fd);
		return true;
FAILED:
		// 
		if(_req_fd >= 0)
		{
			if(_get_audio_suc)
			{
				disconnect_audio(_req_fd, FIRST_STREAM_ID);
			}
			for(int i = 0; i < STREAM_NUM; i++)
			{
				disconnect_video(_req_fd, i + FIRST_STREAM_ID);
			}
			Network::Close(_req_fd);
			_req_fd = -1;
		}

		if(_audio_fd >= 0)
		{
			Network::Close(_audio_fd);
			_audio_fd = -1;
		}

		for(int i = 0; i < STREAM_NUM; i++)
		{
			if(_video_fd_list[i] >= 0)
			{
				Network::Close(_video_fd_list[i]);
				_video_fd_list[i] = -1;
			}
		}

		return false;
	}

	void Self_HIPC_RGB::disconnect()
	{
		paramDeinit();

		if(_get_audio_suc)
		{
			disconnect_audio(_req_fd, FIRST_STREAM_ID);
		}
		for(int i = 0; i < STREAM_NUM; i++)
		{
			if(_video_fd_list[i] > 0)
			{
				disconnect_video(_req_fd, i + FIRST_STREAM_ID);
			}
		}

		Network::Close(_req_fd);
		_req_fd = -1;
		if(_get_audio_suc)
		{
			Network::Close(_audio_fd);
			LOG(LEVEL_INFO, "ypeng@ TCP close, _audio_fd(%d).\n", _audio_fd);
			_audio_fd = -1;
		}
		for(int i = 0; i < STREAM_NUM; i++)
		{
			if(_video_fd_list[i] > 0)
			{
				Network::Close(_video_fd_list[i]);
				LOG(LEVEL_INFO, "ypeng@ TCP close, stream(%d), _video_fd(%d).\n", i+FIRST_STREAM_ID, _video_fd_list[i]);
				_video_fd_list[i] = -1;
			}
		}
	}

	bool Self_HIPC_RGB::videoproc(int streamID)
	{
		int ar_i = streamID - FIRST_STREAM_ID;
		HIPC_V_PACKET header;
		if(GetHeader(_video_fd_list[ar_i], &header) != RET_SUCCESS)
		{
			LOG(LEVEL_ERROR, "ypeng@ GetHeader failed!socket(%d), stream(%d).\n", _video_fd_list[ar_i], streamID);
			return false;
		}

		unsigned int offset = _video_buf_list[ar_i].size();
		_video_buf_list[ar_i].resize(offset + header.packet_size);

		memcpy(&(_video_buf_list[ar_i][offset]), header.frame_data, header.packet_size);

		if(_video_buf_list[ar_i].size() >= MAX_BUF)
		{ 
			return false;
		} 

		if(header.packet_last == 1)
		{
			if(_video_buf_list[ar_i].size() == header.frame_size)
			{
				VS_AREA valid;
				valid.x = header.valid_x;
				valid.y = header.valid_y;
				valid.w = header.valid_w;
				valid.h = header.valid_h;

				//LOG(LEVEL_INFO, "size %d, index %d, bool is %d.\n", _is_first_frame_list.size(), ar_i, _is_first_frame_list[ar_i]==true?1:0);

				if(_is_first_frame_list[ar_i])
				{
					//
					_resol_list[ar_i].w = header.all_w;
					_resol_list[ar_i].h = header.all_h;
					//LOG(LEVEL_INFO, "bbbbbbbbbb");
					//
					_is_first_frame_list[ar_i] = false;

					//
					//new_id();
					req_desc(streamID, _resol_list[ar_i], valid);

				}				

				if(_resol_list[ar_i].w != header.all_w || _resol_list[ar_i].h != header.all_h)
				{
					return false;
				}
#if 0
				static FILE* fp1 = fopen("D:\\UHI01.h264", "w+b");
				if(NULL == fp1)
				{
					goto SAVE_FAILURE;
				}
				static int fp1_cnt = 0;
				static bool start = false;
				if(fp1_cnt++ < 600)
				{
					if((!_firstIFrame_list[ar_i]) &&(1 == header.frame_type))
					{
						_firstIFrame_list[ar_i] = true;
						LOG(LEVEL_INFO, "ypeng@ stream(%d) has first I _frame.\n", streamID);
					}
					if(_firstIFrame_list[ar_i])
						fwrite(&(_video_buf_list[ar_i][0]), 1, _video_buf_list[ar_i].size(), fp1);
				}
				else if(fp1_cnt >= 600)
				{
					fflush(fp1);
					fclose(fp1);
					fp1 = NULL;
				}
			SAVE_FAILURE:
#endif
				if((!_firstIFrame_list[ar_i]) &&(1 == header.frame_type))
				{
					_firstIFrame_list[ar_i] = true;
					LOG(LEVEL_INFO, "ypeng@ stream(%d) has first I _frame.\n", streamID);
				}
				if(_firstIFrame_list[ar_i])
				{
					vFrameNum++;
					if(vFrameNum % 200 == 1)
					{
						LOG(LEVEL_INFO, "ypeng@ req_video, vFrameNum=%u, frameNum=%u, frameType=%u, timestamp=%u, size=%d, sliceSize=%u.\n",
							vFrameNum, header.frame_num, header.frame_type, header.timestamp, _video_buf_list[ar_i].size(), header.frame_size);
						//LOG(LEVEL_INFO, "vHeader's INFO: sync=%x, len=%u, frame_num=%u, frame_size=%u, stream_id=%u, ");
					}
					req_video(streamID, _resol_list[ar_i], valid, header.frame_type == 1 ? true : false, header.timestamp, _video_buf_list[ar_i].size(), &(_video_buf_list[ar_i][0]));
				}

			}

			_video_buf_list[ar_i].clear();

		}

		return true;
	}

	bool Self_HIPC_RGB::audioproc()
	{
		HIPC_A_PACKET header;
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
				aFrameNum++;
				if(aFrameNum % 200 == 1)
				{
					LOG(LEVEL_INFO, "ypeng@ req_audio, aFrameNum=%u, frameNum=%u, timestamp=%u, size=%d, sliceSize=%u.\n",
						aFrameNum, header.frame_num, header.timestamp, _audio_buf.size(), header.frame_size);
				}
				req_audio(header.timestamp, _audio_buf.size(), &_audio_buf[0]);
			}

			_audio_buf.clear();

		}

		return true;
	}

	void Self_HIPC_RGB::req_desc(const Uint8 streamID, const VS_RESOL& resol, const VS_AREA& valid)
	{
		_audiodesc->id = _audio_stream_id;
		_audiodesc->grade = 0;

		int ar_i = streamID - FIRST_STREAM_ID;
		_videodesc_list[ar_i]->id = _video_stream_id_list[ar_i];
		_videodesc_list[ar_i]->isaudio = 0;
		_videodesc_list[ar_i]->resol.w = valid.w;
		_videodesc_list[ar_i]->resol.h = valid.h;
		_videodesc_list[ar_i]->rows = 1;
		_videodesc_list[ar_i]->cols = 1;
		_videodesc_list[ar_i]->grade = grade(resol.w, resol.h);
		_videodesc_list[ar_i]->desc[0].id = _video_block_id_list[ar_i];
		_videodesc_list[ar_i]->desc[0].area = valid;
		_videodesc_list[ar_i]->desc[0].resol = resol;
		_videodesc_list[ar_i]->desc[0].valid = valid;

		//_bRes = true;

		int desc_cnt = 0;
		std::vector<bool>::iterator descIt = _is_first_frame_list.begin();
		std::vector<VS_VIDEO_STREAM_DESC*> videodesc_out;
		while(descIt != _is_first_frame_list.end())
		{
			if(!(*descIt))
			{
				desc_cnt++;
			}
			descIt++;
		}
		if(_stmCnt == desc_cnt)
		{
			for(int i = 0; i < STREAM_NUM; i++)
			{
				if(! _is_first_frame_list[i])//(_videodesc_list[i]->resol.w != 0 || _videodesc_list[i]->resol.h != 0)
				{
					videodesc_out.push_back(_videodesc_list[i]);
				}
			}

			if(_get_audio_suc)
			{
				Instance::req_desc(1, &_audiodesc, _stmCnt, &(videodesc_out[0]));
			}
			else
			{
				Instance::req_desc(0, NULL, _stmCnt, &(videodesc_out[0]));
			}
			LOG(LEVEL_INFO, "ypeng@ req_desc success, getaudio(%d), resol(%d, %d).\n", _get_audio_suc==true?1:0, _videodesc_list[0]->resol.w, _videodesc_list[0]->resol.h);
		}
	}

	void Self_HIPC_RGB::req_video(const Uint8 streamID, const VS_RESOL& resol, const VS_AREA& valid, bool isiframe, u_int32 timestamp, u_int32 datalen, unsigned char* data)
	{
		VideoData vdata;
		
		int ar_i = streamID - FIRST_STREAM_ID;
		vdata.stream = _video_stream_id_list[ar_i];
		vdata.block = _video_block_id_list[ar_i];
		vdata.stampId = _video_stamp_id_list[ar_i];
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

	void Self_HIPC_RGB::req_audio(u_int32 timestamp, u_int32 datalen, unsigned char* data)
	{
#if 0
			static FILE* aeFp = fopen("./uhi01.aac", "wb");
			static int aeCnt = 0;
			if(NULL == aeFp)
			{
				goto SAVE_FAIL;
			}
			aeCnt++;
			if(aeCnt < 100000)
			{
				fwrite(data, sizeof(char), datalen, aeFp);
			}
			else
			{
				fflush(aeFp);
				fclose(aeFp);
				aeFp = NULL;
			}
SAVE_FAIL:
#endif
		//if(!_bRes)
		//	return;

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
	
	bool Self_HIPC_RGB::get_video_rate(int sockfd, Uint8 streamID, int& rate)
	{
		T_HIPC_CMD_VIDEO_FRAME_RATE_REQ req = {0};
		req.head.sync = 0xA05050A1;
		req.head.msgType = 0x8211;
		req.head.len = sizeof(req) - sizeof(T_HIPC_NET_HEADER);
		req.sig_chan = _channel;
		req.streamID = streamID;

		if(Network::Sendn(sockfd, &req, sizeof(req)) != sizeof(req))
		{
			LOG(LEVEL_ERROR, "Send Packet failed!");
			return false;
		}

		fd_set fs;
		struct timeval tv = {1, 0};
		FD_ZERO(&fs);
		FD_SET(sockfd, &fs);
		if(Network::Select(FD_SETSIZE, &fs, NULL, NULL, &tv) <= 0)
		{
			LOG(LEVEL_ERROR, "ypeng@ failed, streamID(%d) no respond.\n", (int)streamID);
			return false;
		}
		
		if(Network::Recvn(sockfd, &req, sizeof(req.head)) != sizeof(req.head))
		{
			LOG(LEVEL_ERROR, "Get Packet failed");
			return false;
		}
		if(req.head.len <= 0)
		{
			LOG(LEVEL_ERROR, "Packet is empty!\n");
			return false;
		}
		else
		{
			if(Network::Recvn(sockfd, &req.sig_chan, req.head.len) != req.head.len)
			{
				LOG(LEVEL_ERROR, "Get Packet failed");
				return false;
			}
		}

		if(req.head.msgType != 0x8212 || req.head.errCode != 0 )
		{
			LOG(LEVEL_ERROR, "ypeng@ failed, msgType(%d), err(%d).\n", req.head.msgType, req.head.errCode);
			return false;
		}

		if(0 == req.framerate)
		{
			LOG(LEVEL_ERROR, "ypeng@ failed, stream(%d):frame_rate(%d).\n", streamID, (int)req.framerate);
			return false;
		}

		rate = req.framerate;
		LOG(LEVEL_INFO, "ypeng@ socket(%d) stream(%d):frame_rate(%d).\n", sockfd, streamID, rate);
		return true;
	}

	bool Self_HIPC_RGB::get_video_res(int sockfd, Uint8 streamID)
	{
		T_IPC_VIDEO_INFO_REQ req;
		memset(&req, 0, sizeof(T_IPC_VIDEO_INFO_REQ) );
		req.head.sync = 0xA05050A1;
		req.head.msgType = 0x8201;
		req.head.len = sizeof(req)-sizeof(T_HIPC_NET_HEADER);
		req.video_encode_type = 0;
		req.sig_chan = _channel;
		req.streamID = streamID;

		if(Network::Sendn(sockfd, &req, sizeof(req)) != sizeof(req))
		{
			LOG(LEVEL_ERROR, "Send Packet failed!");
			return false;
		}

		fd_set fs;
		struct timeval tv = {1, 0};
		FD_ZERO(&fs);
		FD_SET(sockfd, &fs);
		if(Network::Select(FD_SETSIZE, &fs, NULL, NULL, &tv) <= 0)
		{
			LOG(LEVEL_ERROR, "ypeng@ failed, streamID(%d) no respond.\n", (int)streamID);
			return false;
		}
		
		if(Network::Recvn(sockfd, &req, sizeof(req.head)) != sizeof(req.head))
		{
			LOG(LEVEL_ERROR, "Get Packet failed");
			return false;
		}
		if(req.head.len <= 0)
		{
			LOG(LEVEL_ERROR, "Packet is empty!\n");
			return false;
		}
		else
		{
			if(Network::Recvn(sockfd, &req.sig_chan, req.head.len) != req.head.len)
			{
				LOG(LEVEL_ERROR, "Get Packet failed");
				return false;
			}
		}

		if(req.head.msgType != 0x8202 || req.head.errCode != 0 )
		{
			LOG(LEVEL_ERROR, "ypeng@ failed, msgType(%d), err(%d).\n", req.head.msgType, req.head.errCode);
			return false;
		} 

		if((0 == req.width) || (0 == req.height))
		{
			LOG(LEVEL_ERROR, "ypeng@ failed, stream(%d):resol(%d, %d).\n", streamID, (int)req.width, (int)req.height);
			return false;
		}

		int ar_i = streamID - FIRST_STREAM_ID;
		VS_AREA buf_area = {0, 0, req.width, req.height};
		VS_RESOL buf_resol = {req.width, req.height};

		_videodesc_list[ar_i]->id = _video_stream_id_list[ar_i];
		_videodesc_list[ar_i]->isaudio = 0;
		_videodesc_list[ar_i]->resol.w = req.width;//valid.w;
		_videodesc_list[ar_i]->resol.h = req.height;//valid.h;
		_videodesc_list[ar_i]->rows = 1;
		_videodesc_list[ar_i]->cols = 1;
		_videodesc_list[ar_i]->grade = grade(req.width, req.height);
		_videodesc_list[ar_i]->desc[0].id = _video_block_id_list[ar_i];
		_videodesc_list[ar_i]->desc[0].area = buf_area;//valid;
		_videodesc_list[ar_i]->desc[0].resol = buf_resol;
		_videodesc_list[ar_i]->desc[0].valid = buf_area;

		LOG(LEVEL_INFO, "ypeng@ socket(%d) stream(%d):resol(%d, %d).\n", sockfd, streamID, _videodesc_list[ar_i]->resol.w, _videodesc_list[ar_i]->resol.h);
		return true;
	}

	bool Self_HIPC_RGB::get_audio_res(int sockfd)
	{
		T_IPC_AUDIO_INFO_REQ req;
		memset(&req, 0, sizeof(T_IPC_AUDIO_INFO_REQ));
		req.head.sync = 0xA05050A1;
		req.head.msgType = 0x8209;
		req.head.len = sizeof(req) - sizeof(T_IPC_AUDIO_INFO_REQ);
		req.sig_chan = _channel;

		if(Network::Sendn(sockfd, &req, sizeof(req)) != sizeof(req))
		{
			LOG(LEVEL_ERROR, "Send Packet failed");
			return false;
		}

		fd_set fs;
		struct timeval tv = {1, 0};
		FD_ZERO(&fs);
		FD_SET(sockfd, &fs);
		if(Network::Select(FD_SETSIZE, &fs, NULL, NULL, &tv) <= 0)
		{
			LOG(LEVEL_ERROR, "ypeng@ failed, no respond.\n");
			return false;
		}

		if(Network::Recvn(sockfd, &req, sizeof(req.head)) != sizeof(req.head))
		{
			LOG(LEVEL_ERROR, "Get Packet failed");
			return false;
		}
		if(req.head.len <= 0)
		{
			LOG(LEVEL_ERROR, "Packet is empty!\n");
			return false;
		}
		else
		{
			if(Network::Recvn(sockfd, &req.sig_chan, req.head.len) != req.head.len)
			{
				LOG(LEVEL_ERROR, "Get Packet failed");
				return false;
			}
		}
		
		if(req.head.msgType != 0x8210 || req.head.errCode != 0)
		{
			LOG(LEVEL_ERROR, "ypeng@ failed, msgType(%d), err(%d).\n", req.head.msgType, req.head.errCode);
			return false;
		} 

		if(req.audio_sample_bit == 0 || req.audio_sample_chan == 0 || req.audio_sample_rate == 0)
		{
			LOG(LEVEL_ERROR, "ypeng@ failed, audio_sample_bit(%d), audio_sample_chan(%d), audio_sample_rate(%d).\n"
				, (int)req.audio_sample_bit, (int)req.audio_sample_chan, (int)req.audio_sample_rate);
			return false;
		}
		
		_audiodesc->id				= _audio_stream_id;
		_audiodesc->codec			= (HIPC_CODEC_AAC == req.audio_encode_type) ? (CODEC_AAC):(CODEC_PCM);
		_audiodesc->samples			= req.audio_sample_rate;
		_audiodesc->bit_per_samples	= req.audio_sample_bit;
		_audiodesc->channels			= req.audio_sample_chan;
		_audiodesc->grade			= 0;

		LOG(LEVEL_INFO, "ypeng@ socket(%d) audiodesc:codec(%d):samples(%d):bit(%d):chan(%d).\n", sockfd, _audiodesc->codec, _audiodesc->samples, _audiodesc->bit_per_samples, _audiodesc->channels);
		return true;
	}

	bool Self_HIPC_RGB::get_video_port(int sockfd, Uint8 streamID, u_int16& port)
	{
		T_IPC_VIDEO_CONNECT_REQ req;
		memset(&req, 0, sizeof(T_IPC_VIDEO_CONNECT_REQ) );
		req.head.sync = 0xA05050A1;
		req.head.msgType = 0x8203;
		req.head.len = sizeof(req)-sizeof(T_HIPC_NET_HEADER);
		req.sig_chan = _channel;
		req.streamID = streamID;
		req.mode = 2;
		local(sockfd, sizeof(req.ipAddress), (char*)req.ipAddress);
		
		if(Network::Sendn(sockfd, &req, sizeof(req)) != sizeof(req))
		{
			LOG(LEVEL_ERROR, "Send Packet failed");
			return false;
		}

		fd_set fs;
		struct timeval tv = {1, 0};
		FD_ZERO(&fs);
		FD_SET(sockfd, &fs);
		if(Network::Select(FD_SETSIZE, &fs, NULL, NULL, &tv) <= 0)
		{
			LOG(LEVEL_ERROR, "ypeng@ failed, streamID(%d) no respond.\n", (int)streamID);
			return false;
		}
				
		if(Network::Recvn(sockfd, &req, sizeof(req.head)) != sizeof(req.head))
		{
			LOG(LEVEL_ERROR, "Get Packet failed");
			return false;
		}
		if(req.head.len <= 0)
		{
			LOG(LEVEL_ERROR, "Packet is empty!\n");
			return false;
		}
		else
		{
			if(Network::Recvn(sockfd, &req.sig_chan, req.head.len) != req.head.len)
			{
				LOG(LEVEL_ERROR, "Get Packet failed");
				return false;
			}
		}

		if(req.head.msgType != 0x8204 || req.head.errCode != 0 || req.port == 0 )
		{
			LOG(LEVEL_ERROR, "ypeng@ failed, msgType(%d), err(%d), port(%d).\n", req.head.msgType, req.head.errCode, req.port);
			return false;
		} 

		port  = req.port;
		LOG(LEVEL_INFO, "ypeng@socket(%d) stream(%d):port(%d).\n", sockfd, streamID, port);
		return true;
	}

	bool Self_HIPC_RGB::get_audio_port(int sockfd, Uint8 streamID, u_int16& port)
	{
		T_IPC_AUDIO_CONNECT_REQ	req;
		memset(&req, 0, sizeof(T_IPC_AUDIO_CONNECT_REQ) );
		req.head.sync = 0xA05050A1;
		req.head.msgType = 0x8300;
		req.head.len = sizeof(req)-sizeof(T_HIPC_NET_HEADER);
		req.sig_chan = _channel;
		req.audioID = streamID;
		req.mode = 2;
		local(sockfd, sizeof(req.ipAddress), (char*)req.ipAddress);

		if(Network::Sendn(sockfd, &req, sizeof(req)) != sizeof(req))
		{
			LOG(LEVEL_ERROR, "Send Packet failed");
			return false;
		}

		fd_set fs;
		struct timeval tv = {1, 0};
		FD_ZERO(&fs);
		FD_SET(sockfd, &fs);
		if(Network::Select(FD_SETSIZE, &fs, NULL, NULL, &tv) <= 0)
		{
			LOG(LEVEL_ERROR, "ypeng@ failed, streamID(%d) no respond.\n", (int)streamID);
			return false;
		}

		if(Network::Recvn(sockfd, &req, sizeof(req.head)) != sizeof(req.head))
		{
			LOG(LEVEL_ERROR, "Get Packet failed");
			return false;
		}
		if(req.head.len <= 0)
		{
			LOG(LEVEL_ERROR, "Packet is empty!\n");
			return false;
		}
		else
		{
			if(Network::Recvn(sockfd, &req.sig_chan, req.head.len) != req.head.len)
			{
				LOG(LEVEL_ERROR, "Get Packet failed");
				return false;
			}
		}

		if(req.head.msgType != 0x8301 || req.head.errCode != 0 || req.port == 0)
		{
			LOG(LEVEL_ERROR, "ypeng@socket(%d) failed, msgType(%d), err(%d), port(%d).\n", sockfd, req.head.msgType, req.head.errCode, req.port);
			return false;
		} 

		port  = req.port;
		LOG(LEVEL_INFO, "ypeng@ audio_port(%d)\n", port);

		return true;
	}

	void Self_HIPC_RGB::disconnect_video(int sockfd, Uint8 streamID)
	{
		T_IPC_VIDEO_DISCONNECT_REQ req;
		memset(&req, 0, sizeof(T_IPC_VIDEO_DISCONNECT_REQ) );
		req.head.sync = 0xA05050A1;
		req.head.msgType = 0x8205;
		req.head.len = sizeof(req)-sizeof(T_HIPC_NET_HEADER);
		req.sig_chan = _channel;
		req.streamID = streamID;
		req.mode = 2;
		local(sockfd, sizeof(req.ipAddress), (char*)req.ipAddress);

		if(Network::Sendn(sockfd, &req, sizeof(req)) != sizeof(req))
		{
			LOG(LEVEL_ERROR, "Send Packet failed");
			return;
		}

		fd_set fs;
		struct timeval tv = {1, 0};
		FD_ZERO(&fs);
		FD_SET(sockfd, &fs);
		if(Network::Select(FD_SETSIZE, &fs, NULL, NULL, &tv) <= 0)
		{
			LOG(LEVEL_ERROR, "ypeng@ failed, streamID(%d) no respond.\n", (int)streamID);
			return;
		}

		if(Network::Recvn(sockfd, &req, sizeof(req.head)) != sizeof(req.head))
		{
			LOG(LEVEL_ERROR, "Get Packet failed");
			return ;
		}
		if(req.head.len <= 0)
		{
			LOG(LEVEL_ERROR, "Packet is empty!\n");
			return ;
		}
		else
		{
			if(Network::Recvn(sockfd, &req.sig_chan, req.head.len) != req.head.len)
			{
				LOG(LEVEL_ERROR, "Get Packet failed");
				return ;
			}
		}

		if(req.head.msgType != 0x8206 || req.head.errCode != 0)
		{
			LOG(LEVEL_ERROR, "ypeng@ failed, msgType(%d), err(%d).\n", req.head.msgType, req.head.errCode);
			return;
		}

		LOG(LEVEL_INFO, "ypeng@ stream(%d) disconnect.\n", streamID);
	}

	void Self_HIPC_RGB::disconnect_audio(int sockfd, Uint8 streamID)
	{
		T_IPC_AUDIO_DISCONNECT_REQ req;
		memset(&req, 0, sizeof(T_IPC_AUDIO_DISCONNECT_REQ) );
		req.head.sync = 0xA05050A1;
		req.head.msgType = 0x8302;
		req.head.len = sizeof(req)-sizeof(T_HIPC_NET_HEADER);
		req.sig_chan	= _channel;
		req.audioID = streamID;
		req.mode = 2;
		local(sockfd, sizeof(req.ipAddress), (char*)req.ipAddress);

		if(Network::Sendn(sockfd, &req, sizeof(req)) != sizeof(req))
		{
			LOG(LEVEL_ERROR, "Send Packet failed");
			return;
		}

		fd_set fs;
		struct timeval tv = {1, 0};
		FD_ZERO(&fs);
		FD_SET(sockfd, &fs);
		if(Network::Select(FD_SETSIZE, &fs, NULL, NULL, &tv) <= 0)
		{
			LOG(LEVEL_ERROR, "ypeng@ failed, streamID(%d) no respond.\n", (int)streamID);
			return;
		}
		
		if(Network::Recvn(sockfd, &req, sizeof(req.head)) != sizeof(req.head))
		{
			LOG(LEVEL_ERROR, "Get Packet failed");
			return ;
		}
		if(req.head.len <= 0)
		{
			LOG(LEVEL_ERROR, "Packet is empty!\n");
			return ;
		}
		else
		{
			if(Network::Recvn(sockfd, &req.sig_chan, req.head.len) != req.head.len)
			{
				LOG(LEVEL_ERROR, "Get Packet failed");
				return ;
			}
		}

		if(req.head.msgType != 0x8303 || req.head.errCode != 0)
		{
			LOG(LEVEL_ERROR, "ypeng@ failed, msgType(%d), err(%d).\n", req.head.msgType, req.head.errCode);
			return;
		}

		LOG(LEVEL_INFO, "ypeng@ audio disconnect.\n");

	}

	bool Self_HIPC_RGB::make_i_frame(int sockfd, Uint8 streamID)
	{
		T_IPC_VIDEO_I_FRAME_REQ	req;
		memset(&req, 0, sizeof(T_IPC_VIDEO_I_FRAME_REQ) );
		req.head.sync = 0xA05050A1;
		req.head.msgType = 0x8207;
		req.head.len = sizeof(req)-sizeof(T_HIPC_NET_HEADER);
		req.sig_chan = _channel;
		req.streamID = streamID;

		if(Network::Sendn(sockfd, &req, sizeof(req)) != sizeof(req))
		{
			LOG(LEVEL_ERROR, "Send Packet failed");
			return false;
		}

		fd_set fs;
		struct timeval tv = {1, 0};
		FD_ZERO(&fs);
		FD_SET(sockfd, &fs);
		if(Network::Select(FD_SETSIZE, &fs, NULL, NULL, &tv) <= 0)
		{
			LOG(LEVEL_ERROR, "ypeng@ failed, streamID(%d) no respond.\n", (int)streamID);
			return false;
		}
		
		if(Network::Recvn(sockfd, &req, sizeof(req.head)) != sizeof(req.head))
		{
			LOG(LEVEL_ERROR, "Get Packet failed");
			return false;
		}
		if(req.head.len <= 0)
		{
			LOG(LEVEL_ERROR, "Packet is empty!\n");
			return false;
		}
		else
		{
			if(Network::Recvn(sockfd, &req.sig_chan, req.head.len) != req.head.len)
			{
				LOG(LEVEL_ERROR, "Get Packet failed");
				return false;
			}
		}

		if(req.head.msgType != 0x8208 || req.head.errCode != 0)
		{
			LOG(LEVEL_ERROR, "ypeng@ failed, stream(%d), msgType(%d), err(%d).\n", (int)streamID, req.head.msgType, req.head.errCode);
			return false;
		} 

		LOG(LEVEL_INFO, "ypeng@ Make I _frame success. stream(%d).\n", streamID);
		return true;
	}
	

int	Self_HIPC_RGB::sumObj = 0;

void* imp_uhi01_open(const VS_SIGNAL* signal, SigCB cb, void* context, const Uuid& item)
{
	Self_HIPC_RGB* i = new Self_HIPC_RGB(signal, cb, context, item);
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
int   imp_uhi01_close(void* handle)
{
	Self_HIPC_RGB* i = reinterpret_cast<Self_HIPC_RGB*>(handle);
	i->exit();
	return 0l;
}

int   imp_uhi01_set_i_frame(void* handle)
{
	Self_HIPC_RGB* i = reinterpret_cast<Self_HIPC_RGB*>(handle);
	i->set_i_frame();
	return 0l;
}

int   imp_uhi01_error(void* handle)
{
	Self_HIPC_RGB* i = reinterpret_cast<Self_HIPC_RGB*>(handle);
	return i->geterror();
}
