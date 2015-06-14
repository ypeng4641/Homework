#include "TfVideoGroup.h"
#include "TfVideoOutput.h"

#include <utils/time.h>

#include <x_log/msglog.h>

TfVideoGroup::TfVideoGroup(const Uuid& signal, const Uuid& stream, u_int32 group_id, u_int32 multi_ipaddr, u_int16 multi_port):
	_signal(signal), _stream(stream), _id(group_id), 
	_is_multicast(true), _multi_ipaddr(multi_ipaddr), _multi_port(multi_port)
{
}


TfVideoGroup::TfVideoGroup(const Uuid& signal, const Uuid& stream, u_int32 group_id):
	_signal(signal), _stream(stream), _id(group_id), 
	_is_multicast(false), _multi_ipaddr(0l), _multi_port(0)
{
}


TfVideoGroup::~TfVideoGroup(void)
{
	for(std::list<TfVideoOutput*>::iterator it = _output_list.begin();
		it != _output_list.end();
		it++)
	{
		delete (*it);
	}
}

u_int32 TfVideoGroup::id()
{
	return _id;
}

bool TfVideoGroup::isdelete()
{
	return _output_list.empty();
}


int TfVideoGroup::addoutput(const VS_OUTPUT* output, const char info[16], const std::list<VideoSlice*>& ippp_list, TfMethod* tfm)
{
	//
	if(_is_multicast)
	{
		_output_list.push_back(new TfVideoOutput(_signal, _stream, info, _id, output, _multi_ipaddr, _multi_port, ippp_list, tfm));
	}
	else
	{
		_output_list.push_back(new TfVideoOutput(_signal, _stream, info, _id, output, ippp_list, tfm));
	}

	return 0l;
}

int TfVideoGroup::deloutput(const VS_OUTPUT* output)
{
	for(std::list<TfVideoOutput*>::iterator it = _output_list.begin();
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

int TfVideoGroup::multicast(u_int32 multi_ipaddr, u_int16 multi_port)
{
	_is_multicast = true;
	_multi_ipaddr = multi_ipaddr;
	_multi_port = multi_port;

	for(std::list<TfVideoOutput*>::iterator it = _output_list.begin();
		it != _output_list.end();
		it++)
	{
		(*it)->multicast(_multi_ipaddr, _multi_port);
	}

	return 0l;
}

int TfVideoGroup::unicast()
{
	_is_multicast = false;
	_multi_ipaddr = 0;
	_multi_port = 0;

	for(std::list<TfVideoOutput*>::iterator it = _output_list.begin();
		it != _output_list.end();
		it++)
	{
		(*it)->unicast();
	}

	return 0l;
}

int TfVideoGroup::online(const VS_DISP_CHANGE* p, std::set<u_int32>& group_list, const std::list<VideoSlice*>& ippp_list)
{
	for(std::list<TfVideoOutput*>::iterator it = _output_list.begin();
		it != _output_list.end();
		it++)
	{
		(*it)->online(p, group_list, ippp_list);
	}

	return 0l;
}

int TfVideoGroup::offline(const VS_DISP_CHANGE* p)
{
	for(std::list<TfVideoOutput*>::iterator it = _output_list.begin();
		it != _output_list.end();
		it++)
	{
		(*it)->offline(p);
	}

	return 0l;
}

int TfVideoGroup::setbase(u_int32 group_id, u_int32 base_id, const timeval* tv)
{
	for(std::list<TfVideoOutput*>::iterator it = _output_list.begin();
		it != _output_list.end();
		it++)
	{
		(*it)->setbase(group_id, base_id, tv);
	}

	return 0l;
}


int TfVideoGroup::packet(VideoSlice* p)
{
	for(std::list<TfVideoOutput*>::iterator it = _output_list.begin();
		it != _output_list.end();
		it++)
	{
		(*it)->packet(p);
	}

	return 0l;
}

void TfVideoGroup::timeout()
{
	for(std::list<TfVideoOutput*>::iterator it = _output_list.begin();
		it != _output_list.end();
		it++)
	{
		(*it)->timeout();
	}

}


