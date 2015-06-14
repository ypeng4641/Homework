#include "TfAudioSource.h"
#include "TfAudioGroup.h"

#include <network/network.h>

#include "AudioSlice.h"

#include "Dispatch.h"

#include "defvalue.h"

static const Uuid null;

TfAudioSource::TfAudioSource(const Uuid& signal, const Uuid& stream): 
	_signal(signal), _stream(stream), 
	_is_multicast(false), _multi_ipaddr(0l), _multi_port(0), 
	_base_id(0),
	_frame_number(0)
{
}


TfAudioSource::~TfAudioSource(void)
{
	//
	for(std::map<u_int32, TfAudioGroup*>::iterator it = _group_map.begin();
		it != _group_map.end();
		it++)
	{
		delete it->second;
	}

	for(std::list<AudioSlice*>::iterator it = _audio_list.begin();
		it != _audio_list.end();
		it++)
	{
		delete (*it);
	}

	if(_is_multicast)
	{
		Dispatch::instance()->del(_multi_ipaddr, _multi_port);
	}

}

bool TfAudioSource::isthis(const Uuid& stream)
{
	return _stream == stream;
}

int TfAudioSource::addoutput(u_int32 group_id, const VS_OUTPUT* output, const char info[16], TfMethod* tfm)
{
	//
	TfAudioGroup* group = get(group_id);
	if(!group)
	{
		if(_is_multicast)
		{
			group = new TfAudioGroup(_signal, _stream, group_id, _multi_ipaddr, _multi_port);
		}
		else
		{
			group = new TfAudioGroup(_signal, _stream, group_id);
		}

		add(group);
	}

	//
	group->addoutput(output, info, tfm);

	//
	return 0l;

}

int TfAudioSource::deloutput(u_int32 group_id, const VS_OUTPUT* output)
{
	//
	TfAudioGroup* group = get(group_id);
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

int TfAudioSource::multicast(u_int32 multi_ipaddr, u_int16 multi_port)
{
	_is_multicast = true;
	_multi_ipaddr = multi_ipaddr;
	_multi_port = multi_port;

	for(std::map<u_int32, TfAudioGroup*>::iterator it = _group_map.begin();
		it != _group_map.end();
		it++)
	{
		it->second->multicast(_multi_ipaddr, multi_port);
	}

	//
	Dispatch::instance()->add(_multi_ipaddr, _multi_port);

	return 0l;
}

int TfAudioSource::unicast()
{
	// 
	Dispatch::instance()->del(_multi_ipaddr, _multi_port);

	_is_multicast = false;
	_multi_ipaddr = 0;
	_multi_port = 0;

	for(std::map<u_int32, TfAudioGroup*>::iterator it = _group_map.begin();
		it != _group_map.end();
		it++)
	{
		it->second->unicast();
	}

	return 0l;
}

int TfAudioSource::online(const VS_DISP_CHANGE* p, std::set<u_int32>& group_list)
{
	//
	for(std::map<u_int32, TfAudioGroup*>::iterator it = _group_map.begin();
		it != _group_map.end();
		it++)
	{
		it->second->online(p, group_list);
	}

	//
	return 0l;
}

int TfAudioSource::offline(const VS_DISP_CHANGE* p)
{
	for(std::map<u_int32, TfAudioGroup*>::iterator it = _group_map.begin();
		it != _group_map.end();
		it++)
	{
		it->second->offline(p);
	}

	return 0l;
}

int TfAudioSource::setbase(u_int32 group_id, u_int32 base_id, const timeval* tv)
{
	//
	for(std::map<u_int32, TfAudioGroup*>::iterator it = _group_map.begin();
		it != _group_map.end();
		it++)
	{
		it->second->setbase(group_id, base_id, tv);
	}

	_base_id = base_id;

	//
	return 0l;
}

int TfAudioSource::audio(const P_AUDIO* audio, u_int64 timestamp)
{	
	AudioSlice* p = new AudioSlice(_base_id, _frame_number, timestamp, audio);
	_frame_number++;

#ifdef OPT_NO_TIMEOUT
	for(unsigned int i = 0; i < p->slice_count(); i++)
	{
		if(_is_multicast)
		{
			Dispatch::instance()->push(_multi_ipaddr, _multi_port, p->slice(i));
		}

		for(std::map<u_int32, TfAudioGroup*>::iterator groupIt = _group_map.begin();
			groupIt != _group_map.end();
			groupIt++)
		{
			groupIt->second->packet(p->slice(i));
		}
	}
#else
	_audio_list.push_back(p);
#endif//OPT_NO_TIMEOUT
		
	return 0l;
}

void TfAudioSource::timeout()
{
	//
	for(std::list<AudioSlice*>::iterator it = _audio_list.begin();
		it != _audio_list.end(); )
	{
		//
		for(unsigned int i = 0; i < (*it)->slice_count(); i++)
		{
			if(_is_multicast)
			{
				Dispatch::instance()->push(_multi_ipaddr, _multi_port, (*it)->slice(i));
			}

			for(std::map<u_int32, TfAudioGroup*>::iterator groupIt = _group_map.begin();
				groupIt != _group_map.end();
				groupIt++)
			{
				groupIt->second->packet((*it)->slice(i));
			}

		}

		//
		delete (*it);
		it = _audio_list.erase(it);
	}
}

void TfAudioSource::add(TfAudioGroup* group)
{
	_group_map.insert(std::make_pair(group->id(), group));
}

void TfAudioSource::del(TfAudioGroup* group)
{
	std::map<u_int32, TfAudioGroup*>::iterator it = _group_map.find(group->id());
	if(it != _group_map.end())
	{
		_group_map.erase(it);
	}
}

TfAudioGroup* TfAudioSource::get(u_int32 group_id)
{
	std::map<u_int32, TfAudioGroup*>::iterator it = _group_map.find(group_id);
	if(it != _group_map.end())
	{
		return it->second;
	}

	return NULL;
}


