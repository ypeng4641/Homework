#ifndef _BACKGROUND_H_
#define _BACKGROUND_H_
#include <x_pattern/controler.h>
#include <x_pattern/netpump.h>
#include <x_util/uuid.h>
#include "Plugin.h"
#include "nosignal.h"

class Background
{
#define SIZE 1024*1024
public:
	~Background()
	{
		delete[] (char*)p;
		delete[] (char*)vdesc;
	}

	Background()
	{
		p = reinterpret_cast<P_VIDEO*>(new char[sizeof(P_VIDEO)+SIZE]);
		vdesc =  reinterpret_cast<VS_VIDEO_STREAM_DESC*>(new char[sizeof(VS_VIDEO_STREAM_DESC)+sizeof(VS_BLOCK_DESC)]);

		_area.x = 0;
		_area.y = 0;
		_area.w = 704;
		_area.h = 576;
		_res.w = 704;
		_res.h = 576;

		memcpy(p->data, (char*)dat, sizeof(dat));

		p->area = _area;
		p->resol = _res;
		p->valid = _area;
		p->codec = 0;
		p->frametype = 0;//IFrame
		p->timestamp = GetTickCount();
		p->datalen = sizeof(dat);
		

		////
		d.audio_num = 0;
		d.video_num = 1;
		d.videos = &vdesc;
		d.audios = NULL;
		
		//vdesc->id = id;
		vdesc->isaudio = 0;
		vdesc->resol = _res;
		vdesc->rows = 1;
		vdesc->cols = 1;
		vdesc->grade = 6;
		//vdesc->desc[0].id = id();
		vdesc->desc[0].area = _area;
		vdesc->desc[0].resol = _res;
		vdesc->desc[0].valid = _area;
	}

	P_VIDEO* getvideo(Uuid &id)
	{
		p->item = id;
		p->stream = id;
		p->block = id;
		p->stampId = id;
		p->timestamp = GetTickCount();

		return p;
	}

	device::Desc *getdesc(Uuid& id)
	{
		d.videos[0]->id = id;
		d.videos[0]->desc[0].id = id;
		return &d;
	}

	VS_AREA area()
	{
		return _area;
	}

	VS_RESOL res()
	{
		return _res;
	}

	VS_AREA valid()
	{
		return _area;
	}

private:
	P_VIDEO* p;
	VS_AREA _area;
	VS_RESOL _res;
private:
	VS_VIDEO_STREAM_DESC *vdesc;
	device::Desc d;

};



#endif
