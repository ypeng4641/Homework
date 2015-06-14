#include "Dispatch.h"

#include <network/network.h>

#include "__send_buf.h"
#include "msg.h"
#include "Counter.h"

#include <x_vs/xvs.h>
#include <utils/time.h>

Dispatch* Dispatch::_instance = NULL;

long g_dif = 0l;
unsigned long long g_app_preTime = 0;
unsigned long long g_app_preTime2 = 0;

int g_plugin_nowtime[30] = {0};
int g_plugin_cnt = 0;

int g_cnt[50] = {0};


Dispatch::Dispatch(void)
{
}


Dispatch::~Dispatch(void)
{
}

int Dispatch::Init()
{
	//
	pthread_mutex_init(&_mutex, NULL);

	//
	for(unsigned int i = 0, j = 0; i < SOCKET_POOL_SIZE; i++)
	{
		SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);

		for(; true; j++)
		{
			sockaddr_in sa;
			sa.sin_family = AF_INET;
			sa.sin_addr.s_addr = INADDR_ANY;//inet_addr("127.0.0.1");
			sa.sin_port = htons(FIRST_PORT + j);
			if(bind(s, (sockaddr*)&sa, sizeof(sa)) != SOCKET_ERROR)
			{
				break;
			}
		}

		Network::DefaultDatagram(s);

		Network::SetNonBlock(s);

		_socket_pool.push_back(s);
	}

	//
	if(evutil_socketpair(AF_INET, SOCK_STREAM, 0, _pipe) != 0)
	{
		return -1;
	}

	//
	if(evutil_socketpair(AF_INET, SOCK_STREAM, 0, _exit) != 0)
	{
		return -1;
	}

	Network::DefaultStream(_pipe[0]);
	Network::DefaultStream(_pipe[1]);
	Network::DefaultStream(_exit[0]);
	Network::DefaultStream(_exit[1]);

	//
	if(BeginThread() != 0)
	{
		return -1;
	}

	return 0l;
}

int Dispatch::Deinit()
{
	//
	if(EndThread() != 0)
	{
		return -1;
	}

	//
	evutil_closesocket(_pipe[0]);
	evutil_closesocket(_pipe[1]);

	//
	evutil_closesocket(_exit[0]);
	evutil_closesocket(_exit[1]);

	//
	for(std::vector<SOCKET>::iterator it = _socket_pool.begin();
		it != _socket_pool.end();
		it++)
	{
		Network::Close((*it));
	}

	//
	pthread_mutex_destroy(&_mutex);

	//
	return 0l;
}

int Dispatch::BeginThread()
{
	if(pthread_create(&_thread, NULL, Dispatch::ThreadFunc, this) != 0)
	{
		return -1;
	}

	return 0l;
}

int Dispatch::EndThread()
{
	//
	exitloop();

	//
	pthread_join(_thread, NULL); 

	//
	return 0l;
}

int Dispatch::add(u_int32 ipaddr, u_int16 port)
{
	data_add* d = new data_add;
	d->cmd = cmd_add;
	d->ipaddr = ipaddr;
	d->port = port;

	command((data_common*)d);

	return 0l;
}

int Dispatch::del(u_int32 ipaddr, u_int16 port)
{
	data_del* d = new data_del;
	d->cmd = cmd_del;
	d->ipaddr = ipaddr;
	d->port = port;

	command((data_common*)d);

	return 0l;
}

int Dispatch::push(u_int32 ipaddr, u_int16 port, AUTOMEM* mem)
{
	mem->ref();

	data_push* d = new data_push;
	d->cmd = cmd_push;
	d->ipaddr = ipaddr;
	d->port = port;
	d->datalen = 0;
	d->data = (char*)mem;
	
	command((data_common*)d);

	return 0l;
}


int Dispatch::command(data_common* d)
{
	pthread_mutex_lock(&_mutex); 
	_command_list.push_back(d);
	pthread_mutex_unlock(&_mutex);

	//
	char flag = 0x0;
	if(::send(_pipe[0], &flag, sizeof(flag), 0) != sizeof(flag))
	{
		return -1;
	}

	//
	return 0;

}

Dispatch::data_common* Dispatch::command()
{
	//
	char flag = 0x0;
	if(::recv(_pipe[1], &flag, sizeof(flag), 0) != sizeof(flag))
	{
		return NULL;
	}

	//
	data_common* ret = NULL;

	//
	pthread_mutex_lock(&_mutex); 
	if(_command_list.size() > 0)
	{
		ret = _command_list.front();
		_command_list.pop_front();
	}
	pthread_mutex_unlock(&_mutex);

	//
	return ret;
}

int Dispatch::exitloop()
{
	char flag = 0x0;
	::send(_exit[0], &flag, sizeof(flag), 0);
	return 0l;
}

void* Dispatch::ThreadFunc(void* data)
{
	Dispatch* pThis = reinterpret_cast<Dispatch*>(data);
	pThis->Run();
	return NULL;
}

void Dispatch::Run()
{
	u_int64 basetime = Counter::instance()->tick();

	while(true)
	{
		FD_SET fdset;
		FD_ZERO(&fdset);

		FD_SET(_pipe[1], &fdset);
		FD_SET(_exit[1], &fdset);

		struct timeval tv = {0, TICK_TIME};
		int ret = Network::Select(FD_SETSIZE, &fdset, NULL, NULL, &tv);
		if(ret > 0)
		{
			if(FD_ISSET(_pipe[1], &fdset))
			{
				data_common* d = command();
				if(d)
				{
					process(d);
				}

			}

			if(FD_ISSET(_exit[1], &fdset))
			{
				break;
			}

		}

		u_int64 nowtime = Counter::instance()->tick();

		u_int64 diff = nowtime - basetime;
		if(diff >= TICK_TIME / 1000)
		{
			//
			basetime = nowtime;

			// 
			send(diff*1000/TICK_TIME);
		}

	}
}

void Dispatch::process(data_common* d)
{
	if(d->cmd == cmd_push)
	{
		data_push* p = (data_push*)d;

		std::map<u_int64,item*>::iterator it = _item_list.find(ITEMID(p->ipaddr, p->port));
		if(it != _item_list.end())
		{
#ifdef OPT_MEM_TO_PTR
			AUTOMEM* mem = (AUTOMEM *)p->data;
			VS_VIDEO_PACKET* vsp = reinterpret_cast<VS_VIDEO_PACKET*>(mem->get());
			int sc = SplitFrame_Push(vsp, &((*it).second->packet_list));
			mem->release();
#else
			packet* m = new packet;
			m->datalen = 0; //no use
			m->data = p->data;

			(*it).second->packet_list.push_back(m);
#endif//OPT_MEM_TO_PTR
		}
		else
		{
			AUTOMEM* mem = (AUTOMEM *)p->data;
			mem->release();
		}

		delete p;		
	}
	else if(d->cmd == cmd_add)
	{
		data_add* p = (data_add*)d;

		item* i = new item;
		i->ipaddr = p->ipaddr;
		i->port = p->port;

		_item_list[ITEMID(p->ipaddr, p->port)] = i;

		delete p;

	}
	else if(d->cmd == cmd_del)
	{
		data_del* p = (data_del*)d;

		std::map<u_int64,item*>::iterator it = _item_list.find(ITEMID(p->ipaddr, p->port));
		if(it != _item_list.end())
		{
			// 
			for(std::list<packet*>::iterator it2 = (*it).second->packet_list.begin();
				it2 != (*it).second->packet_list.end();
				it2++)
			{
				AUTOMEM* mem = (AUTOMEM*)(*it2)->data;
				mem->release();

				delete (*it2);
			}

			//
			delete (it->second);

			//
			_item_list.erase(it);
		}

		delete p;
	}
	else
	{
		LOG(LEVEL_ERROR, "don't support command(%d)", d->cmd);
	}
}

void Dispatch::send(int loop)
{
	if(loop > 4)
	{
		LOG(LEVEL_INFO, "#################### LOOP=%d", loop);
	}

	while(loop-- > 0)
	{
		//
		std::list<unsigned int> cntlist;		

		for(std::map<u_int64, item*>::iterator it = _item_list.begin();
			it != _item_list.end();
			it++)
		{	
			if((*it).second->packet_list.size() > 0)
			{
				cntlist.push_back((*it).second->packet_list.size() / TICK_COUNT + 1);
			}
			else
			{
				cntlist.push_back(0);
			}
		}

		// 发送数据
		while(1)
		{
			//
			bool bsend = false;

			//
			std::list<unsigned int>::iterator it1 = cntlist.begin();
			std::map<u_int64, item*>::iterator it2 = _item_list.begin();
			for( ; it1 != cntlist.end() && it2 != _item_list.end(); it1++, it2++)
			{
				if((*it1) > 0)
				{
					// 置发送标识
					bsend = true;

					// 减小需要发送数量
					(*it1)--;

					// init
					item* p1 = (*it2).second;
					packet* p2 = (*it2).second->packet_list.front();

					AUTOMEM* mem = (AUTOMEM*)p2->data;
										
					// 
					__send_buf(_socket_pool[p1->port % SOCKET_POOL_SIZE], p1->ipaddr, p1->port, mem->size(), mem->get());
									
					static int i=0;
					if((i++ % 1000) == 0)
					{
						//LOG(LEVEL_INFO, "------send %08X:%d, size=%d", p1->ipaddr, (u_int32)p1->port, mem->size());
					}
					
#ifdef OPT_DEBUG_OUT
	VS_VIDEO_PACKET* packet = (VS_VIDEO_PACKET*)mem->get();

	struct timeval tv;
	gettimeofday(&tv, NULL);

	ULONGLONG nowtime = ((ULONGLONG)tv.tv_sec * 1000 + tv.tv_usec/1000);

	g_dif = packet->timestamp - nowtime;

	if(g_dif < 0)//(packet->slicecnt > 3 || g_dif < 0)
	{
		union
		{
			unsigned char v[4];
			unsigned int  value;
		}ip;
		ip.value = p1->ipaddr;
	
		pthread_t self_id = pthread_self();
		LOG(LEVEL_INFO, "thread=%d:%u, packet.size=%d, loop=%d, SEND >> addr=%d.%d.%d.%d, port=%d, size=%d. stamp=%llu, framenum=%u, slicecnt=%u, slicenum=%u,nowtimestamp=%llu, dif=%ld.\n"
			, (int)self_id.p, self_id.x
			, p1->packet_list.size(), loop
			, ip.v[3], ip.v[2], ip.v[1], ip.v[0], p1->port
			, mem->size(), packet->timestamp, packet->framenum, packet->slicecnt, packet->slicenum, nowtime, g_dif);
	}
#endif

					// 
					mem->release();
					delete p2;
					p1->packet_list.pop_front();

				}

			}

			// 如果没有数据发送 表示不需要
			if(!bsend)
			{
				return;
			}

		}

	}
}


int Dispatch::SplitFrame_Push(VS_VIDEO_PACKET* vsp, std::list<packet*>* plp)
{
	//取出内存结构体，获取码流数据
	OPT_MEM* omem = (OPT_MEM*)vsp->data;

	unsigned int slicecnt = omem->memLen / VDATA_SLICE_LEN;
	if(omem->memLen % VDATA_SLICE_LEN != 0)
	{
		slicecnt += 1;
	}

	//
	unsigned int used_len = 0;
	unsigned int i = 0;
	for(i = 0; i < slicecnt; i++)
	{
		//
		unsigned int data_size = omem->memLen - used_len;
		if(data_size > VDATA_SLICE_LEN)
		{
			data_size = VDATA_SLICE_LEN;
		}

		//
		unsigned int sumlen = sizeof(VS_VIDEO_PACKET) + data_size;

		AUTOMEM* mem = new AUTOMEM(sumlen);
		VS_VIDEO_PACKET* vspSlice = reinterpret_cast<VS_VIDEO_PACKET*>(mem->get());

		vspSlice->packet_type = 0x0;
		vspSlice->signal_id = vsp->signal_id;
		vspSlice->stream_id = vsp->stream_id;
		vspSlice->base_id = vsp->base_id;
		vspSlice->framenum = vsp->framenum;
		vspSlice->sys_basenum = vsp->sys_basenum;
		vspSlice->sys_framenum = vsp->sys_framenum;
		vspSlice->replay = vsp->replay;
		vspSlice->slicecnt = slicecnt;
		vspSlice->slicenum = i;
		vspSlice->timestamp = vsp->timestamp;
		vspSlice->area = vsp->area;
		vspSlice->resol = vsp->resol;
		vspSlice->valid = vsp->valid;
		vspSlice->codec = vsp->codec;
		vspSlice->frametype = vsp->frametype;
		vspSlice->datalen = data_size;
		memcpy(vspSlice->data, omem->memData + used_len, data_size);

		//
		used_len += data_size;
				
		//
		packet* m = new packet();
		m->data = (char*)mem;
		m->datalen = 0;//nouse
		plp->push_back(m);
	}

	//delete[] omem->memData;
	return i;
}
