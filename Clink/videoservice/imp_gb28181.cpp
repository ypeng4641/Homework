#include "imp_gb28181.h"

#include "h264.h"
#include "put_bits.h"

#include <Iphlpapi.h>
#pragma comment(lib, "Iphlpapi.lib")

#include <string>
#include <sstream>
using namespace std;


int	Self_GB28181::sumObj			= 0;


unsigned int Self_GB28181::GetFrameType(unsigned char* data, int size, unsigned int codec)
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
					break;
				case 8:
					break;
				default:
					LOG(LEVEL_INFO, "default: ");
					break;
				}
			}
			else if(loopsize > 1024)
			{
				break;
			}

			data++;
		}		

		//return 0;//STREAM_IFRAME;
		return -2; //不能识别的帧类型，一般发生在I帧之后，应该是I帧太大导致I帧被分包所致
	}

	return -1;//STREAM_UNTYPE;
}

	void Self_GB28181::req_desc(void)
	{
		if (_is_audio_valid && _is_video_valid)
		{
			Instance::req_desc(1, &_audiodesc, 1, &_videodesc);
			return;
		}

		if (_is_audio_valid)
		{
			Instance::req_desc(1, &_audiodesc, 0, NULL);
			return;
		}

		if (_is_video_valid)
		{
			Instance::req_desc(0, NULL, 1, &_videodesc);
			return;
		}
	}

	void Self_GB28181::req_audio(u_int32 timestamp, u_int32 datalen, unsigned char* data)
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

	void Self_GB28181::req_video(const VS_RESOL& resol, const VS_AREA& valid, bool isiframe, u_int32 timestamp, u_int32 datalen, unsigned char* data)
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


void* imp_gb28181_open(const VS_SIGNAL* signal, SigCB cb, void* context, const Uuid& item)
{
	Self_GB28181* i = new Self_GB28181(signal, cb, context, item);
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
int   imp_gb28181_close(void* handle)
{
	Self_GB28181* i = reinterpret_cast<Self_GB28181*>(handle);
	i->exit();
	return 0l;
}

int   imp_gb28181_set_i_frame(void* handle)
{
	Self_GB28181* i = reinterpret_cast<Self_GB28181*>(handle);
	i->set_i_frame();
	return 0l;
}

int   imp_gb28181_error(void* handle)
{
	Self_GB28181* i = reinterpret_cast<Self_GB28181*>(handle);
	return i->geterror();
}


bool Self_GB28181::GetInfOfSignal(PARAMS_DEF_GB28181_t& paraDef)
{
	//
	const VS_SIGNAL* p = reinterpret_cast<const VS_SIGNAL*>(signal());
	if(p->datalen != sizeof(ExtInfSignal))
	{
		LOG(LEVEL_ERROR, "Self_GB28181::GetInfOfSignal().....p->datalen= %d, sizeof(ExtInfSignal)= %d\n", p->datalen, sizeof(ExtInfSignal));
		return false;
	}
		
	ExtInfSignal extInf;
	memcpy(&extInf, p->data, p->datalen);

	char ip[32];
	host_ultoa(ipaddr(), sizeof(ip), ip);
	memcpy(paraDef.localIP, ip, sizeof(ip));
	paraDef.port = port();

	////test
	//memcpy(paraDef.localIP, "172.16.129.2", sizeof(ip));
	//paraDef.port = 6070;

	memcpy(paraDef.ProxyServer, extInf.ProxyServer, 32*sizeof(char));
	memcpy(paraDef.userName, extInf.userName, 32*sizeof(char));
	memcpy(paraDef.passWd, extInf.passWd, 32*sizeof(char));
	memcpy(paraDef.deviceId, extInf.deviceId, 32*sizeof(char));
	memcpy(paraDef.platID, extInf.platID, 32*sizeof(char));
	memcpy(paraDef.platIP, extInf.platIP, 32*sizeof(char));

	LOG(LEVEL_INFO, "Self_GB28181::GetInfOfSignal().....sizeof(ExtInfSignal)= %d\r\n", sizeof(ExtInfSignal));

	LOG(LEVEL_INFO, "Self_GB28181::GetInfOfSignal().....paraDef.localIP= %s, paraDef.port= %d\r\n", paraDef.localIP, paraDef.port);
	LOG(LEVEL_INFO, "Self_GB28181::GetInfOfSignal().....paraDef.ProxyServer= %s\r\n", paraDef.ProxyServer);
	LOG(LEVEL_INFO, "Self_GB28181::GetInfOfSignal().....paraDef.userName= %s\r\n", paraDef.userName);
	LOG(LEVEL_INFO, "Self_GB28181::GetInfOfSignal().....paraDef.passWd= %s\r\n", paraDef.passWd);
	LOG(LEVEL_INFO, "Self_GB28181::GetInfOfSignal().....paraDef.deviceId= %s\r\n", paraDef.deviceId);
	LOG(LEVEL_INFO, "Self_GB28181::GetInfOfSignal().....paraDef.platID= %s\r\n", paraDef.platID);
	LOG(LEVEL_INFO, "Self_GB28181::GetInfOfSignal().....paraDef.platIP= %s\r\n", paraDef.platIP);

	return true;
}




/*----------------------- CSIP_SDK ------------------------*/
#if CSIP_SDK
int Self_GB28181::SDK_Login()
{
	int ret = 0;

	GetInfOfSignal(_paraDef);	
	connectMsg cM = {0};
	strcpy(cM.GBIP, _paraDef.localIP);
	cM.GBPort = _paraDef.port;
	strcpy(cM.usr, _paraDef.userName);
	strcpy(cM.pwd, _paraDef.passWd);
	strcpy(cM.localIP, _paraDef.platIP);

	ret = GB28181_Connect(_serverHd, cM, ciCB, eCB, (void*)this);	
	GetCurTime(m_ui64RecvTimePre);

	LOG(LEVEL_INFO, "SDK_Login().... ret = %d", ret);
	return ret;
}

int	Self_GB28181::SDK_Logout()
{
	LOG(LEVEL_INFO, "SDK_Logout().... ");
	return GB28181_Disconnect(_serverHd);
}

int Self_GB28181::SDK_Play()
{
	int ret = 0;
	//实时点播
	channelInfo ci = {0};
	strcpy(ci.destID, _paraDef.deviceId);
	strcpy(ci.subID, _paraDef.platID);
	strcpy(ci.subIP, _paraDef.platIP);
	ci.subPort = SUB_PORT;

	ret = GB28181_PlayStream(_serverHd, ci, GB28181_MEDIA_ES, mCB, (void*)this, _realHd);
	GetCurTime(m_ui64RecvTimePre);

	LOG(LEVEL_INFO, "SDK_Play().... ret = %d", ret);	
	return ret;
}

int Self_GB28181::SDK_Stop()
{
	LOG(LEVEL_INFO, "SDK_Stop().... ");
	return GB28181_StopStream(_realHd);
}


bool Self_GB28181::SDK_videoproc(const long realHd, mediaInfo* mI, char* data, int size)
{	
	if ((size<=0) || (data==NULL))
	{
		LOG(LEVEL_ERROR, "Self_GB28181::SDK__videoproc()...... if ((size<=0) || (data==NULL)) \n");
		return false;
	}

	int codecType = GetCodec(NULL, "H264");
	if(codecType < 0)
	{
		LOG(LEVEL_ERROR, "Self_GB28181::SDK__videoproc()...... GetCodec() failed. \n");
		req_error(XVIDEO_PLUG_RECVVIDEO_ERR);
		return false;
	}

	int subtype = GetFrameType((unsigned char*)data, size, codecType);
	if(subtype == -1)
	{
		LOG(LEVEL_ERROR, "Self_GB28181::SDK__videoproc()...... GetFrameType() failed. \n");
		req_error(XVIDEO_PLUG_RECVVIDEO_ERR);
		return false;
	}

	if (subtype != -2)
	{
		memcpy(frameData, data, size);
		uiFrameLen = size;
	}	

	if (!_is_video_valid)
	{
		{
			uint16_t w = 0;
			uint16_t h = 0;
			if (!H264Parse::GetResolution((unsigned char*)data, size, &w, &h))
			{
				LOG(LEVEL_ERROR, "h264_decode_seq_parameter_set failed!");
				req_error(XVIDEO_PLUG_RECVVIDEO_ERR);
				return false;
			}
			_resol.w = w;
			_resol.h = h;
			//_resol.w = mI->param.vInfo.width;
			//_resol.h = mI->param.vInfo.height;

			/*		_resol.w = 1920;
			_resol.h = 1090;*/

			LOG(LEVEL_INFO, "SDK__videoproc() _resol.w= %u, _resol.h = %u \n", _resol.w, _resol.h);
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

	//LOG(LEVEL_INFO, "_timestamp = %u", _timestamp);

#if 0//写文件， h264

	static FILE* fp3 = fopen("c:\\28181\\h264_sdk_videoproc.h264","w+b");
	fwrite(data,1, size, fp3);
	fflush(fp3);
#endif


	//VS_AREA valid = {0, 0, _resol.w, _resol.h};
	//req_video(_resol, valid, (0==subtype)?true:false, mI->param.vInfo.stamp, size, (unsigned char*)data);	

	if (subtype != -2)
	{
		VS_AREA valid = {0, 0, _resol.w, _resol.h};
		req_video(_resol, valid, (0==subtype)?true:false, mI->param.vInfo.stamp, size, (unsigned char*)data);	
	}
	else
	{
		memcpy(frameData+uiFrameLen, data, size);
		uiFrameLen += size;

		VS_AREA valid = {0, 0, _resol.w, _resol.h};
		req_video(_resol, valid, (0==subtype)?true:false, mI->param.vInfo.stamp, uiFrameLen, (unsigned char*)frameData);	
	}

	return true;
}


bool Self_GB28181::IsIDRFrame()
{
	return m_bIDRFrame;
}

void Self_GB28181::CheckIDRFrame(char* data, unsigned int datalen)
{
	unsigned int i = 0;
	for (i=0; i<datalen; i++)
	{
		//if(data[0]=='\x00' && data[1]=='\x00' && data[2]=='\x00' && data[3]=='\x01' && data[4]=='\x67')
		//if(((data[i]&0xff)==0) && ((data[i+1]&0xff)==0) && ((data[i+2]&0xff)==0) && ((data[i+3]&0xff)==1) &&((data[i+4]&0xff)==103))
		if(((data[i])==0x00) && ((data[i+1])==0x00) && ((data[i+2])==0x00) && ((data[i+3])==0x01) &&((data[i+4])==0x67))
		{
			m_bIDRFrame = true;
			break;
		}
	}


	//int subtype = GetFrameType((unsigned char*)data, datalen, CODEC_H264);
	//if(subtype == 0)
	//{
	//	m_bIDRFrame = true;
	//}
}

void	Self_GB28181::GetCurTime(unsigned __int64 &arg)
{
	clock_t cBuf;
	cBuf = clock();
	if(-1 == cBuf)
	{
		LOG(LEVEL_INFO, "clock() failed.");
	}
	else
	{
		arg = (unsigned __int64)cBuf;
	}
}

bool	Self_GB28181::CheckData()
{	
	unsigned __int64 ui64CurTime;
	GetCurTime(ui64CurTime);

	int diff = (int)(ui64CurTime - m_ui64RecvTimePre);
	if (diff > 6000) //6秒钟收不到数据，认为平台或者信号设备出故障
	{
		LOG(LEVEL_ERROR, "The Upper platform or signal device has been broken, please check them.");
		return false;
	}

	return true;
}

void Self_GB28181::ciCB(const long serverHd, const catalog* cIL, void* parameter)
{
	//printf("sever(%ld) has an catalog message. len=%d.\n", serverHd, cIL->length);
	LOG(LEVEL_INFO, "sever(%ld) has an catalog message. len=%d.\n", serverHd, cIL->length);
}

void Self_GB28181::eCB(const long serverHd, const errorInfo* eI, void* parameter)
{
	//printf("realPlay(%ld) of server(%ld) has an error. err=%d.\n", eI->realHd, serverHd, eI->errorID);
	LOG(LEVEL_ERROR, "realPlay(%ld) of server(%ld) has an error. err=%d.\n", eI->realHd, serverHd, eI->errorID);
}

void Self_GB28181::mCB(const long realHd, mediaInfo* mI, char* data, int dataLen, void* parameter)
{
	//LOG(LEVEL_INFO, "Self_GB28181::mCB().......");
	bool ret = false;

	Self_GB28181* plugin = (Self_GB28181*)parameter;
	plugin->GetCurTime(plugin->m_ui64RecvTimePre);

	if (mI->type != mediaInfo::GB28181_video)
	{
		LOG(LEVEL_ERROR, "Not video stream. mI->type= %d", mI->type);
		return;
	}

	if ((dataLen==0) || (data==NULL))
	{
		LOG(LEVEL_ERROR, "Not Data. if ((dataLen==0)) || (data==NULL))");
		return;
	}
	

	if (!plugin->IsIDRFrame())
	{
		plugin->CheckIDRFrame(data, dataLen);
	}
	if (plugin->IsIDRFrame())
	{
#if 0
		static FILE* fp = fopen("c:\\28181\\h264_sdkmCB.h264","w+b");
		fwrite(data,1, dataLen, fp);
		fflush(fp);

#endif
		
		ret = plugin->SDK_videoproc(realHd, mI, data, dataLen);
		if (!ret)
		{
			LOG(LEVEL_INFO, "plugin->SDK__videoproc() failed.");
		}
	}		
}

#endif