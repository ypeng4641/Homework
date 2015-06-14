#include "TfVideoSource.h"
#include "TfVideoGroup.h"

#include <network/network.h>

#include "VideoSlice.h"

#include "Dispatch.h"

#include "defvalue.h"


TfVideoSource::TfVideoSource(const Uuid& signal, const Uuid& stream, const Uuid& block): 
	_signal(signal), _stream(stream), _block(block),
	_is_multicast(false), _multi_ipaddr(0l), _multi_port(0), 
	_base_id(0),
	_frame_number(0)
{
}


TfVideoSource::~TfVideoSource(void)
{
	//
	for(std::map<u_int32, TfVideoGroup*>::iterator it = _group_map.begin();
		it != _group_map.end();
		it++)
	{
		delete it->second;
	}

	//
	for(std::list<VideoSlice*>::iterator it = _video_list.begin();
		it != _video_list.end();
		it++)
	{
		(*it)->release();
	}

	//
	for(std::list<VideoSlice*>::iterator it = _ipppp_list.begin();
		it != _ipppp_list.end();
		it++)
	{
		(*it)->release();
	}

	if(_is_multicast)
	{
		Dispatch::instance()->del(_multi_ipaddr, _multi_port);
	}
}

bool TfVideoSource::isthis(const Uuid& stream, const Uuid& block)
{
	return _stream == stream && _block == block;
}

int TfVideoSource::addoutput(u_int32 group_id, const VS_OUTPUT* output, const char info[16], TfMethod* tfm)
{
	//
	TfVideoGroup* group = get(group_id);
	if(!group)
	{
		if(_is_multicast)
		{
			group = new TfVideoGroup(_signal, _stream, group_id, _multi_ipaddr, _multi_port);
		}
		else
		{
			group = new TfVideoGroup(_signal, _stream, group_id);
		}
		
		add(group);
	}

	//
	group->addoutput(output, info, _ipppp_list, tfm);
//	LOG(LEVEL_INFO, "TfVideoSource(%d), TfVideoGroup(%d) add output, ipaddr=%08X, port=%hu, _is_multicast=%d.\n", this, output->ipaddr, output->port, (_is_multicast)?(1):(0));

	//
	return 0l;

}

int TfVideoSource::deloutput(u_int32 group_id, const VS_OUTPUT* output)
{
	//
	TfVideoGroup* group = get(group_id);
	if(!group)
	{
		return 0l;
	}

	//
	group->deloutput(output);

	//
	if(group->isdelete())
	{
		del(group);
		delete group;		
	}

	//
	return 0l;
}

int TfVideoSource::multicast(u_int32 multi_ipaddr, u_int16 multi_port)
{
	//
	_is_multicast = true;
	_multi_ipaddr = multi_ipaddr;
	_multi_port = multi_port;

	//
	for(std::map<u_int32, TfVideoGroup*>::iterator it = _group_map.begin();
		it != _group_map.end();
		it++)
	{
		it->second->multicast(_multi_ipaddr, multi_port);
	}

	//
	Dispatch::instance()->add(_multi_ipaddr, _multi_port);


	return 0l;
}

int TfVideoSource::unicast()
{
	// 
	Dispatch::instance()->del(_multi_ipaddr, _multi_port);

	//
	_is_multicast = false;
	_multi_ipaddr = 0;
	_multi_port = 0;

	//
	for(std::map<u_int32, TfVideoGroup*>::iterator it = _group_map.begin();
		it != _group_map.end();
		it++)
	{
		it->second->unicast();
	}

	return 0l;
}

int TfVideoSource::online(const VS_DISP_CHANGE* p, std::set<u_int32>& group_list)
{
	for(std::map<u_int32, TfVideoGroup*>::iterator it = _group_map.begin();
		it != _group_map.end();
		it++)
	{
		it->second->online(p, group_list, _ipppp_list);
	}

	//
	return 0l;
}

int TfVideoSource::offline(const VS_DISP_CHANGE* p)
{
	//
	for(std::map<u_int32, TfVideoGroup*>::iterator it = _group_map.begin();
		it != _group_map.end();
		it++)
	{
		it->second->offline(p);
	}

	return 0l;
}

int TfVideoSource::setbase(u_int32 group_id, u_int32 base_id, const timeval* tv)
{
	//
	for(std::map<u_int32, TfVideoGroup*>::iterator it = _group_map.begin();
		it != _group_map.end();
		it++)
	{
		it->second->setbase(group_id, base_id, tv);
	}

	_base_id = base_id;

	//
	return 0l;
}


int TfVideoSource::video(const P_VIDEO* video, u_int64 sys_timestamp, u_int32 sys_frameno, int8 replay, u_int32 framenum)
{
	// 
	VideoSlice* p = new VideoSlice(_base_id, sys_frameno, sys_timestamp, replay, framenum, video);
	_frame_number++;

	//
#ifdef OPT_NO_TIMEOUT
	if(_is_multicast)
	{
		for(unsigned int i = 0; i < p->slice_count(); i++)
		{
			Dispatch::instance()->push(_multi_ipaddr, _multi_port, p->slice(i));
		}
	}
#else
	_video_list.push_back(p);
#endif//OPT_NO_TIMEOUT

	//
	ippp(p);

	 
	for(std::map<u_int32, TfVideoGroup*>::iterator it = _group_map.begin();
		it != _group_map.end();
		it++)
	{
		it->second->packet(p);
	}

#ifdef OPT_NO_TIMEOUT
	p->release();
#endif

	//
	return 0l;
}

void TfVideoSource::timeout()
{
	for(std::list<VideoSlice*>::iterator it = _video_list.begin();
		it != _video_list.end(); )
	{
		//
		if(_is_multicast)
		{
			for(unsigned int i = 0; i < (*it)->slice_count(); i++)
			{
				Dispatch::instance()->push(_multi_ipaddr, _multi_port, (*it)->slice(i));
			}
		}

		//
		(*it)->release();
		it = _video_list.erase(it);
	}

	for(std::map<u_int32, TfVideoGroup*>::iterator groupIt = _group_map.begin();
		groupIt != _group_map.end();
		groupIt++)
	{
		groupIt->second->timeout();
	}
}

void TfVideoSource::ippp(VideoSlice* p)
{
	//_ipppp_list保存的是最后一个I帧到当前帧的全部数据
	if(!p->is_i_frame() && _ipppp_list.size() == 0)
	{
		LOG(LEVEL_INFO, "drop it");
		return;
	}

	
	if(p->is_i_frame())
	{
		for(std::list<VideoSlice*>::iterator it = _ipppp_list.begin();
			it != _ipppp_list.end(); )
		{
			(*it)->release();
			it = _ipppp_list.erase(it);
		}
	}

	_ipppp_list.push_back(p->get());

}

void TfVideoSource::add(TfVideoGroup* group)
{
	_group_map.insert(std::make_pair(group->id(), group));
}

void TfVideoSource::del(TfVideoGroup* group)
{
	std::map<u_int32, TfVideoGroup*>::iterator it = _group_map.find(group->id());
	if(it != _group_map.end())
	{
		_group_map.erase(it);
	}
}

TfVideoGroup* TfVideoSource::get(u_int32 group_id)
{
	std::map<u_int32, TfVideoGroup*>::iterator it = _group_map.find(group_id);
	if(it != _group_map.end())
	{
		return it->second;
	}

	return NULL;
}

