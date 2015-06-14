#include "TfAudioOutput.h"
#include "Dispatch.h"

#include "__send_buf.h"

TfAudioOutput::TfAudioOutput(
	const Uuid& signal_id, 
	const Uuid& stream_id,
	const char info[16], 
	u_int32 group_id, 
	const VS_OUTPUT* output, 
	u_int32 multi_ipaddr, 
	u_int16 multi_port, 
	TfMethod* tfm): 
	_signal_id(signal_id), 
	_stream_id(stream_id),
	_group_id(group_id), 
	_output(*output), 
	_is_multicast(true), 
	_multi_ipaddr(multi_ipaddr), 
	_multi_port(multi_port),
	_tfm(tfm)
{
	//
	init(info);

	//
	Dispatch::instance()->add(_output.ipaddr, _output.port);
}

TfAudioOutput::TfAudioOutput(
	const Uuid& signal_id, 
	const Uuid& stream_id,
	const char info[16], 
	u_int32 group_id, 
	const VS_OUTPUT* output, 
	TfMethod* tfm): 
	_signal_id(signal_id), 
	_stream_id(stream_id),
	_group_id(group_id), 
	_output(*output), 
	_is_multicast(false), 
	_multi_ipaddr(0), 
	_multi_port(0),
	_tfm(tfm)
{
	//
	init(info);

	//
	Dispatch::instance()->add(_output.ipaddr, _output.port);
}

TfAudioOutput::~TfAudioOutput(void)
{
	//
	Dispatch::instance()->del(_output.ipaddr, _output.port);

}

bool TfAudioOutput::isthis(const VS_OUTPUT* output)
{
	if(_output.ipaddr == output->ipaddr &&
	   _output.output == output->output &&
	   _output.index == output->index &&
	   _output.port == output->port)
	{
		return true;
	}

	return false;
}

int TfAudioOutput::multicast(u_int32 multi_ipaddr, u_int16 multi_port)
{
	//
	_is_multicast = true;
	_multi_ipaddr = multi_ipaddr;
	_multi_port = multi_port;

	//
	_time = GetTickCount64();

	//
	VS_MULTICAST data;
	data.output = _output;
	data.multi_ipaddr = _multi_ipaddr;
	data.multi_port = _multi_port;
	_tfm->multicast(&data);

	return 0l;

}

int TfAudioOutput::unicast()
{
	//
	_is_multicast = false;
	_multi_ipaddr = 0;
	_multi_port = 0;

	//
	VS_UNICAST data;
	data.output = _output;
	_tfm->unicast(&data);

	return 0l;
}

int TfAudioOutput::online(const VS_DISP_CHANGE* p, std::set<u_int32>& group_list)
{
	if(_output.ipaddr == p->ipaddr && _output.output == p->output)
	{
		for(unsigned int i = 0; i < p->port_cnt; i++)
		{
			if(_output.index == i)
			{
				Dispatch::instance()->del(_output.ipaddr, _output.port);

				//
				_output.port = p->port[i];

				//
				Dispatch::instance()->add(_output.ipaddr, _output.port);

				//
				group_list.insert(_group_id);

				//
				char info[16];
				memset(info, 0, sizeof(info));
				init(info);

				//
				break;

			}
		}
	}	

	return 0l;
}

int TfAudioOutput::offline(const VS_DISP_CHANGE* p)
{
	if(_output.ipaddr == p->ipaddr && _output.output == p->output)
	{
		for(unsigned int i = 0; i < p->port_cnt; i++)
		{
			if(_output.index == i)
			{
				Dispatch::instance()->del(_output.ipaddr, _output.port);

				_output.port = 0;

				Dispatch::instance()->add(_output.ipaddr, _output.port);

				break;
			}
		}
	}

	return 0l;
}

int TfAudioOutput::setbase(u_int32 group_id, u_int32 base_id, const timeval* tv)
{
	VS_SETBASE data;
	data.output = _output;
	data.signal_id = _signal_id;  
	data.group_id = group_id;   
	data.base_id = base_id;    
	data.is_set = _group_id == group_id ? 1 : 0; 
	data.basetime = (u_int64)tv->tv_sec * 1000 + tv->tv_usec / 1000; 
	_tfm->base(&data);

	return 0l;
}

int TfAudioOutput::packet(AUTOMEM* mem)
{
	if(!_is_multicast || GetTickCount64() - _time < 100)
	{
		Dispatch::instance()->push(_output.ipaddr, _output.port, mem);
	}

	return 0l;
}

void TfAudioOutput::init(const char info[16])
{
	//
	_time = GetTickCount64();

	//
	VS_JOIN data;
	data.output = _output;
	data.signal_id = _signal_id;
	data.stream_id = _stream_id;
	memcpy(data.info, info, sizeof(data.info));
	_tfm->join(&data);

	//
	if(_is_multicast)
	{
		VS_MULTICAST data;
		data.output = _output;
		data.multi_ipaddr = _multi_ipaddr;
		data.multi_port = _multi_port;
		_tfm->multicast(&data);
	}
	else
	{
		VS_UNICAST data;
		data.output = _output;
		_tfm->unicast(&data);
	}

}


