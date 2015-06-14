#include "TfAudioGroup.h"
#include "TfAudioOutput.h"

TfAudioGroup::TfAudioGroup(const Uuid& signal, const Uuid& stream, u_int32 group_id, u_int32 multi_ipaddr, u_int16 multi_port): 
	_signal(signal), 
	_stream(stream),
	_id(group_id), 
	_is_multicast(true), _multi_ipaddr(multi_ipaddr), _multi_port(multi_port)
{
}


TfAudioGroup::TfAudioGroup(const Uuid& signal, const Uuid& stream, u_int32 group_id): 
	_signal(signal), 
	_stream(stream),
	_id(group_id), 
	_is_multicast(false), _multi_ipaddr(0l), _multi_port(0)
{
}


TfAudioGroup::~TfAudioGroup(void)
{
	for(std::list<TfAudioOutput*>::iterator it = _output_list.begin();
		it != _output_list.end();
		it++)
	{
		delete *it;
	}

}

u_int32 TfAudioGroup::id()
{
	return _id;
}

bool TfAudioGroup::isdelete()
{
	return _output_list.empty();
}

int TfAudioGroup::addoutput(const VS_OUTPUT* output, const char info[16], TfMethod* tfm)
{
	if(_is_multicast)
	{
		_output_list.push_back(new TfAudioOutput(_signal, _stream, info, _id, output, _multi_ipaddr, _multi_port, tfm));
	}
	else
	{
		_output_list.push_back(new TfAudioOutput(_signal, _stream, info, _id, output, tfm));
	}

	return 0l;
}

int TfAudioGroup::deloutput(const VS_OUTPUT* output)
{
	for(std::list<TfAudioOutput*>::iterator it = _output_list.begin();
		it != _output_list.end();
		it++)
	{
		if((*it)->isthis(output))
		{
			delete (*it);
			_output_list.erase(it);
			break;
		}
	}

	return 0l;
}

int TfAudioGroup::multicast(u_int32 multi_ipaddr, u_int16 multi_port)
{
	_is_multicast = true;
	_multi_ipaddr = multi_ipaddr;
	_multi_port = multi_port;

	for(std::list<TfAudioOutput*>::iterator it = _output_list.begin();
		it != _output_list.end();
		it++)
	{
		(*it)->multicast(_multi_ipaddr, _multi_port);
	}

	return 0l;
}

int TfAudioGroup::unicast()
{
	_is_multicast = false;
	_multi_ipaddr = 0;
	_multi_port = 0;

	for(std::list<TfAudioOutput*>::iterator it = _output_list.begin();
		it != _output_list.end();
		it++)
	{
		(*it)->unicast();
	}

	return 0l;
}

int TfAudioGroup::online(const VS_DISP_CHANGE* p, std::set<u_int32>& group_list)
{
	for(std::list<TfAudioOutput*>::iterator it = _output_list.begin();
		it != _output_list.end();
		it++)
	{
		(*it)->online(p, group_list);
	}

	return 0l;
}

int TfAudioGroup::offline(const VS_DISP_CHANGE* p)
{
	for(std::list<TfAudioOutput*>::iterator it = _output_list.begin();
		it != _output_list.end();
		it++)
	{
		(*it)->offline(p);
	}

	return 0l;


}

int TfAudioGroup::setbase(u_int32 group_id, u_int32 base_id, const timeval* tv)
{
	for(std::list<TfAudioOutput*>::iterator it = _output_list.begin();
		it != _output_list.end();
		it++)
	{
		(*it)->setbase(group_id, base_id, tv);
	}

	return 0l;
}

int TfAudioGroup::packet(AUTOMEM* mem)
{
	for(std::list<TfAudioOutput*>::iterator it = _output_list.begin();
		it != _output_list.end();
		it++)
	{
		(*it)->packet(mem);
	}

	return 0l;
}


