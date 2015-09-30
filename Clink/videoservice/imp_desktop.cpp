#include "imp_desktop.h"
#include "imp_ipms_play.h"
#include "IpmsNetSDK.h"
#include <error/xerror.h>
#include <utils/time.h>
#include <list>
#include <map>
#include <math.h>
#include <time.h>
#include "xprotocol/xprotocol.h"
#include "vscreen/vsprotocol.h"
#include "vscreen/shmem.h"
//#include "rawdecode-sdk.h"
static bool _hasConnect = false;
static int _objCount = 0;
static pthread_mutex_t g_conn_mutex = PTHREAD_MUTEX_INITIALIZER;
class Self_Clink_Play: public Instance
{
private:
#pragma pack(push)
#pragma pack(1)
	struct DefExt
	{
		char	proxy_for_xlan[32];
		u_int32	channel;
		u_int8  streamType;
		u_int8  row;
		u_int8  col;
	};
#pragma pack(pop)

public:
	Self_Clink_Play(const VS_SIGNAL* signal, SigCB cb, void* context, const Uuid& item): Instance(signal, cb, context, item),
		_bHaveData(false),
		_buf1(NULL),
		_buf2(NULL)
	{
		if(!getext(_defext))
		{
			memset(&_defext, 0, sizeof(_defext));
		}
		_objCount++;
		LOG(LEVEL_DEBUG,"Construct Objective Num = %d",_objCount);
	//	_hasConnect = false;
		_sock = -1;
	}

	~Self_Clink_Play()
	{
		if(_buf1)
		{
			delete[] _buf1;
		}

		if(_buf2)
		{
			delete[] _buf2;
		}

		_objCount--;
		LOG(LEVEL_DEBUG,"DeConstruct Objective Num = %d",_objCount);

	}
public:
	bool getLinkState()
	{
		return _hasConnect;
	}

private:
	bool getext(DefExt& ext)
	{
		//
		const VS_SIGNAL* p = reinterpret_cast<const VS_SIGNAL*>(signal());
		if(p->datalen != sizeof(DefExt))
		{
			LOG(LEVEL_ERROR,"ssm::p->datalen=%d,sizeof(DefExt)=%d",p->datalen,sizeof(DefExt));
			return false;
		}
		// 
		memcpy(&ext, p->data, p->datalen);
		return true;

	}

private:
	void new_id()
	{
		_video_stream_id.Generate();
		_audio_stamp_id.Generate();

		_stream2id.clear();
	}

	Uuid new_stamp_id()
	{
		Uuid id;
		id.Generate();
		return id;
	}

	Uuid new_id(int stream)
	{
		Uuid id;
		id.Generate();

		_stream2id.insert(std::make_pair(stream, id));
		return id;
	}

	bool get_id(int stream, Uuid& id)
	{
		std::map<int, Uuid>::iterator it = _stream2id.find(stream);
		if(it != _stream2id.end())
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
				if(0/*streams[i].format == IPMS_MEDIA_FMT_G711*/)
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
		LOG(LEVEL_INFO,"11");
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
		LOG(LEVEL_INFO,"22");
		r = (u_int8)(1 / base.video.region.h + 0.5);
		c = (u_int8)(1 / base.video.region.w + 0.5);

		LOG(LEVEL_INFO,"r=%d,c=%d\n",r,c);

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
	void videoproc(IPMS_FRAME_t* frame,int index)
	{
		//
		Uuid id;
		if(!get_id(frame->stream_id, id))
		{
			LOG(LEVEL_ERROR,"ssm:::frame->stream_id!=id!!!!,frameid=%d\n",frame->stream_id);
			return;
		}

		//
		VideoData vdata;

		vdata.stream = _video_stream_id;
		vdata.block = id;
		vdata.stampId = _video_stamp_id.at(index);

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

		vdata.codec = frame->media_fmt;// == IPMS_MEDIA_FMT_H264 ? CODEC_H264 : CODEC_MPEG2;

		vdata.timestamp = frame->timestamp;
		vdata.useVirtualStamp = false;
		vdata.datalen = frame->frame_size;
		vdata.data = (char*)frame->frame_data;
	//	LOG(LEVEL_INFO,"SendData Len=%d",vdata.datalen);
		Instance::req_video(&vdata);
	}

	void audioproc(IPMS_FRAME_t* frame)
	{
		//
		Uuid id;
		if(!get_id(frame->stream_id, id))
		{
			return;
		}// 
		AudioData adata;
		adata.stream = id;
		adata.stampId = _audio_stamp_id;
		adata.timestamp = frame->timestamp;
		adata.codec = CODEC_G711;
		adata.samples = frame->audio.sample_rate;
		adata.channels = frame->audio.channels;
		adata.bit_per_samples = frame->audio.bits_per_sample;
		adata.datalen = frame->frame_size;
		adata.data = (char*)frame->frame_data;

		//
		Instance::req_audio(&adata);

	}

private:
	void OnStream(int stream_num, IPMS_STREAM_t streams[])
	{		
		if(_buf1)
		{
			delete[] _buf1;
			_buf1 = NULL;
		}

		// 
		if(_buf2)
		{
			delete[] _buf2;
			_buf2 = NULL;
		}

		// 获取有效的流描述
		std::list<IPMS_STREAM_t> audio_list, video_list;
		get(stream_num, streams, audio_list, video_list);

		// 计算IPMS通道描述细节， 不完整的话不上报码流描述
		if(!get(video_list, _resol, _rows, _cols, _grade))
		{
			return;
		}
		//
		new_id();
		// 分配描述所使用的内存，当前IPMS只有一个码流，但可能有多个块
		unsigned int len1 = audio_list.size() * sizeof(VS_AUDIO_STREAM_DESC);
		unsigned int len2 = sizeof(VS_VIDEO_STREAM_DESC) + video_list.size() * sizeof(VS_BLOCK_DESC);

		_buf1 = new char[len1];
		_buf2 = new char[len2];

		// 填充音频结构
		unsigned int audio_index = 0;
		VS_AUDIO_STREAM_DESC* p1 = reinterpret_cast<VS_AUDIO_STREAM_DESC*>(_buf1);
		for(std::list<IPMS_STREAM_t>::iterator it = audio_list.begin();
			it != audio_list.end();
			it++)
		{
			//
			p1[audio_index].id = new_id(it->stream_id);
			p1[audio_index].codec = CODEC_G711;
			p1[audio_index].samples = it->audio.sample_rate;
			p1[audio_index].channels = it->audio.channels;
			p1[audio_index].bit_per_samples = it->audio.bits_per_sample;
			p1[audio_index].grade = 0;

			//
			audio_index++;
		}

		// 填充视频结构
		VS_VIDEO_STREAM_DESC* p2 = reinterpret_cast<VS_VIDEO_STREAM_DESC*>(_buf2);
		p2->id = _video_stream_id;
		p2->isaudio = 0;
		p2->resol = _resol;
		p2->rows = _rows;
		p2->cols = _cols;
		p2->grade = _grade;

		for(std::list<IPMS_STREAM_t>::iterator it = video_list.begin();it != video_list.end();it++)
		{
			u_int8 row = 0;
			u_int8 col = 0;

			get(*it, row, col);

			VS_BLOCK_DESC* p = &p2->desc[row * _cols + col];

			//
			p->id = new_id(it->stream_id);

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
		Instance::req_desc(audio_list.size(), &p1, 1, &p2);
	}

private:
	void OnFrame(IPMS_FRAME_t* frame,int index)
	{
	//	LOG(LEVEL_INFO,"ssm::OnFrame!");
#if 0
		char name[10]="";
		sprintf(name,"D:\\%d.h264",index);
		FILE *sp = fopen(name,"ab");
		fwrite(frame->frame_data,1,frame->frame_size,sp);
		fclose(sp);
#endif
		if(frame->is_video != 0)		// 视频
		{
			_bHaveData = true;
			videoproc(frame,index);
		}
		else						// 音频
		{
			_bHaveData = true;
			audioproc(frame);
		}		
	}


private:
	u_int64 GetCurTime()
	{
		struct timeval tv;
		gettimeofday(&tv, NULL);
		return ((ULONGLONG)tv.tv_sec * 1000 + tv.tv_usec/1000);		
	}

private:
	void TestReadFile()
	{
		//1.视频描述信息
		LOG(LEVEL_INFO,"Begin To Read File!\n");
		int streamNum = 2;
		IPMS_STREAM_t streams[2];
		for(int index=0;index<streamNum;index++)
		{
			streams[index].stream_id = 0+5*index;
			streams[index].is_video = 1;
			streams[index].format = IPMS_MEDIA_FMT_H264;
	//		streams[index].video.frame_type = IPMS_FRAME_IFRAME;
			streams[index].video.region.h = 1.0;
			streams[index].video.region.w = 0.5;
			streams[index].video.region.x = 0.5*index;
			streams[index].video.region.y = 0;
			streams[index].video.width = 1920;
			streams[index].video.height = 1080;
			streams[index].video.clip.h = 1.0;
			streams[index].video.clip.w = 1.0;
			streams[index].video.clip.x = 0;
			streams[index].video.clip.y = 0;
		}
		OnStream(streamNum,streams);
		Sleep(50);
		//2.视频流
		//读取本地文件，获取码流
		char *filename = "E:/IPProject/x1000FFmpeg/1920x1080-tokyo.h264";
		char *fileDesc = "E:/IPProject/x1000FFmpeg/1920x1080-tokyo.h264.hdr";
		FILE *fp = fopen(filename,"rb");
		FILE *fp2 = fopen(fileDesc,"rb");
		if(NULL==fp||NULL==fp2)
		{
			LOG(LEVEL_ERROR,"Read 264 file error!\n");
			return ;
		}

		for(int index=0;index<2;index++)
		{
			Uuid id;
			id = new_stamp_id();
			_video_stamp_id.push_back(id);
		}
		
		while(!feof(fp2))
		{
			//if(!feof(fp2))
			//{
			//	fseek(fp2,0,SEEK_SET);
			//}
#define MAX_SIZE      300000
			unsigned int length = 0;
			char buf[20] = {0};
			char buf_data[MAX_SIZE] = {0};

			fgets(buf,20,fp2);
			length = atoi(buf);
			fread(buf_data,1,length,fp);

			IPMS_FRAME_TYPE_t frametype;
			if((buf_data[4]&0x1f)==7)
			{
				frametype = IPMS_FRAME_IFRAME;
			}
			else if((buf_data[4]&0x1f)==6)
			{
				frametype = IPMS_FRAME_BFRAME;
			}
			else if((buf_data[4]&0x1f)==1)
			{
				frametype = IPMS_FRAME_PFRAME;
			}
			else
			{
				LOG(LEVEL_INFO,"ssm::Undefined Frame!\n");
			}
			static ULONGLONG stamp = 0;
			static ULONGLONG srcStamp = stamp;
			stamp = GetCurTime();


			for(int index =0;index<2;index++)
			{
				IPMS_FRAME_t* frame = (IPMS_FRAME_t*)new char[sizeof(IPMS_FRAME_t)];
				memset(frame,0,sizeof(IPMS_FRAME_t));
				frame->stream_id = 0+5*index;
				frame->media_fmt = IPMS_MEDIA_FMT_H264;
				frame->is_video = 1;
				frame->timestamp =  stamp;
				frame->video.height = 1080;
				frame->video.width = 1920;
				frame->video.region.h = 1.0;
				frame->video.region.w = 0.5;
				frame->video.region.x = 0.5*index;
				frame->video.region.y = 0;
				frame->video.clip.h = 1.0;
				frame->video.clip.w = 1.0;
				frame->video.clip.x = 0;
				frame->video.clip.y = 0;
				frame->video.frame_type = frametype;
				frame->frame_data = (unsigned char*)buf_data;				
				frame->frame_size = length;
				OnFrame(frame,index);				
				delete[] frame;
				frame = NULL;				
			}
			Sleep(100);
		}
	}
private:
	int Connect()
	{
		pthread_mutex_lock(&g_conn_mutex);
		if(!_hasConnect)
		{
			_sock = Network::MakeTCPClient("127.0.0.1",13380);
			if(_sock<0)
			{
				LOG(LEVEL_ERROR,"ssm::Make TCP Client Error!\n");
			}
			else
			{
				_hasConnect = true;
				LOG(LEVEL_INFO,"ssm::Make TCP Client Right,sock=%d\n",_sock);
			}
		}
		pthread_mutex_unlock(&g_conn_mutex);
		return _sock;
	}

	void Disconnect()
	{
		if(_sock >= 0)
		{
			Network::Close(_sock);
			_sock = -1;
			_hasConnect = false;
		}
	}

private:
	bool SendPkg(int sock, int subtype, void* data, int datalen)
	{
		int pkglen = sizeof(X_PACKAGE_t) + datalen;
		X_PACKAGE_t* pkg = (X_PACKAGE_t*)malloc(pkglen);
		memset(pkg, 0, pkglen);

		pkg->sequence = g_pkgnum++;
		pkg->datalen  = sizeof(DT_LOGIN_t);
		pkg->type	  = DT_COMMAND;
		pkg->subtype  = subtype;
		pkg->version  = 0x10;

		memcpy(pkg->data, data, datalen);

		if (RET_SUCCESS != XProtocol::SendPackage(sock, pkg, pkglen))
		{
			free(pkg);
			return false;
		}

		free(pkg);
		return true;
	}
private:
	bool Login(int sock)
	{
		DT_LOGIN_t login;
		login.streamType = _defext.streamType;
		login.row = _defext.row;
		login.col = _defext.col;
		strcpy_s(login.username, "vtron");
		strcpy_s(login.passwd, "digicom");
		LOG(LEVEL_INFO,"ssm::StreamType=%d,row=%d,col=%d\n",login.streamType,login.row,login.col);
		if (!SendPkg(sock, DT_CMD_LOGIN, &login, sizeof(login)))
		{
			return false;
		}

		int respLen = XProtocol::ReadPackLen(sock);
		if (respLen != (sizeof(X_PACKAGE_t) + sizeof(DT_STATUS_t)))
		{
			printf("Packet len:%d", respLen);
			return false;
		}

		X_PACKAGE_t* pkg     = (X_PACKAGE_t*)malloc(respLen);
		DT_STATUS_t* loginst = (DT_STATUS_t*)pkg->data;

		if ( RET_SUCCESS != XProtocol::GetPackage(sock, pkg, respLen))
		{
			free(pkg);
			return false;
		}

		if (loginst->code != 0)
		{
			printf("Login failed.\n");
			free(pkg);
			return false;
		}

		free(pkg);
		return true;
	}

	void Logout()
	{
		//
	}

	bool Play()
	{
		int interval = GetFrameInterval(_sock);				
		//if(!SetFrameInterval(_sock, FRAMEINTER))
		//{
		//	LOG(LEVEL_WARNING,"Set Frame Interval Failed!\n");
		//}
		if(GetWallConfig(_sock,_row,_col,_resv,_resh))
		{
			////报描述信息
			SendDescInfo(_row,_col,_resv,_resh);
			
			MakeStampID(_row,_col);
			

			SendPkg(_sock, DT_CMD_PLAYSTREAM, NULL, 0);

			int respLen = XProtocol::ReadPackLen(_sock);
			if (respLen == (sizeof(X_PACKAGE_t) + sizeof(DT_STATUS_t)))
			{
				X_PACKAGE_t* pkg = (X_PACKAGE_t*)malloc(respLen);
				DT_STATUS_t* playStatus = (DT_STATUS_t*)pkg->data;

				XProtocol::GetPackage(_sock, pkg, respLen);
				if (playStatus->code != 0)
				{
					printf("Play stream failed.\n");
					free(pkg);
					return false;
				}
				free(pkg);
			}

			return true;
		}

		return false;
	}
	
	void Stop()
	{
		//关闭码流
		SendPkg(_sock, DT_CMD_STOPSTREAM, NULL, 0);	
	}

private:
	u_int32 GetFrameInterval(int sock)
	{
		if (!SendPkg(sock, DT_CMD_GET_FRAME_INTERVAL, NULL, 0))
		{
			return -1;
		}

		int respLen = XProtocol::ReadPackLen(sock);
		X_PACKAGE_t* pkg = (X_PACKAGE_t*)malloc(respLen);

		if (RET_SUCCESS != XProtocol::GetPackage(sock, pkg, respLen))
		{
			free(pkg);
			return -1;
		}

		DT_FRAME_INTERVAL_t* inter = (DT_FRAME_INTERVAL_t*)pkg->data;
		u_int32 interval = inter->millisecond;
		free(pkg);
		return interval;
	}

private:
	/*********************************
	*设置帧间距，inter：帧间距，单位毫秒
	**********************************/
	bool SetFrameInterval(const int sock, u_int16 inter)
	{
		if (inter < 40)		//1000/40 = 25fps
		{
			printf("Should no more than 25 frame per second.");
			return false;
		}

		DT_FRAME_INTERVAL_t frameInter;
		frameInter.millisecond = inter;

		if (!SendPkg(sock, DT_CMD_SET_FRAME_INTERVAL, &frameInter, sizeof(frameInter)))
		{
			printf("Send request failed.\n");
			return false;
		}

		int respLen = XProtocol::ReadPackLen(sock);
		X_PACKAGE_t* pkg = (X_PACKAGE_t*)malloc(respLen);

		if (RET_SUCCESS != XProtocol::GetPackage(sock, pkg, respLen))
		{
			free(pkg);
			return false;
		}

		DT_STATUS_t* status = (DT_STATUS_t*)pkg->data;
		if ( 0 != status->code)
		{
			free(pkg);
			return false;
		}

		free(pkg);
		return true;
	}
private:
	bool GetWallConfig(const int sock, int& row, int& col, int& resv, int& resh)
	{
		if ( !SendPkg(sock, DT_CMD_GET_DESKTOPCFG, NULL, 0))
		{
			return false;
		}

		int respLen = XProtocol::ReadPackLen(sock);
		X_PACKAGE_t* pkg = (X_PACKAGE_t*)malloc(respLen);
		if (RET_SUCCESS != XProtocol::GetPackage(sock, pkg, respLen))
		{
			free(pkg);
			return false;
		}

		DT_DESKTOP_CFG_t* cfg = (DT_DESKTOP_CFG_t*)pkg->data;
		row  = cfg->row;
		col  = cfg->col;
		resh = cfg->resh;
		resv = cfg->resv;

		free(pkg);
		return true;
	}
private:
	bool SendDescInfo(int row,int col,int resv,int resh)
	{
		LOG(LEVEL_INFO,"ssm::SendDescInfo,row=%d,col=%d,resv=%d,resh=%d\n",row,col,resv,resh);
		int streamNum = row*col;
		IPMS_STREAM_t *streams = (IPMS_STREAM_t *)new char[streamNum*sizeof(IPMS_STREAM_t)];
		memset(streams,0,streamNum*sizeof(IPMS_STREAM_t));
		double per_w = 1.0/col;
		double per_h = 1.0/row;

		for(int i=0;i<row;i++)
			for(int j=0;j<col;j++)
			{
				streams[i*col+j].stream_id = 0+5*(i*col+j);
				streams[i*col+j].is_video = 1;
				streams[i*col+j].format = IPMS_MEDIA_FMT_H264;
				streams[i*col+j].video.region.h = per_h;
				streams[i*col+j].video.region.w = per_w;
				streams[i*col+j].video.region.x = per_w*j;
				streams[i*col+j].video.region.y = per_h*i;
		//		LOG(LEVEL_INFO,"ssm::w=%f,h=%f,x=%f,y=%f\n",per_w,per_h,per_w*j,per_h*i);
				streams[i*col+j].video.width = resh;
				streams[i*col+j].video.height =resv ;
				streams[i*col+j].video.clip.h = 1.0;
				streams[i*col+j].video.clip.w = 1.0;
				streams[i*col+j].video.clip.x = 0;
				streams[i*col+j].video.clip.y = 0;
			}
		OnStream(streamNum,streams);
		return true;
	}
private:
	void MakeStampID(int row,int col)
	{
		for(int index=0;index<row*col;index++)
		{
			Uuid id;
			id = new_stamp_id();
			_video_stamp_id.push_back(id);
		}
	}

private:
	IPMS_FRAME_TYPE_t GetFrameType(char *buf_data,int length)
	{
		IPMS_FRAME_TYPE_t frametype;
		if((buf_data[4]&0x1f)==7)
		{
			frametype = IPMS_FRAME_IFRAME;
		}
		else if((buf_data[4]&0x1f)==6)
		{
			frametype = IPMS_FRAME_BFRAME;
		}
		else if((buf_data[4]&0x1f)==1)
		{
			frametype = IPMS_FRAME_PFRAME;
		}
		else
		{
			frametype = IPMS_FRAME_NONE;
			LOG(LEVEL_ERROR,"ssm::Undefined Frame!\n");
		}
		return frametype;
	}

private:
	void SendVideoData(IPMS_FRAME_TYPE_t type,char *data,int len,int index,ULONGLONG framestamp,int streamType)
	{
	//	LOG(LEVEL_INFO,"SendVideoData::index=%d,row=%d,col=%d",index,_row,_col);
		double per_w = 1.0/_col;
		double per_h = 1.0/_row;
		for(int i=0;i<_row;i++)
			for(int j=0;j<_col;j++)
			{
				if((i*_col+j)==index)
				{
					IPMS_FRAME_t* frame = (IPMS_FRAME_t*)new char[sizeof(IPMS_FRAME_t)];
					memset(frame,0,sizeof(IPMS_FRAME_t));
					frame->stream_id = 0+5*(index);
					frame->media_fmt = (IPMS_MEDIA_FMT_t)streamType;
					frame->is_video = 1;
					frame->timestamp =  framestamp;
					frame->video.height = _resv;
					frame->video.width = _resh;
					frame->video.region.h = per_h;
					frame->video.region.w = per_w;
					frame->video.region.x = per_w*j;
					frame->video.region.y = per_h*i;
					frame->video.clip.h = 1.0;
					frame->video.clip.w = 1.0;
					frame->video.clip.x = 0;
					frame->video.clip.y = 0;
					frame->video.frame_type = type;
					frame->frame_data = (unsigned char*)data;				
					frame->frame_size = len;
					OnFrame(frame,index);
					delete[] frame;
					frame = NULL;
				}
			}
	}

private:	
	void DoBlock(DT_BLOCK_LIST_t* blocks)
	{
		DT_BLOCK_t*		 block  = (DT_BLOCK_t*)blocks->blocklist;

		u_int64 stamp = GetCurTime();
		static int flag_count = 0;
		if(flag_count%30==0)
		{
			struct tm *ptr;
			time_t lt;
			lt =time(NULL);
			char *str_time = ctime(&lt);
			LOG(LEVEL_INFO,"DeskTop::current time=%s,block.num=%d,block.frametype=%d",str_time,blocks->num,block[0].streamType);
		}
		flag_count++;
//		LOG(LEVEL_INFO,"ssm::time=%ul",stamp);
		for (int i = 0; i < blocks->num; i++)
		{
			if (NULL == block[i].shmName || 0 == block[i].framesize)
			{
				continue;
			}
	//		LOG(LEVEL_INFO,"ssm:::BlockID");
			Shmem shmem;
			if (shmem.Open(block[i].shmName, block[i].framesize))
			{
				//IPMS_FRAME_TYPE_t type = GetFrameType((char*)shmem.Ptr(),block[i].framesize);
				///发送码流数据
				IPMS_FRAME_TYPE_t type;
				if (block[i].frametype == 0)
				{
					type = IPMS_FRAME_IFRAME;
				}
				else if(block[i].frametype == 1)
				{
					type = IPMS_FRAME_PFRAME;
				}
				else if(block[i].frametype == 2)
				{
					type = IPMS_FRAME_BFRAME;
				}
				else
				{
					LOG(LEVEL_INFO, "No know frametype");
				}
				SendVideoData(type,(char*)shmem.Ptr(),block[i].framesize,i,stamp,block[i].streamType);
			}
			else
			{
				req_error(XVIDEO_PLUG_NODATA_ERR);
				LOG(LEVEL_INFO, " size:%d",  block[i].framesize);
			}

		}
	}

	void FreeBlock(X_PACKAGE_t* packets)
	{
		DT_BLOCK_LIST_t* blocks = (DT_BLOCK_LIST_t*)packets->data;
		DT_BLOCK_t*		 block  = (DT_BLOCK_t*)blocks->blocklist;

		for (int i = 0; i < blocks->num; i++)
		{
			if (NULL == block[i].shmName  || 0 == block[i].framesize)
			{
				continue;
			}

			Shmem shmem;
			if (shmem.Open(block[i].shmName,block[i].framesize))
			{
				shmem.SetStatus(FRAME_STATUS_FREE);
			}
		}
		free(packets);
	}

private:
	void threadloop()
	{
		if(Connect()<0)
		{
			req_error(XVIDEO_PLUG_CONNECT_ERR);
			return;
		}
		if(!Login(_sock))
		{
			Disconnect();
			LOG(LEVEL_ERROR,"Login Failed!\n");
			return ;
		}
		if(!Play())
		{
			LOG(LEVEL_ERROR,"Play Failed!\n");
			Logout();
			Disconnect();
			return;
		}
		X_PACKAGE_t* LastPacket = NULL;

		while(!isexit())		//循环接收取数据指令
		{
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(_sock, &fds);

			struct timeval tv = {0, 100000};
			int nfds = select(FD_SETSIZE, &fds, NULL, NULL, &tv);

			if (nfds > 0 && FD_ISSET(_sock, &fds))
			{			
				int cmdLen = XProtocol::ReadPackLen(_sock);
				if (cmdLen < 0)
				{
					LOG(LEVEL_INFO,"ssm::ReadPackLen!");
					req_error(XVIDEO_PLUG_NODATA_ERR);
					break ;
				}
				X_PACKAGE_t* hdpkg = (X_PACKAGE_t*)malloc(cmdLen);
				if (RET_SUCCESS != XProtocol::GetPackage(_sock, hdpkg, cmdLen))
				{
					LOG(LEVEL_INFO,"ssm::ReadPackLen Failed!");
					free(hdpkg);
					req_error(XVIDEO_PLUG_NODATA_ERR);
					break ;
				}

				if (hdpkg->subtype != DT_MSG_HANDLE_BUF)
				{
					LOG(LEVEL_INFO,"Not right pkg.\n");
					req_error(XVIDEO_PLUG_NODATA_ERR);
					break ;
				}

				if(LastPacket)
				{
					FreeBlock(LastPacket);
				}

				LastPacket = hdpkg;
			//	LOG(LEVEL_INFO,"ssm::DoBlock!");
				//发送数据块
				DoBlock((DT_BLOCK_LIST_t*)hdpkg->data);
			}
			else if(nfds==0)
			{
				continue;
				if(LastPacket)
				{
					LOG(LEVEL_INFO,"ssm::DoBlock2!");
					DoBlock((DT_BLOCK_LIST_t*)LastPacket->data);
				}
			}
			else
			{
				req_error(XVIDEO_PLUG_NODATA_ERR);
				break;
			}
		}

		if(LastPacket)
		{
		//	LOG(LEVEL_INFO,"ssm::FreeBlock!");
			FreeBlock(LastPacket);
			LastPacket = NULL;
		}
							
		Stop();
		Logout();
		Disconnect();
	}


private:
	enum
	{
		STAMP_MAX_NUM  = 100,
		FRAMEINTER     = 40   ///帧间隔，1000/FRAMEINTER==fps
	};

private:
	IPMS_USER_t _user;

private:
	VS_RESOL _resol;
	u_int8   _rows;
	u_int8   _cols;
	u_int8   _grade;
	bool     _bHaveData;

	char*    _buf1;
	char*    _buf2;
	int      _sock;
	int      _row;
	int      _col;
	int      _resv;
	int      _resh;
private:
	u_int32 g_pkgnum;

private:
	DefExt _defext;

private:
	Uuid _video_stream_id;
	std::vector<Uuid> _video_stamp_id;
	Uuid _audio_stamp_id;

private:
	std::map<int, Uuid> _stream2id;

};


void* imp_clink_play_open(const VS_SIGNAL* signal, SigCB cb, void* context, const Uuid& item)
{
	Self_Clink_Play* i = new Self_Clink_Play(signal, cb, context, item);
	if(!i)
	{
		return NULL;
	}
	
	if(i->getLinkState())
	{
		LOG(LEVEL_WARNING,"&&&&&&&&&&&&&&&DeskTop::Has Already Connect to Desktop Server,exit!!!!!!!!");
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
int   imp_clink_play_close(void* handle)
{
	Self_Clink_Play* i = reinterpret_cast<Self_Clink_Play*>(handle);
	i->exit();
	return 0l;
}

int   imp_clink_play_set_i_frame(void* handle)
{
	Self_Clink_Play* i = reinterpret_cast<Self_Clink_Play*>(handle);
	i->set_i_frame();
	return 0l;
}

int   imp_clink_play_error(void* handle)
{
	Self_Clink_Play* i = reinterpret_cast<Self_Clink_Play*>(handle);
	return i->geterror();
}

