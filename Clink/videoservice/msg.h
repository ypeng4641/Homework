#ifndef MSG_INC
#define MSG_INC

#include <list>
#include <vector>

#include <math.h>

#include <Rpc.h>

#include <x_types/intxx.h>
#include <x_util/uuid.h>
#include <x_vs/xvs.h>

#include "Plugin.h"
#include "optimizer.h"

using namespace x_util;

#pragma warning(disable: 4200)

/* declare type value */
enum 
{
	/* inner use in app object */
	APP_INNER_COMMAND,
	APP_INNER_RESULT,
	APP_INNER_REQUEST,
	APP_INNER_SERVICE,
	APP_INNER_CLIENT,
	APP_INNER_PACKET,

	TRANSFER_INNER_COMMAND,
	TRANSFER_INNER_RESULT,
	TRANSFER_INNER_REQUEST,

	PLUGIN_REQ_ERROR,
	PLUGIN_REQ_DESC,
	PLUGIN_REQ_AUDIO_DATA,
	PLUGIN_REQ_VIDEO_DATA,

	TRANSFER_CMD_ADD_SIGNAL,
	TRANSFER_CMD_DEL_SIGNAL,
	TRANSFER_CMD_ADD_OUTPUT,
	TRANSFER_CMD_DEL_OUTPUT,
	TRANSFER_CMD_MULTICAST,
	TRANSFER_CMD_UNICAST,
	TRANSFER_CMD_RESET_BASE,
	TRANSFER_CMD_ONLINE,
	TRANSFER_CMD_OFFLINE,
	TRANSFER_CMD_UPDATE_DESC,
	TRANSFER_CMD_AUDIO_DATA,
	TRANSFER_CMD_VIDEO_DATA,

	TRANSFER_REQ_JOIN,
	TRANSFER_REQ_BASE,
	TRANSFER_REQ_MULTICAST,
	TRANSFER_REQ_UNICAST,

	TRANSFER_RET_STATUS,

};

struct P_STATUS
{
	int		status;
};

struct P_ERROR
{
	Uuid	item;
	int		error;
};

typedef VS_AUDIO_STREAM_DESC	P_AUDIO_DESC;
typedef VS_BLOCK_DESC			P_BLOCK_DESC;
typedef VS_VIDEO_STREAM_DESC	P_VIDEO_DESC;
typedef VS_SIGNAL_DESC			P_DESC;
typedef VS_ADD_OUTPUT			P_ADD_OUTPUT;
typedef VS_DEL_OUTPUT			P_DEL_OUTPUT;
typedef VS_SWITCH_UNICAST		P_UNICAST;
typedef VS_SWITCH_MULTICAST		P_MULTICAST;
typedef VS_BASE					P_BASE;
typedef	VS_DISP_CHANGE			P_DISP;


struct P_AUDIO
{
	Uuid		item;
	Uuid		stream;
	Uuid		stampId;
	u_int64		timestamp;
	u_int8		codec;
	u_int32		samples;
	u_int8		channels;
	u_int8		bit_per_samples;
	u_int32		datalen;
	char		data[0];
};


struct P_VIDEO
{
	Uuid		item;
	Uuid		stream;		//
	Uuid		block;			//
	Uuid		stampId;
	u_int64		timestamp;		//
	bool		useVirtualStamp;//使用平均帧率算法
	VS_AREA		area;
	VS_RESOL	resol;
	VS_AREA		valid;
	u_int8		codec;
	u_int8		frametype;		// 0:I帧， 1:P帧
	u_int32		datalen;
	char		data[0];
};


class O_AUDIO_DESC
{
public:
	O_AUDIO_DESC(const P_AUDIO_DESC* desc):
	  _desc(*desc)
	{
	}
	~O_AUDIO_DESC()
	{
	}

private:
	O_AUDIO_DESC(const O_AUDIO_DESC& noused)
	{
	}

	void operator = (const O_AUDIO_DESC& noused)
	{
	}

public:
	unsigned int len()  const
	{
		return sizeof(_desc);
	}

	void fill(char* buf) const
	{
		memcpy(buf, &_desc, len());
	}

public:
	Uuid id() const
	{
		return _desc.id;
	}

	unsigned int grade() const
	{
		return _desc.grade;
	}

public:
	const P_AUDIO_DESC* get() const
	{
		return &_desc;
	}

private:
	P_AUDIO_DESC _desc;
};


class O_BLOCK_DESC
{
public:
	O_BLOCK_DESC(const P_BLOCK_DESC* desc):_desc(*desc)
	{
	}

	~O_BLOCK_DESC()
	{
	}

private:
	O_BLOCK_DESC(const O_BLOCK_DESC& noused)
	{
	}

	void operator = (const O_BLOCK_DESC& noused)
	{
	}

public:
	unsigned int len()  const
	{
		return sizeof(_desc);
	}

	void fill(char* buf) const
	{
		memcpy(buf, &_desc, len());
	}

public:
	const P_BLOCK_DESC* get() const
	{
		return &_desc;
	}

public:
	Uuid id() const
	{
		return _desc.id;
	}

	VS_AREA area() const
	{
		return _desc.area;
	}

private:
	P_BLOCK_DESC _desc;
};

class O_VIDEO_DESC
{
public:
	O_VIDEO_DESC(const P_VIDEO_DESC* desc)
	{
		_id = desc->id;
		_isaudio = desc->isaudio;
		_resol = desc->resol;
		_rows = desc->rows;
		_cols = desc->cols;
		_grade = desc->grade;
		
		for(unsigned int i = 0; i < _rows; i++)
		{
			for(unsigned int j = 0; j < _cols; j++)
			{
				_blocks.push_back(new O_BLOCK_DESC(&desc->desc[i * _cols + j]));
			}
		}
	}

	~O_VIDEO_DESC()
	{
		for(std::vector<O_BLOCK_DESC*>::iterator it = _blocks.begin();
			it != _blocks.end();
			it++)
		{
			delete *it;
		}
	}

private:
	O_VIDEO_DESC(const O_VIDEO_DESC& noused)
	{
	}

	void operator = (const O_VIDEO_DESC& noused)
	{
	}

public:
	unsigned int len() const
	{
		return sizeof(P_VIDEO_DESC) + _blocks.size() * sizeof(P_BLOCK_DESC);
	}

	void fill(char* buf) const
	{
		P_VIDEO_DESC* p = reinterpret_cast<P_VIDEO_DESC*>(buf);
		p->id = _id;
		p->isaudio = _isaudio;
		p->resol = _resol;
		p->rows = _rows;
		p->cols = _cols;
		p->grade = _grade;

		unsigned int i = 0;
		for(std::vector<O_BLOCK_DESC*>::const_iterator it = _blocks.begin();
			it != _blocks.end();
			it++)
		{
			(*it)->fill(reinterpret_cast<char*>(&p->desc[i]));
			i++;
		}
	}

public:
	Uuid id() const
	{
		return _id;
	}

	bool isaudio() const
	{
		return _isaudio == 1 ? true : false;
	}

	VS_RESOL resol() const
	{
		return _resol;
	}

	unsigned int block_cnt() const
	{
		return _blocks.size();
	}

	unsigned int row() const
	{
		return _rows;
	}

	unsigned int col() const
	{
		return _cols;
	}

	u_int32 grade() const
	{
		return _grade;
	}

	const O_BLOCK_DESC* block(unsigned int row, unsigned int col) const
	{
		return _blocks[row * _cols + col];
	}

private:
	Uuid _id;
	int8 _isaudio;				// 是否包含音频
	VS_RESOL _resol;
	u_int8 _rows;
	u_int8 _cols;
	u_int8 _grade;
	std::vector<O_BLOCK_DESC*> _blocks;
};

class O_DESC
{
public:
	O_DESC(const P_DESC* desc)
	{
		_id = desc->id;

		unsigned int offset = 0;

		for(unsigned int i = 0; i < desc->audio_num; i++)
		{
			const P_AUDIO_DESC* audio = reinterpret_cast<const P_AUDIO_DESC*>(desc->data + offset);
			_audios.push_back(new O_AUDIO_DESC(audio));

			offset += sizeof(P_AUDIO_DESC);
		}

		for(unsigned int i = 0; i < desc->video_num; i++)
		{
			const P_VIDEO_DESC* video = reinterpret_cast<const P_VIDEO_DESC*>(desc->data + offset);
			_videos.push_back(new O_VIDEO_DESC(video));

			offset += sizeof(P_VIDEO_DESC) + video->rows * video->cols * sizeof(P_BLOCK_DESC);
		}
	}

	O_DESC(const Uuid& id, const device::Desc* p)
	{
		_id = id;

		for(unsigned int i = 0; i < p->audio_num; i++)
		{
			_audios.push_back(new O_AUDIO_DESC(p->audios[i]));
		}

		for(unsigned int i = 0; i < p->video_num; i++)
		{
			_videos.push_back(new O_VIDEO_DESC(p->videos[i]));
		}

	}

	~O_DESC()
	{
		for(std::vector<O_AUDIO_DESC*>::iterator it = _audios.begin();
			it != _audios.end();
			it++)
		{
			delete *it;
		}

		for(std::vector<O_VIDEO_DESC*>::iterator it = _videos.begin();
			it != _videos.end();
			it++)
		{
			delete *it;
		}
	}

private:
	O_DESC(const O_DESC& noused)
	{
	}

	void operator = (const O_DESC& noused)
	{
	}

public:
	unsigned int len()  const
	{
		unsigned sumlen = 0;
		sumlen += sizeof(P_DESC);

		for(std::vector<O_AUDIO_DESC*>::const_iterator it = _audios.begin();
			it != _audios.end();
			it++)
		{
			sumlen += (*it)->len();
		}

		for(std::vector<O_VIDEO_DESC*>::const_iterator it = _videos.begin();
			it != _videos.end();
			it++)
		{
			sumlen += (*it)->len();
		}

		return sumlen;
	}

	void fill(char* buf) const
	{
		P_DESC* p = reinterpret_cast<P_DESC*>(buf);
		p->id = _id;
		p->audio_num = _audios.size();
		p->video_num = _videos.size();

		unsigned int offset = 0;
		for(std::vector<O_AUDIO_DESC*>::const_iterator it = _audios.begin();
			it != _audios.end();
			it++)
		{
			(*it)->fill(p->data + offset);
			offset += (*it)->len();
		}

		for(std::vector<O_VIDEO_DESC*>::const_iterator it = _videos.begin();
			it != _videos.end();
			it++)
		{
			(*it)->fill(p->data + offset);
			offset += (*it)->len();
		}
	}

public:
	Uuid id() const
	{
		return _id;
	}

	unsigned int audio_cnt() const
	{
		return _audios.size();
	}

	unsigned int video_cnt() const
	{
		return _videos.size();
	}

	const O_AUDIO_DESC* audio(unsigned int i) const
	{
		return _audios[i];
	}

	const O_VIDEO_DESC* video(unsigned int i) const
	{
		return _videos[i];
	}

private:
	Uuid _id;
	std::vector<O_AUDIO_DESC*> _audios;
	std::vector<O_VIDEO_DESC*> _videos;

};


class AUTOMEM
{
private:
	~AUTOMEM()
	{
		data = NULL;
	}
public:
	AUTOMEM(int size):cnt(1),len(size), isVVP(false)
	{
		data = new char [size];
	}

#ifdef OPT_MEM_TO_PTR
	AUTOMEM(const VS_VIDEO_PACKET* vvp, const P_VIDEO* pv): cnt(1)
		, len(sizeof(VS_VIDEO_PACKET) + sizeof(OPT_MEM))
		, isVVP(true)
	{
		data = new char [sizeof(VS_VIDEO_PACKET) + sizeof(OPT_MEM)];
		memcpy(data, vvp, sizeof(VS_VIDEO_PACKET));

		//
		VS_VIDEO_PACKET* auto_vvp = (VS_VIDEO_PACKET*)data;
		auto_vvp->datalen = sizeof(OPT_MEM);

		OPT_MEM* auto_om = (OPT_MEM*)auto_vvp->data;
		//auto_om->memData = new char[pv->datalen];
		//memcpy(auto_om->memData, pv->data, sizeof(pv->datalen));
		//auto_om->memLen = pv->datalen;

		OPT_MEM* om = (OPT_MEM*)pv->data;
		auto_om->memData = om->memData;
		auto_om->memLen = om->memLen;
	}
#endif//OPT_MEM_TO_PTR

	char* get()
	{
		return data;
	}
	int size()
	{
		return len;
	}
	void ref()
	{
		cnt++;
	}
	int count()
	{
		return cnt;
	}
	void release()
	{
		if((--cnt) == 0)
		{
			if(isVVP)
			{
				VS_VIDEO_PACKET* auto_vvp = (VS_VIDEO_PACKET*)data;
				OPT_MEM* auto_om = (OPT_MEM*)auto_vvp->data;
				delete[] auto_om->memData;
				auto_om->memData = NULL;
				auto_om->memLen = 0;
			}

			//
			delete[] data;
			delete this;
		}
	}
private:
	int cnt;
	int len;
	char* data;

	bool isVVP;
};


#endif