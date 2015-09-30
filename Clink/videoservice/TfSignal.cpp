#include "TfSignal.h"
#include "TfAudioSource.h"
#include "TfVideoSource.h"

#include <x_log/msglog.h>

TfSignal::TfSignal(const Uuid& id): _id(id), _base_id(0)
	, _nowBlockStamp(10), _nowSysTime(0), _preSysTime(0)
{
}


TfSignal::~TfSignal(void)
{
	for(std::list<TfAudioSource*>::iterator it = _audio_list.begin();
		it != _audio_list.end();
		it++)
	{
		delete (*it);
	}

	for(std::list<TfVideoSource*>::iterator it = _video_list.begin();
		it != _video_list.end();
		it++)
	{
		delete (*it);
	}

	for(std::list<Stamp2Showtime*>::iterator it = _s2s_list.begin();
		it != _s2s_list.end(); 
		it++)
	{
		delete (*it);
	}
}

int TfSignal::addoutput(const Uuid& stream_id, const Uuid& block_id, u_int32 group_id, const VS_OUTPUT* output, const char info[16], TfMethod* tfm)
{
	TfVideoSource* p = get(stream_id, block_id);
	if(!p)
	{
		return 0l;
	}

	p->addoutput(group_id, output, info, tfm);
	
	return 0l;
}

int TfSignal::deloutput(const Uuid& stream_id, const Uuid& block_id, u_int32 group_id, const VS_OUTPUT* output)
{
	TfVideoSource* p = get(stream_id, block_id);
	if(!p)
	{
		return 0l;
	}

	p->deloutput(group_id, output);

	return 0l;
}

int TfSignal::multicast(const Uuid& stream_id, const Uuid& block_id, u_int32 multi_ipaddr, u_int16 multi_port)
{
	TfVideoSource* p = get(stream_id, block_id);
	if(!p)
	{
		return 0l;
	}

	p->multicast(multi_ipaddr, multi_port);

	return 0l;
}

int TfSignal::unicast(const Uuid& stream_id, const Uuid& block_id)
{
	TfVideoSource* p = get(stream_id, block_id);
	if(!p)
	{
		return 0l;
	}

	p->unicast();

	return 0l;
}

int TfSignal::addoutput(const Uuid& stream_id, u_int32 group_id, const VS_OUTPUT* output, const char info[16], TfMethod* tfm)
{
	TfAudioSource* p = get(stream_id);
	if(!p)
	{
		return 0l;
	}

	p->addoutput(group_id, output, info, tfm);
	
	return 0l;
}

int TfSignal::deloutput(const Uuid& stream_id, u_int32 group_id, const VS_OUTPUT* output)
{
	TfAudioSource* p = get(stream_id);
	if(!p)
	{
		return 0l;
	}

	p->deloutput(group_id, output);

	return 0l;
}
int TfSignal::multicast(const Uuid& stream_id, u_int32 multi_ipaddr, u_int16 multi_port)
{
	TfAudioSource* p = get(stream_id);
	if(!p)
	{
		return 0l;
	}

	p->multicast(multi_ipaddr, multi_port);

	return 0l;
}

int TfSignal::unicast(const Uuid& stream_id)
{
	TfAudioSource* p = get(stream_id);
	if(!p)
	{
		return 0l;
	}

	p->unicast();

	return 0l;
}

int TfSignal::setbase(u_int32 group_id)
{
	newbase();
	setbase(group_id, _base_id, &_base_time);

	return 0l;
}

int TfSignal::online(const VS_DISP_CHANGE* p)
{
	std::set<u_int32> group_list;
	for(std::list<TfAudioSource*>::iterator it = _audio_list.begin();
		it != _audio_list.end();
		it++)
	{
		(*it)->online(p, group_list);
	}

	for(std::list<TfVideoSource*>::iterator it = _video_list.begin();
		it != _video_list.end();
		it++)
	{
		(*it)->online(p, group_list);
	}

	newbase();
	for(std::set<u_int32>::iterator it = group_list.begin();
		it != group_list.end();
		it++)
	{
		setbase((*it), _base_id, &_base_time);
	}

	return 0l;
}

int TfSignal::offline(const VS_DISP_CHANGE* p)
{
	for(std::list<TfAudioSource*>::iterator it = _audio_list.begin();
		it != _audio_list.end();
		it++)
	{
		(*it)->offline(p);
	}

	for(std::list<TfVideoSource*>::iterator it = _video_list.begin();
		it != _video_list.end();
		it++)
	{
		(*it)->offline(p);
	}

	return 0l;
}

int TfSignal::desc(const VS_SIGNAL_DESC* p)
{

#if 1
	
	O_DESC de(p);

	// audio
	std::list<TfAudioSource*> new_audio_list;
	for(unsigned int i = 0; i < de.audio_cnt(); i++)
	{
		bool found = false;
		for(std::list<TfAudioSource*>::iterator it = _audio_list.begin(); it != _audio_list.end(); ++it)
		{
			if ((*it)->isthis(de.audio(i)->id()))
			{
				found = true;
				new_audio_list.push_back(*it);
				_audio_list.erase(it);
				break;
			}
		}

		if (!found)
		{
			new_audio_list.push_back(new TfAudioSource(_id, de.audio(i)->id()));
		}
	}
	for(std::list<TfAudioSource*>::iterator it = _audio_list.begin(); it != _audio_list.end();)
	{
		delete *it;
		it = _audio_list.erase(it);
	}
	_audio_list = new_audio_list;



	// video
	std::list<TfVideoSource*> new_video_list;
	for(unsigned int i = 0; i < de.video_cnt(); i++)
	{
		const O_VIDEO_DESC* desc = de.video(i);
		for(unsigned int row = 0; row < desc->row(); row++)
		{
			for(unsigned int col = 0; col < desc->col(); col++)
			{
				bool found = false;
				for(std::list<TfVideoSource*>::iterator it = _video_list.begin(); it != _video_list.end();  ++it)
				{
					if ((*it)->isthis(de.video(i)->id(), de.video(i)->block(row, col)->id()))
					{
						found = true;
						new_video_list.push_back(*it);
						_video_list.erase(it);
						break;
					}
				}

				if (!found)
				{
					new_video_list.push_back(new TfVideoSource(_id, de.video(i)->id(), de.video(i)->block(row, col)->id()));
				}
			}
		}
	}
	for(std::list<TfVideoSource*>::iterator it = _video_list.begin(); it != _video_list.end(); )
	{
		delete *it;
		it = _video_list.erase(it);
	}
	_video_list = new_video_list;
	
	return 0l;
#else
	// delete old source object
	for(std::list<TfAudioSource*>::iterator it = _audio_list.begin();
		it != _audio_list.end(); )
	{
		delete *it;
		it = _audio_list.erase(it);
	}

	for(std::list<TfVideoSource*>::iterator it = _video_list.begin();
		it != _video_list.end(); )
	{
//		LOG(LEVEL_INFO, "####################delete TfVideoSource, this=%d.\n", *it);
		delete *it;
		it = _video_list.erase(it);
	}

	//
	for(std::list<Stamp2Showtime*>::iterator it = _s2s_list.begin();
		it != _s2s_list.end(); )
	{
		delete *it;
		it = _s2s_list.erase(it);
	}
//	LOG(LEVEL_INFO, "#################desc, delete old source object! And add new source object!\n");

	// create new source object
	O_DESC de(p);
	for(unsigned int i = 0; i < de.audio_cnt(); i++)
	{
		_audio_list.push_back(new TfAudioSource(_id, de.audio(i)->id()));
	}

	for(unsigned int i = 0; i < de.video_cnt(); i++)
	{
		const O_VIDEO_DESC* desc = de.video(i);

		for(unsigned int row = 0; row < desc->row(); row++)
		{
			for(unsigned int col = 0; col < desc->col(); col++)
			{
				_video_list.push_back(new TfVideoSource(_id, de.video(i)->id(), de.video(i)->block(row, col)->id()));
//				LOG(LEVEL_INFO, "############# new TfVideoSource, this=%d.\n", _video_list.back());
			}
		}
	}

	return 0l;
#endif
}

int TfSignal::audio(const P_AUDIO* p)
{
	TfAudioSource* s = get(p->stream);
	if(!s)
	{
		return 0l;
	}

	Stamp2Showtime* s2s = gets2s(p->stampId);
	if(!s2s)
	{
		s2s = new Stamp2Showtime(p->stampId);
		_s2s_list.push_back(s2s);
	}
	
	struct timeval tv;
	gettimeofday(&tv, NULL);
	u_int64 nowtime = ((u_int64)tv.tv_sec) * 1000 + tv.tv_usec / 1000;

	u_int64 timestamp = 0;
	u_int64 sys_showtime = 0;
	u_int32 sys_frameno = 0;
	//if(s2s->toshowtime(p->timestamp, timestamp, true, 0))
	bool ret = s2s->toshowtime(p->timestamp, timestamp, false, nowtime);
	s2s->map2sys(timestamp, sys_frameno, sys_showtime);
	if(ret)
	{
		s->audio(p, sys_showtime);
	}
	
	//static bool once = false;
	//if(! once)
	//{
	//	once = true;
	//	LOG(LEVEL_INFO, "ypeng@ Audio stampID:%s .\n", p->stampId.string().c_str());
	//}
	return 0l;
}

int TfSignal::video(const P_VIDEO* p)
{
	TfVideoSource* s = get(p->stream, p->block);
	if(!s)
	{
		LOG(LEVEL_ERROR, "############Can't find TfVideoSource!\n");
		return 0l;
	}
//	LOG(LEVEL_INFO, "get TfVideoSource, this=%d.\n", s);

	Stamp2Showtime* s2s = gets2s(p->stampId);
	if(!s2s)
	{
		s2s = new Stamp2Showtime(p->stampId);
		_s2s_list.push_back(s2s);
	}

	u_int64 sys_timestamp = 0;
	u_int32 sys_frameno   = 0;
	u_int32 framenum      = 0;
	int8    replay        = 0;

	if(_nowBlockStamp != p->timestamp)
	{
		_nowBlockStamp = p->timestamp;
		struct timeval tv;
		gettimeofday(&tv, NULL);
		_nowSysTime = ((u_int64)tv.tv_sec) * 1000 + tv.tv_usec / 1000;
	}
	//if(s2s->toshowtime(p->timestamp, timestamp))
	if(s2s->getshowtime(p->timestamp, sys_timestamp, sys_frameno, replay, framenum, p->useVirtualStamp, _nowSysTime))
	{
		s->video(p, sys_timestamp, sys_frameno, replay, framenum);
		if(p->frametype == 0)
		{
	//		LOG(LEVEL_INFO, "s2s(%d), ###I Frame, ORGstamp=%llu, MAKEstamp=%llu, frameNum=%u, sysTime=%llu, sysDif=%llu, isVirtual=%s.\n", s2s, p->timestamp, sys_timestamp, framenum, _nowSysTime, (_nowSysTime - _preSysTime), (p->useVirtualStamp)?("true"):("false"));
			_preSysTime = _nowSysTime;
		}
	}

	//static bool once = false;
	//if(! once)
	//{
	//	once = true;
	//	LOG(LEVEL_INFO, "ypeng@ Video stampID:%s .\n", p->stampId.string().c_str());
	//}
	return 0l;

}

void TfSignal::timeout()
{
	for(std::list<TfAudioSource*>::iterator it = _audio_list.begin();
		it != _audio_list.end(); 
		it++)
	{
		(*it)->timeout();
	}

	for(std::list<TfVideoSource*>::iterator it = _video_list.begin();
		it != _video_list.end(); 
		it++)
	{
		(*it)->timeout();
	}
}

void TfSignal::newbase()
{
	gettimeofday(&_base_time, NULL);

	_base_time.tv_usec += 50000;
	if(_base_time.tv_usec >= 1000000)
	{
		_base_time.tv_sec += 1;
		_base_time.tv_usec -= 1000000;
	}

	_base_id++;
}

void TfSignal::setbase(u_int32 group_id, u_int32 base_id, const timeval* basetime)
{
	LOG(LEVEL_INFO, "group_id = %u, base_id = %u, basetime = %llu", group_id, base_id, (u_int64)basetime->tv_sec * 1000 + basetime->tv_usec / 1000);

	for(std::list<TfAudioSource*>::iterator it = _audio_list.begin();
		it != _audio_list.end();
		it++)
	{
		(*it)->setbase(group_id, base_id, basetime);
	}

	for(std::list<TfVideoSource*>::iterator it = _video_list.begin();
		it != _video_list.end();
		it++)
	{
		(*it)->setbase(group_id, base_id, basetime);
	}

}

TfAudioSource* TfSignal::get(const Uuid& stream)
{
	for(std::list<TfAudioSource*>::iterator it = _audio_list.begin();
		it != _audio_list.end();
		it++)
	{
		if((*it)->isthis(stream))
		{
			return *it;
		}
	}

	return NULL;
}

TfVideoSource* TfSignal::get(const Uuid& stream, const Uuid& block)
{
	for(std::list<TfVideoSource*>::iterator it = _video_list.begin();
		it != _video_list.end();
		it++)
	{
		if((*it)->isthis(stream, block))
		{
			return *it;
		}
	}

	return NULL;
}

Stamp2Showtime* TfSignal::gets2s(const Uuid& stampId)
{
	for(std::list<Stamp2Showtime*>::iterator it = _s2s_list.begin();
		it != _s2s_list.end();
		it++)
	{
		if((*it)->id() == stampId)
		{
			return (*it);
		}
	}

	return NULL;

}




