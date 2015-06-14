#include "TfVideoOutput.h"

#include "VideoSlice.h"
#include "Dispatch.h"

#include "__send_buf.h"

#include <utils/time.h>

TfVideoOutput::TfVideoOutput(
	const Uuid& signal_id, 
	const Uuid& stream_id,
	const char info[16], 
	u_int32 group_id, 
	const VS_OUTPUT* output, 
	u_int32 multi_ipaddr, 
	u_int16 multi_port, 
	const std::list<VideoSlice*>& ippp_list, 
	TfMethod* tfm): 
	_signal_id(signal_id), 
	_stream_id(stream_id),
	_group_id(group_id), 
	_output(*output), 
	_is_multicast(true), 
	_multi_ipaddr(multi_ipaddr), 
	_multi_port(multi_port),
	_is_ippp(true),
	_is_first(true),
	_tfm(tfm)
{
	//
	VS_JOIN data;
	data.output = _output;
	data.signal_id = _signal_id;
	data.stream_id = _stream_id;
	memcpy(data.info, info, sizeof(data.info));
	_tfm->join(&data);

	//
	init(ippp_list);

	//
	Dispatch::instance()->add(_output.ipaddr, _output.port);

}

TfVideoOutput::TfVideoOutput(
	const Uuid& signal_id, 
	const Uuid& stream_id,
	const char info[16], 
	u_int32 group_id, 
	const VS_OUTPUT* output, 
	const std::list<VideoSlice*>& ippp_list, 
	TfMethod* tfm): 
	_signal_id(signal_id), 
	_stream_id(stream_id),
	_group_id(group_id), 
	_output(*output), 
	_is_multicast(false), 
	_multi_ipaddr(0), 
	_multi_port(0),
	_is_ippp(true),
	_is_first(true),
	_tfm(tfm)
{
	//
	VS_JOIN data;
	data.output = _output;
	data.signal_id = _signal_id;
	data.stream_id = _stream_id;
	memcpy(data.info, info, sizeof(data.info));
	_tfm->join(&data);

	//
	init(ippp_list);

	//
	Dispatch::instance()->add(_output.ipaddr, _output.port);

}

TfVideoOutput::~TfVideoOutput(void)
{
	//
	Dispatch::instance()->del(_output.ipaddr, _output.port);

	//
	clear();
}

bool TfVideoOutput::isthis(const VS_OUTPUT* output)
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

int TfVideoOutput::multicast(u_int32 multi_ipaddr, u_int16 multi_port)
{
	//
	_is_multicast = true;
	_multi_ipaddr = multi_ipaddr;
	_multi_port = multi_port;

	//
	if(!_is_ippp)
	{
		_time = GetTickCount();

		VS_MULTICAST data;
		data.output = _output;
		data.multi_ipaddr = _multi_ipaddr;
		data.multi_port = _multi_port;

		_tfm->multicast(&data);
	}

	return 0l;
}

int TfVideoOutput::unicast()
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

int TfVideoOutput::online(const VS_DISP_CHANGE* p, std::set<u_int32>& group_list, const std::list<VideoSlice*>& ippp_list)
{
	if(_output.ipaddr == p->ipaddr && _output.output == p->output)
	{
		if(_output.index < p->port_cnt && _output.index >= 0)
		{
			u_int32 i = _output.index;

			{
				Dispatch::instance()->del(_output.ipaddr, _output.port);

				//
				_output.port = p->port[i];

				Dispatch::instance()->add(_output.ipaddr, _output.port);

				//
				group_list.insert(_group_id);

				//
				VS_JOIN data;
				data.output = _output;
				data.signal_id = _signal_id;
				data.stream_id = _stream_id;
				memset(data.info, 0, sizeof(data.info));
				_tfm->join(&data);

				//
				clear();

				//
				init(ippp_list);

				//
				_is_ippp = true;
			}
		}
		else
		{
			LOG(LEVEL_ERROR, "index Error! index:%u, port_cnt:%u."
				, _output.index, p->port_cnt);
		}
	}
	else
	{
		LOG(LEVEL_ERROR, "Message has changed! src(%x:%u), now(%x:%u).", _output.ipaddr, _output.port, p->ipaddr, p->port);
	}

	return 0l;
}

int TfVideoOutput::offline(const VS_DISP_CHANGE* p)
{
	if(_output.ipaddr == p->ipaddr && _output.output == p->output)
	{
		if(_output.index < p->port_cnt && _output.index >= 0)
		{
			u_int32 i = _output.index;

			{
				Dispatch::instance()->del(_output.ipaddr, _output.port);

				_output.port = 0;

				//Dispatch::instance()->add(_output.ipaddr, _output.port);
			}
		}
	}

	return 0l;
}

int TfVideoOutput::setbase(u_int32 group_id, u_int32 base_id, const timeval* tv)
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

int TfVideoOutput::packet(VideoSlice* p)
{
	//_is_ippp: 表示未见到I帧
	if(_is_ippp)
	{
		//如果是I帧，全部清除前面缓冲的数据
		if(p->is_i_frame())
		{
			//
			_is_ippp = false;

			//
			clear();

			// send current videoslice
			for(unsigned int i = 0; i < p->slice_count(); i++)
			{
				Dispatch::instance()->push(_output.ipaddr, _output.port, p->slice(i));
			}

			LOG(LEVEL_INFO,"### I frame %llu", p->timestamp());

			// multicast or unicast
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
		else
		{
#ifndef OPT_NO_TIMEOUT
			//不是I帧，缓冲起来
			_video_list.push_back(p->get());
#endif//OPT_NO_TIMEOUT
		}

	}
	else
	{
		//如果单播
		if(!_is_multicast || GetTickCount64() - _time < 100)
		{
#ifdef OPT_NO_TIMEOUT
			for(unsigned int i = 0; i < p->slice_count(); i++)
			{
				Dispatch::instance()->push(_output.ipaddr, _output.port, p->slice(i));
			}
#else
			_video_list.push_back(p->get());
#endif//OPT_NO_TIMEOUT
			
#ifdef OPT_DEBUG_OUT
	VS_VIDEO_PACKET* packet = (VS_VIDEO_PACKET*)p->slice(0)->get();

	struct timeval tv;
	gettimeofday(&tv, NULL);

	ULONGLONG nowtime = ((ULONGLONG)tv.tv_sec * 1000 + tv.tv_usec/1000);

	long l_dif = packet->timestamp - nowtime;

	if(l_dif < 0 || g_dif < 0)
	{
		union
		{
			unsigned char v[4];
			unsigned int  value;
		}ip;
		ip.value = _output.ipaddr;
	
		OPT_MEM* omem = (OPT_MEM*)packet->data;
		pthread_t self_id = pthread_self();
		LOG(LEVEL_INFO, "thread=%d:%u, SEND >> addr=%d.%d.%d.%d, port=%d. data=%d, size=%d, stamp=%llu, framenum=%u, slicecnt=%u, slicenum=%u,nowtime=%llu, dif=%ld.\n"
			, (int)self_id.p, self_id.x
			, ip.v[3], ip.v[2], ip.v[1], ip.v[0], _output.port
			, (int)omem->memData, omem->memLen
			, packet->timestamp, packet->framenum, packet->slicecnt, packet->slicenum, nowtime, l_dif);
	}
#endif//OPT_DEBUG_OUT
		}
	}

	return 0l;
}

void TfVideoOutput::timeout()
{
	// must call be here
	struct timeval tv;
	gettimeofday(&tv, NULL);


	const u_int64 diffnx = 2 * diff(&tv);	// 

	if(_is_ippp)
	{
		// 
		if(_video_list.size() > 0)
		{
			// 
			u_int64 startpos = _video_list.front()->timestamp();

			// 
			for(std::list<VideoSlice*>::iterator it = _video_list.begin();
				it != _video_list.end(); )
			{
				if((*it)->timestamp() - startpos <= diffnx)
				{
					//
					for(unsigned int i = 0; i < (*it)->slice_count(); i++)
					{
						Dispatch::instance()->push(_output.ipaddr, _output.port, (*it)->slice(i));
					}

					//单播队列回收
					(*it)->release();
					it = _video_list.erase(it);

				}
				else
				{
					break;
				}
			}
		}

		// 
		if(_video_list.size() == 0)
		{
			//
			_is_ippp = false;

			// multicast or unicast
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

	}
	else
	{
		// send all slice
		for(std::list<VideoSlice*>::iterator it = _video_list.begin();
			it != _video_list.end(); )
		{
			// LOG(LEVEL_INFO, "%s, %llu", (*it)->is_i_frame() ? "I frame" : "P frame", (*it)->timestamp());

			//
			for(unsigned int i = 0; i < (*it)->slice_count(); i++)
			{
				Dispatch::instance()->push(_output.ipaddr, _output.port, (*it)->slice(i));
				
#ifdef OPT_DEBUG_OUT
	VS_VIDEO_PACKET* packet = (VS_VIDEO_PACKET*)(*it)->slice(i)->get();

	struct timeval tv;
	gettimeofday(&tv, NULL);

	ULONGLONG nowtime = ((ULONGLONG)tv.tv_sec * 1000 + tv.tv_usec/1000);

	long l_dif = packet->timestamp - nowtime;

	if(l_dif < 0 || g_dif < 0)//(packet->slicecnt > 3 || g_dif < 0)
	{
		union
		{
			unsigned char v[4];
			unsigned int  value;
		}ip;
		ip.value = _output.ipaddr;
	
		pthread_t self_id = pthread_self();
		LOG(LEVEL_INFO, "thread=%d:%u, packet.size=%d, loop=%d, SEND >> addr=%d.%d.%d.%d, port=%d, size=%d. stamp=%llu, framenum=%u, slicecnt=%u, slicenum=%u,nowtimestamp=%llu, dif=%ld.\n"
			, (int)self_id.p, self_id.x
			, _video_list.size(), (*it)->slice_count()
			, ip.v[3], ip.v[2], ip.v[1], ip.v[0], _output.port
			, (*it)->slice(i)->size(), packet->timestamp, packet->framenum, packet->slicecnt, packet->slicenum, nowtime, l_dif);
	}
#endif//OPT_DEBUG_OUT
			}

			//单播队列回收
			(*it)->release();
			it = _video_list.erase(it);

		}
	}
}

void TfVideoOutput::clear()
{
	//
	for(std::list<VideoSlice*>::iterator it = _video_list.begin();
		it != _video_list.end(); )
	{
		//单播队列回收
		(*it)->release();
		it = _video_list.erase(it);
	}

}

void TfVideoOutput::init(const std::list<VideoSlice*>& ippp_list)
{
	for(std::list<VideoSlice*>::const_iterator it = ippp_list.begin();
		it != ippp_list.end();
		it++)
	{
		_video_list.push_back((*it)->get());
	}

}

u_int64 TfVideoOutput::diff(const struct timeval* tv)
{
	if(_is_first)
	{
		_is_first = false;
		_prev_time = *tv;
	}

	// 
	struct timeval curr_time;
	curr_time = *tv;

	u_int64 diff = (curr_time.tv_sec - _prev_time.tv_sec) * 1000 + (curr_time.tv_usec - _prev_time.tv_usec) / 1000;
	_prev_time = curr_time;

	return diff;

}

