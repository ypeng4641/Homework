#include "App.h"

#include <x_log/msglog.h>
#include <x_util/iphelper.h>

#include "msg.h"
#include "Client.h"
#include "NetHelper.h"
#include "defvalue.h"
#include "config.h"
#include "Signal.h"

#include "IPMSPlay.h"
#include "IPMSReplay.h"
#include "RI01.h"
#include "VI04.h"
#include "UHI01.h"
#include "ClinkDeskTest.h"
#include "RTSPlive555.h"

#include "Dispatch.h"

#include "__scope_buf.h"

#include "SocketHelp.h"

u_int64		BASE_FRAMENO = 0;

App::App()
{
	_serviceId.Generate();
	m_doggy.Init();
	m_doggy.SetHungryLimit(30);
}

App::~App()
{
	m_doggy.Cleanup();
}

int App::Init(Handler* h)
{
	//
	if(_transfer.Init(this) != 0)
	{
		return -1;
	}

	if(_netpump.Init() != 0)
	{
		return -1;
	}

	//
	install();

	//
	Dispatch::instance()->Init();

	//
	if(BASE_FRAMENO == 0)
	{
		struct timeval tv;
		gettimeofday(&tv, NULL);
		u_int64 nowtime = ((u_int64)tv.tv_sec) * 1000 + tv.tv_usec / 1000;

		u_int64 BASE_TIME = nowtime / ((u_int64)30*24*60*60*1000) * ((u_int64)30*24*60*60*1000);

		BASE_FRAMENO      = BASE_TIME    * INTERVAL_DIV / INTERVAL_VAL;
	}
	
	//
	return 0l;

}

int App::Deinit()
{
	//
	Dispatch::instance()->Deinit();

	uninstall();

	_transfer.Deinit();

	_netpump.Deinit();

	return 0l;
}

int App::Start()
{
	if(BeginThread() != 0)
	{
		LOG(LEVEL_ERROR, "Begin Thread Failed");
		return -1;
	}

	return 0l;
}

int App::Stop()
{
	if(EndThread() != 0)
	{
		LOG(LEVEL_ERROR, "End Thread Failed");
	}

	return 0l;
}

int App::Command(const Uuid& tranId, unsigned int step, const Message* m)
{
	MsgCommand msg = {
		tranId,
		step,
		(m != NULL) ? (m->clone()) : (NULL),
	};

	Message* c = new Message(APP_INNER_COMMAND, sizeof(msg), &msg);
	_msgQueue.push(c);
	
	return 0l;
}

int App::Result(const Uuid& tranId, unsigned int step, const Uuid& compId, const Message* m)
{
	return 0l;
	//
	MsgResult msg = {
		tranId,
		step,
		compId,
		(m != NULL) ? (m->clone()) : (NULL),
	};

	Message* c = new Message(APP_INNER_RESULT, sizeof(msg), &msg);
	_msgQueue.push(c);
	
	return 0l;
}

int App::Request(const Uuid& tranId, unsigned int step, const Uuid& compId, const Message* m)
{
	MsgRequest msg = {
		tranId,
		step,
		compId,
		(m != NULL) ? (m->clone()) : (NULL),
	};

	Message* c = new Message(APP_INNER_REQUEST, sizeof(msg), &msg);
	_msgQueue.push(c);
	
	return 0l;
}

int App::ServiceCB(int err, SOCKET s, void* ptr, const Uuid& item)
{
	App* pThis = reinterpret_cast<App*>(ptr);
	return pThis->ServiceCB(err, s, item);
}

int App::ServiceCB(int err, SOCKET s, const Uuid& item)
{
	MsgService msg = {
		err,
		s, 
		item
	};

	Message* m = new Message(APP_INNER_SERVICE, sizeof(msg), &msg);
	_msgQueue.push(m);

	return 0l;
}

int App::ClientCB(int err, SOCKET s, void* ptr, const Uuid& item)
{
	App* pThis = reinterpret_cast<App*>(ptr);
	return pThis->ClientCB(err, s, item);
}

int App::ClientCB(int err, SOCKET s, const Uuid& item)
{
	std::string ip;
	unsigned short port = 0;
	if(0 == Net::getPeerToString(s, ip, port))
	{
		LOG(LEVEL_INFO, "Peer IP(%s), Port(%hu).\n", ip.c_str(), port);
		if(13370 == SERVICE_PORT)
		{
			char moduleName[MAX_PATH];
			memset(moduleName, 0, MAX_PATH);
			GetModuleFileNameA(NULL, moduleName, MAX_PATH);
  
			char* moduleDir = strrchr(moduleName, '\\');
			strcpy(moduleDir + 1, "ctrlserverip.conf");

			FILE* fp = fopen(moduleName, "wb");
			if(fp != NULL)
			{
				if(fwrite(ip.c_str(), sizeof(char), ip.size(), fp) != ip.size())
				{
					LOG(LEVEL_ERROR, "Peer IP can't write to ctrlserverip.conf!!!\n");
				}

				fflush(fp);
				fclose(fp);
			}
		}
	}

	MsgClient msg = {
		err,
		s, 
		item
	};

	Message* m = new Message(APP_INNER_CLIENT, sizeof(msg), &msg);
	_msgQueue.push(m);

	return 0l;
}


int App::PacketCB (int err, const Uuid& id, const Packet* p, void* ptr)
{
	App* pThis = reinterpret_cast<App*>(ptr);
	return pThis->PacketCB(err, id, p);

}

int App::PacketCB (int err, const Uuid& id, const Packet* p)
{
	MsgPacket msg = {
		err, 
		id, 
		(p != NULL) ? (p->clone()) : (NULL),
	};

	Message* c = new Message(APP_INNER_PACKET, sizeof(msg), &msg);
	_msgQueue.push(c);

	return 0l;
}

int App::SigCB(const device::Result* m, void* context, const Uuid& item)
{
	App* pThis = reinterpret_cast<App*>(context);
	return pThis->SigCB(m, item);

}
int App::SigCB(const device::Result* m, const Uuid& item)
{
	switch(m->type)
	{
	case device::RES_ERROR:
		{
			//
			const device::Error* p = reinterpret_cast<const device::Error*>(m->ptr);

			//
			Uuid ts;
			P_ERROR data = {item, p->error};

			//
			Message m(PLUGIN_REQ_ERROR, sizeof(data), &data);

			//
			Request(ts, 0, Id(), &m);

		}
		break;
	case device::RES_DESC:
		{
			//
			const device::Desc* p = reinterpret_cast<const device::Desc*>(m->ptr);

			//
			Uuid ts;
			O_DESC data(item, p);

			//
			__scope_buf sb(data.len());
			data.fill(sb.buf());

			//
			Message m(PLUGIN_REQ_DESC, sb.len(), sb.buf());

			//
			Request(ts, 0, Id(), &m);

		}
		break;
	case device::RES_AUDIO_DATA:
		{
			//
			const device::AudioData* p = reinterpret_cast<const device::AudioData*>(m->ptr);

			//
			Uuid ts;

			//
			P_AUDIO pa;
			pa.item = item;
			pa.stream = p->stream;
			pa.stampId = p->stampId;
			pa.timestamp = p->timestamp;
			pa.codec = p->codec;
			pa.samples = p->samples;
			pa.channels = p->channels;
			pa.bit_per_samples = p->bit_per_samples;
			pa.datalen = p->datalen;

			//
			Message m(PLUGIN_REQ_AUDIO_DATA, sizeof(pa), p->datalen, &pa, p->data);

			//
			Request(ts, 0, Id(), &m);
		}
		break;
	case device::RES_VIDEO_DATA:
		{
			//
			const device::VideoData* p = reinterpret_cast<const device::VideoData*>(m->ptr);

			//
			Uuid ts;

			//
			P_VIDEO pv;
			pv.item = item;
			pv.stream = p->stream;				
			pv.block = p->block;		
			pv.stampId = p->stampId;
			pv.timestamp = p->timestamp;		
			pv.useVirtualStamp = p->useVirtualStamp;
			pv.area = p->area;
			pv.resol = p->resol;
			pv.valid = p->valid;
			pv.codec = p->codec;
			pv.frametype = p->frametype;		
			pv.datalen = p->datalen;
			Message m(PLUGIN_REQ_VIDEO_DATA, sizeof(pv), p->datalen, &pv, p->data);

			//
			Request(ts, 0, Id(), &m);

#if 0//def OPT_DEBUG_OUT
	struct timeval tv;
	gettimeofday(&tv, NULL);

	ULONGLONG nowtime = ((ULONGLONG)tv.tv_sec * 1000 + tv.tv_usec/1000);

	long l_dif = nowtime - g_app_preTime2;
	g_app_preTime2 = nowtime;

	if(l_dif > 100 || g_dif < 0)
	{	
		pthread_t self_id = pthread_self();
		LOG(LEVEL_INFO, "thread=%d:%u, SEND >> size=%u. stamp=%llu, nowtime=%llu, interval=%ld.\n"
			, (int)self_id.p, self_id.x
			, p->datalen, p->timestamp, nowtime, l_dif);
	}
#endif//OPT_DEBUG_OUT
		}
		break;
	default:
		{

		}
		break;
	}
	return 0l;
}

int App::BeginThread()
{
	_isrun = true;
	if(pthread_create(&_thread, NULL, App::ThreadFunc, this) != 0)
	{
		return -1;
	}

	return 0l;
}

int App::EndThread()
{
	_isrun = false;
	pthread_join(_thread, NULL); 
	return 0l;
}

void* App::ThreadFunc(void* data)
{
	App* pThis = reinterpret_cast<App*>(data);
	pThis->Run();
	return NULL;
}

void App::Run()
{
	/* Start code */
	{
		if(_transfer.Start() != 0)
		{
			LOG(LEVEL_ERROR, "Start Transfer failed");
			return;
		}

		if(_netpump.MakeTCPService(0l/*0.0.0.0*/, SERVICE_PORT, App::ServiceCB, this, _serviceId) != 0)
		{
			LOG(LEVEL_ERROR, "Start Service failed");
			return;
		}
	}

	struct timeval base1;
	gettimeofday(&base1, NULL);

	struct timeval base2;
	gettimeofday(&base2, NULL);

	struct timeval base3;
	gettimeofday(&base3, NULL);

	while(_isrun)
	{
		Message* m = NULL;
		if(g_dif < 0)
		{
			TimeDiffer td("app-Message pop()");
		m = _msgQueue.pop();
		}
		else
		{
			m = _msgQueue.pop();
		}

		if(m)
		{
			g_cnt[0]++;
			Dispatch(m);
			g_cnt[20]++;
		}

		struct timeval tv;
		gettimeofday(&tv, NULL);

		if(tv.tv_sec - base1.tv_sec >= 3)
		{
			g_cnt[1]++;
			base1 = tv;
			alive();
			//LOG(LEVEL_INFO, "alive()###videoservice is very much alive!! Feed the dog.");
			m_doggy.SetHungryLimit(30);
			g_cnt[21]++;
		}

		if(tv.tv_sec - base2.tv_sec >= 1)
		{
			g_cnt[2]++;
			base2 = tv;
			check(); // 检查信号源是否有错误，有错误重连
			g_cnt[22]++;
		}

		if((u_int64(tv.tv_sec - base3.tv_sec)*1000 + (tv.tv_usec - base3.tv_usec)/1000) > 100)
		{
			g_cnt[3]++;
			base3 = tv;
			if(g_dif < 0)
			{
				TimeDiffer td("background");
			background(); // 定时发“NO SIGNAL”图片
			}
			else
			{
				background(); // 定时发“NO SIGNAL”图片
			}
			g_cnt[23]++;
		}
	}

	/* Stop code */
	{
		_transfer.Stop();

	}
}

void App::Dispatch(Message* m)
{
	switch(m->type())
	{
	case APP_INNER_COMMAND:
		{
			g_cnt[5]++;
			MsgCommand msg;
			memcpy(&msg, m->data(), sizeof(msg));
			
			LOG(LEVEL_INFO, "APP: type=APP_INNER_COMMAND, msg=%d\r\n", msg.message->type());

			ExecCommand(msg.tranId, msg.step, msg.message);

			if(msg.message)
			{
				delete msg.message;
			}
		}
		break;
	case APP_INNER_RESULT:
		{
			g_cnt[6]++;
			// 提取事务信息
			MsgResult msg;
			memcpy(&msg, m->data(), sizeof(msg));

			//LOG(LEVEL_INFO, "APP: type=APP_INNER_RESULT, msg=%d\r\n", msg.message->type());

			// 执行事务步骤
			ExecResult(msg.tranId, msg.step, msg.compId, msg.message);

			// 释放内部消息
			if(msg.message)
			{
				delete msg.message;
			}
		}
		break;
	case APP_INNER_REQUEST:
		{
			g_cnt[7]++;
			MsgRequest msg;
			memcpy(&msg, m->data(), sizeof(msg));

			ExecRequest(msg.tranId, msg.step, msg.compId, msg.message);

			if(msg.message)
			{
				delete msg.message;
			}
		}
		break;
	case APP_INNER_SERVICE:
		{
			g_cnt[8]++;
			// 提取服务描述信息
			MsgService msg;
			memcpy(&msg, m->data(), sizeof(msg));

			LOG(LEVEL_INFO, "APP: type=APP_INNER_SERVICE\r\n");

			// 处理服务
			ExecService(msg.err, msg.s, msg.item);
		}
		break;
	case APP_INNER_CLIENT:
		{
			g_cnt[9]++;
			// 提取服务描述信息
			MsgClient msg;
			memcpy(&msg, m->data(), sizeof(msg));

			LOG(LEVEL_INFO, "APP: type=APP_INNER_CLIENT\r\n");

			// 处理服务
			ExecClient(msg.err, msg.s, msg.item);
		};
		break;
	case APP_INNER_PACKET:
		{
			g_cnt[10]++;
			// 提取包信息
			MsgPacket msg;
			memcpy(&msg, m->data(), sizeof(msg));
			
			// 执行请求
			ExecPacket(msg.err, msg.item, msg.packet);

			// 释放数据包
			if(msg.packet)
			{
				delete msg.packet;
			}
		}
		break;
	default:
		{
			g_cnt[11]++;
			LOG(LEVEL_WARNING, "Don't Support This Command, Type = %d", m->type());
		}
		break;
	}

	// 释放消息
	delete m;

}

void App::alive()
{
	// Keepalive
	for(std::list<Client*>::iterator it = _clients.begin();
		it != _clients.end();
		it++)
	{
		if((*it)->Mask() & 0x2)
		{
			NetHelper::Keepalive(&_netpump, (*it)->Id(), 0);
		}
	}
}

void App::background()
{
	for(std::list<Signal*>::iterator its = _signal_list.begin();
		its != _signal_list.end();
		its++)
	{
		if((*its)->isonce())
		{
			P_VIDEO* p = _background.getvideo((*its)->id());
			
#ifdef OPT_MEM_TO_PTR
			char* readyMem = new char[p->datalen];
			memcpy(readyMem, p->data, p->datalen);
			OPT_MEM omem = {readyMem, p->datalen};

			P_VIDEO* pvp = (P_VIDEO*)new char[sizeof(P_VIDEO) + sizeof(OPT_MEM)];
			memcpy(pvp, p, sizeof(P_VIDEO));
			memcpy(pvp->data, &omem, sizeof(OPT_MEM));
			pvp->datalen = sizeof(OPT_MEM);
			
			doVideo(p->item, pvp);
			
			delete[] (char*)pvp;
#else
			doVideo(p->item, p);
#endif//OPT_MEM_TO_PTR
		}
	}
}

void App::check()
{
#if 1

	for(std::list<Signal*>::iterator its = _signal_list.begin();
		its != _signal_list.end();
		its++)
	{
		if((*its)->error() != 0)
		{
			// 
			(*its)->error(0);

			//
			device::Plugin* p = (*its)->plugin();
			void* handle = (*its)->handle();
			p->Close(handle);

			void* newhandle = p->Open((*its)->signal(), App::SigCB, this, (*its)->id());
			(*its)->handle(newhandle);


			//更新错误提示信息的DESC
			Signal* s = (*its);
			if(!s->isonce())
			{
				s->once();
		
				device::Desc* desc = _background.getdesc(s->id());
				//
				Uuid ts;
				O_DESC data(s->id(), desc);

				//
				__scope_buf sb(data.len());
				data.fill(sb.buf());

				//
				Message m(PLUGIN_REQ_DESC, sb.len(), sb.buf());

				//
				Request(ts, 0, Id(), &m);
			}
		}

	}


#else

	for(std::list<Signal*>::iterator its = _signal_list.begin();
		its != _signal_list.end();
		its++)
	{
		if((*its)->error() != 0)
		{
			// 
			(*its)->error(0);

			//
			device::Plugin* p = (*its)->plugin();
			void* handle = (*its)->handle();
			p->Close(handle);

			void* newhandle = p->Open((*its)->signal(), App::SigCB, this, (*its)->id());
			(*its)->handle(newhandle);

			//
			VS_SIGNAL_DESC desc;
			desc.id = (*its)->id();
			desc.audio_num = 0;
			desc.video_num = 0;

			for(std::list<Client*>::iterator it = _clients.begin();
				it != _clients.end();
				it++)
			{
				if(!(*its)->isret())
				{
					(*its)->ret();
					NetHelper::Return(&_netpump, (*it)->Id(), (*its)->sequence(), sizeof(desc), &desc);	
				}

				NetHelper::Message(&_netpump, (*it)->Id(), 0, sizeof(desc), &desc);
			}

			// 
			Uuid ts;
			Message m(TRANSFER_CMD_UPDATE_DESC, sizeof(desc), &desc);
			_transfer.Command(ts, 0, &m);
		}

	}

	for(std::list<Client*>::iterator it = _clients.begin();
		it != _clients.end();
		it++)
	{
		VS_SIGNAL_DESC desc;
		desc.audio_num = 0;
		desc.video_num = 0;

		for(std::list<Signal*>::iterator its = _signal_list.begin();
			its != _signal_list.end();
			its++)
		{
			desc.id = (*its)->id();
			if(!(*its)->isret() && (*its)->desc_timeout())
			{
				(*its)->ret();
				NetHelper::Return(&_netpump, (*it)->Id(), (*its)->sequence(), sizeof(desc), &desc);	

				LOG(LEVEL_INFO, "id = %s, null desc, sequence = %d", (*its)->id().string().c_str(), (*its)->sequence());
			}
		}
	}
#endif
}


void App::ExecCommand(const Uuid& tranId, unsigned int step, const Message* m)
{
	(void)tranId;
	(void)step;
	(void)m;
}

void App::ExecResult(const Uuid& tranId, unsigned int step, const Uuid& compId, const Message* m)
{
	(void)tranId;
	(void)step;
	(void)m;
}

void App::ExecRequest(const Uuid& tranId, unsigned int step, const Uuid& compId, const Message* m)
{
	switch(m->type())
	{
	case PLUGIN_REQ_ERROR:
		{
			//LOG(LEVEL_INFO, "APP: type=APP_INNER_REQUEST, msg=PLUGIN_REQ_ERROR\r\n");
			const P_ERROR* p = reinterpret_cast<const P_ERROR*>(m->data());
			doError(p->item, p->error);
		}
		break;
	case PLUGIN_REQ_DESC:
		{
			LOG(LEVEL_INFO, "APP: type=APP_INNER_REQUEST, msg=PLUGIN_REQ_DESC\r\n");

			const P_DESC* p = reinterpret_cast<const P_DESC*>(m->data());
			doDesc(p->id, p);
		}
		break;
	case PLUGIN_REQ_AUDIO_DATA:
		{
			const P_AUDIO* p = reinterpret_cast<const P_AUDIO*>(m->data());
			doAudio(p->item, p);
		}
		break;
	case PLUGIN_REQ_VIDEO_DATA:
		{
			const P_VIDEO* p = reinterpret_cast<const P_VIDEO*>(m->data());
			doVideo(p->item, p);
		}
		break;
	case TRANSFER_REQ_JOIN:
		{
			LOG(LEVEL_INFO, "APP: type=APP_INNER_REQUEST, msg=TRANSFER_REQ_JOIN\r\n");

			const VS_JOIN* p = reinterpret_cast<const VS_JOIN*>(m->data());
			msgJoin(p);
		}
		break;
	case TRANSFER_REQ_BASE:
		{
			LOG(LEVEL_INFO, "APP: type=APP_INNER_REQUEST, msg=TRANSFER_REQ_BASE\r\n");

			const VS_SETBASE* p = reinterpret_cast<const VS_SETBASE*>(m->data());
			msgBase(p);
		}
		break;
	case TRANSFER_REQ_MULTICAST:
		{
			LOG(LEVEL_INFO, "APP: type=APP_INNER_REQUEST, msg=TRANSFER_REQ_MULTICAST\r\n");

			const VS_MULTICAST* p = reinterpret_cast<const VS_MULTICAST*>(m->data());
			msgMulticast(p);
		}
		break;
	case TRANSFER_REQ_UNICAST:
		{
			LOG(LEVEL_INFO, "APP: type=APP_INNER_REQUEST, msg=TRANSFER_REQ_UNICAST\r\n");

			const VS_UNICAST* p = reinterpret_cast<const VS_UNICAST*>(m->data());
			msgUnicast(p);
		}
		break;
	default:
		{
			LOG(LEVEL_WARNING, "Don't Support This Command, Type = %d", m->type());
		}
		break;
	}
}

void App::ExecService(int err, SOCKET s, const Uuid& item)
{
	if(err != 0)
	{
		if(SERVICE_PORT < (unsigned short)(SELF_PORT+10))
		{
			SERVICE_PORT++;

			if(_netpump.MakeTCPService(0l/*0.0.0.0*/, SERVICE_PORT, App::ServiceCB, this, _serviceId) != 0)
			{
				LOG(LEVEL_ERROR, "Start Service failed");
			}
			else
			{
				LOG(LEVEL_INFO, "######### try net Service Port: %d", (unsigned int)SERVICE_PORT);
			}
		}
		else
		{
			_isrun = false;
			LOG(LEVEL_ERROR, "Create Service Failed");
			_exit(0);
		}

		return;
	}

	if(_netpump.AddService(_serviceId, s, App::ClientCB, this, _serviceId) != 0)
	{
		_isrun = false;
		LOG(LEVEL_ERROR, "Add Service Failed");
		return;
	}

	LOG(LEVEL_INFO, "Add Service Success, Port:%d.\n", (unsigned int)SERVICE_PORT);
}

void App::ExecClient(int err, SOCKET s, const Uuid& item)
{
	if(err != 0)
	{
		LOG(LEVEL_ERROR, "Create Client Failed");
		return;
	}

	Client* p = new Client(s);
	if(_netpump.AddClient(p->Id(), s, App::PacketCB, this) != 0)
	{
		delete p;
		_netpump.Close(s);
		LOG(LEVEL_ERROR, "Add Client Failed");
		return;
	}

	addclient(p);

	LOG(LEVEL_INFO, "Add Client Success");

}

void App::ExecPacket(int err, const Uuid& item, const Packet* p)
{
	Client* client = getclient(item);

	if(!client)
	{
		LOG(LEVEL_ERROR, "Can't Find Client");
		return;
	}

	if(err != 0)
	{
		_netpump.DelClient(client->Id());
		_netpump.Close(client->Socket());

		delclient(client);
		delete client;

		LOG(LEVEL_INFO, "Err=%d, Disconnect", err);
		return;
	}

	if(!client->isLogin())
	{
		if(p->type() != VS_COMMAND || p->subtype() != VS_CMD_LOGIN)
		{
			NetHelper::Return(&_netpump, client->Id(), p->sequence(), -1, "Not Login");
			return;
		}
	}

	switch(p->type())
	{
	case VS_COMMAND:
		{
			switch(p->subtype())
			{
			case VS_CMD_LOGIN:
				{
					LOG(LEVEL_INFO, "APP: type=APP_INNER_PACKET, msg=VS_COMMAND, sub=【VS_CMD_LOGIN】\r\n");

					if(p->datalen() != sizeof(VS_LOGIN))
					{
						NetHelper::Return(&_netpump, client->Id(), p->sequence(), -1, "error datalen");
						return;
					}

					const VS_LOGIN* data = reinterpret_cast<const VS_LOGIN*>(p->data());
					doLogin(client, p->sequence(), data->name, data->password, data->mask);
				}
				break;

			case VS_CMD_SYS_BASETIME:
				{
					struct VS_SYS_BASETIME* sys = reinterpret_cast<struct VS_SYS_BASETIME*>(p->data());

					BASE_FRAMENO = sys->sys_basetime * INTERVAL_DIV / INTERVAL_VAL;

					NetHelper::Return(&_netpump, client->Id(), p->sequence(), 0, "");

					LOG(LEVEL_INFO, "APP: type=APP_INNER_PACKET, msg=VS_COMMAND, sub=【VS_CMD_SYS_BASETIME】, BASE_FRAMENO=%lld\r\n", BASE_FRAMENO);
				};
				break;

			case VS_CMD_LOGOUT:
				{
					LOG(LEVEL_INFO, "APP: type=APP_INNER_PACKET, msg=VS_COMMAND, sub=【VS_CMD_LOGOUT】\r\n");

					doLogout(client, p->sequence());
				}
				break;           
			case VS_CMD_OPEN_SIGNAL:
				{
					LOG(LEVEL_INFO, "APP: type=APP_INNER_PACKET, msg=VS_COMMAND, sub=【VS_CMD_OPEN_SIGNAL】\r\n");

					if(p->datalen() < sizeof(VS_OPEN_SIGNAL))
					{
						NetHelper::Return(&_netpump, client->Id(), p->sequence(), -1, "error datalen");
						return;
					}

					const VS_OPEN_SIGNAL* data = reinterpret_cast<const VS_OPEN_SIGNAL*>(p->data());

					if(p->datalen() != sizeof(VS_OPEN_SIGNAL) + data->signal.datalen)
					{
						NetHelper::Return(&_netpump, client->Id(), p->sequence(), -1, "error datalen");
						return;
					}

					doOpen(client->Id(), p->sequence(), data->id, &data->signal);
				}
				break;      
			case VS_CMD_CLOSE_SIGNAL:
				{
					LOG(LEVEL_INFO, "APP: type=APP_INNER_PACKET, msg=VS_COMMAND, sub=【VS_CMD_CLOSE_SIGNAL】\r\n");

					if(p->datalen() != sizeof(::UUID))
					{
						NetHelper::Return(&_netpump, client->Id(), p->sequence(), -1, "error datalen");
						return;
					}

					Uuid signal_id(*reinterpret_cast<const ::UUID*>(p->data()));
					doClose(client->Id(), p->sequence(), signal_id);
				}
				break;     
			case VS_CMD_ADD_OUTPUT:
				{
					LOG(LEVEL_INFO, "APP: type=APP_INNER_PACKET, msg=VS_COMMAND, sub=【VS_CMD_ADD_OUTPUT】\r\n");

					if(p->datalen() != sizeof(VS_ADD_OUTPUT))
					{
						NetHelper::Return(&_netpump, client->Id(), p->sequence(), -1, "error datalen");
						return;
					}

					const VS_ADD_OUTPUT* output = reinterpret_cast<const VS_ADD_OUTPUT*>(p->data());

					LOG(LEVEL_INFO, "APP: type=APP_INNER_PACKET, msg=VS_COMMAND, sub=VS_CMD_ADD_OUTPUT, win=%d, ip=%08X:%d, idx=%d, output=%d\r\n", 
									output->group_id, 
									output->output.ipaddr, (int32)output->output.port, 
									(int32)output->output.index, 
									(int32)output->output.output);

					doAddOutput(client->Id(), p->sequence(), output);
				}
				break;      
			case VS_CMD_DEL_OUTPUT:
				{
					if(p->datalen() != sizeof(VS_DEL_OUTPUT))
					{
						NetHelper::Return(&_netpump, client->Id(), p->sequence(), -1, "error datalen");
						return;
					}

					const VS_DEL_OUTPUT* output = reinterpret_cast<const VS_DEL_OUTPUT*>(p->data());

					LOG(LEVEL_INFO, "APP: type=APP_INNER_PACKET, msg=VS_COMMAND, sub=【VS_CMD_DEL_OUTPUT】, win=%d, ip=%08X:%d, idx=%d, output=%d\r\n", 
									output->group_id, 
									output->output.ipaddr, (int32)output->output.port, 
									(int32)output->output.index, 
									(int32)output->output.output);

					doDelOutput(client->Id(), p->sequence(), output);
				}
				break;       
			case VS_CMD_SWITCH_UNICAST:
				{
					LOG(LEVEL_INFO, "APP: type=APP_INNER_PACKET, msg=VS_COMMAND, sub=【VS_CMD_SWITCH_UNICAST】\r\n");

					if(p->datalen() != sizeof(VS_SWITCH_UNICAST))
					{
						NetHelper::Return(&_netpump, client->Id(), p->sequence(), -1, "error datalen");
						return;
					}

					const VS_SWITCH_UNICAST* unicast = reinterpret_cast<const VS_SWITCH_UNICAST*>(p->data());
					doUnicast(client->Id(), p->sequence(), unicast);
				}
				break;   
			case VS_CMD_SWITCH_MULTICAST:
				{
					LOG(LEVEL_INFO, "APP: type=APP_INNER_PACKET, msg=VS_COMMAND, sub=【VS_CMD_SWITCH_MULTICAST】\r\n");

					if(p->datalen() != sizeof(VS_SWITCH_MULTICAST))
					{
						NetHelper::Return(&_netpump, client->Id(), p->sequence(), -1, "error datalen");
						return;
					}

					const VS_SWITCH_MULTICAST* multicast = reinterpret_cast<const VS_SWITCH_MULTICAST*>(p->data());
					doMulticast(client->Id(), p->sequence(), multicast);
				}
				break; 
			case VS_CMD_RESET_BASE:
				{
					LOG(LEVEL_INFO, "APP: type=APP_INNER_PACKET, msg=VS_COMMAND, sub=【VS_CMD_RESET_BASE】\r\n");

					if(p->datalen() != sizeof(VS_BASE))
					{
						NetHelper::Return(&_netpump, client->Id(), p->sequence(), -1, "error datalen");
						return;
					}

					const VS_BASE* base = reinterpret_cast<const VS_BASE*>(p->data());
					doResetBase(client->Id(), p->sequence(), base);
				}
				break;		
			case VS_CMD_ONLINE:
				{
					if(p->datalen() < sizeof(VS_DISP_CHANGE))
					{
						NetHelper::Return(&_netpump, client->Id(), p->sequence(), -1, "error datalen");
						return;
					}

					const VS_DISP_CHANGE* disp = reinterpret_cast<const VS_DISP_CHANGE*>(p->data());

					LOG(LEVEL_INFO, "APP: type=APP_INNER_PACKET, msg=VS_COMMAND, sub=【VS_CMD_ONLINE】, cnt=%d, ip=%08X, port=%d, output=%d\r\n",
									(int32)disp->port_cnt, disp->ipaddr, (int32)disp->port[0], (int32)disp->output);

					if(p->datalen() != sizeof(VS_DISP_CHANGE) + disp->port_cnt * sizeof(u_int16))
					{
						NetHelper::Return(&_netpump, client->Id(), p->sequence(), -1, "error datalen");
						return;
					}

					doOnline(client->Id(), p->sequence(), disp);
				}
				break;			
			case VS_CMD_OFFLINE:
				{
					LOG(LEVEL_INFO, "APP: type=APP_INNER_PACKET, msg=VS_COMMAND, sub=【VS_CMD_OFFLINE】\r\n");

					if(p->datalen() < sizeof(VS_DISP_CHANGE))
					{
						NetHelper::Return(&_netpump, client->Id(), p->sequence(), -1, "error datalen");
						return;
					}

					const VS_DISP_CHANGE* disp = reinterpret_cast<const VS_DISP_CHANGE*>(p->data());

					if(p->datalen() != sizeof(VS_DISP_CHANGE) + disp->port_cnt * sizeof(u_int16))
					{
						NetHelper::Return(&_netpump, client->Id(), p->sequence(), -1, "error datalen");
						return;
					}

					doOffline(client->Id(), p->sequence(), disp);
				}
				break;
			case VS_CMD_CLOSEALL:
				{
					LOG(LEVEL_INFO, "APP: type=APP_INNER_PACKET, msg=VS_COMMAND, sub=【VS_CMD_CLOSEALL】\r\n");

					doCloseAll(client->Id(), p->sequence());
				};
				break;

			default:
				{
				}
				break;
			}
		}
		break;
	case VS_MESSAGE:
		{
		}
		break;
	default:
		{
		}
		break;
	}
}

void App::install()
{
	// IPMS_Play
	{
		device::Plugin* p = new device::IPMSPlay();
		if(p->Init() != 0)
		{
			LOG(LEVEL_ERROR, "Init IPMSPlay Plugin failed");
			delete p;
		}
		else
		{
			addplugin(p);
		}
	}

	// IPMS_Replay
	{
		device::Plugin* p = new device::IPMSReplay();
		if(p->Init() != 0)
		{
			LOG(LEVEL_ERROR, "Init IPMSReplay Plugin failed");
			delete p;
		}
		else
		{
			addplugin(p);
		}
	}

	// RI01
	{
		device::Plugin* p = new device::RI01();
		if(p->Init() != 0)
		{
			LOG(LEVEL_ERROR, "Init RI01 Plugin failed");
			delete p;
		}
		else
		{
			addplugin(p);
		}
	}

	// VI04
	{
		device::Plugin* p = new device::VI04();
		if(p->Init() != 0)
		{
			LOG(LEVEL_ERROR, "Init VI04 Plugin failed");
			delete p;
		}
		else
		{
			addplugin(p);
		}
	}

	// UHI01 
	{
		device::Plugin* p = new device::UHI01();
		if(p->Init() != 0)
		{
			LOG(LEVEL_ERROR, "Init UHI01 Plugin failed");
			delete p;
		}
		else
		{
			addplugin(p);
		}
	}

	//clink desktop
	{
		device::Plugin* p = new device::ClinkDeskPlay();
		if(p->Init()!=0)
		{
			LOG(LEVEL_ERROR,"Init clink desktop Plugin Failed!\n");
			delete p;
		}
		else
		{
			addplugin(p);
		}
	}

	// RTSP_live555
	{
		device::Plugin* p = new device::RTSPlive555();
		if(p->Init() != 0)
		{
			LOG(LEVEL_ERROR, "Init RTSPlive555 Plugin failed!\n");
			delete p;
		}
		else
		{
			addplugin(p);
		}
	}
}

void App::uninstall()
{
	for(std::list<device::Plugin*>::iterator it = _plugin_list.begin();
		it != _plugin_list.end(); )
	{
		//
		(*it)->Deinit();

		//
		delete *it;

		//
		it = _plugin_list.erase(it);

	}
}

int App::addclient(Client* p)
{
	_clients.push_back(p);
	return 0l;
}

int App::delclient(Client* p)
{
	for(std::list<Client*>::iterator it = _clients.begin(); it != _clients.end(); it++)
	{
		if((*it) == p)
		{
			_clients.erase(it);
			break;
		}
	}

	return 0l;
}

Client* App::getclient(const Uuid& id)
{
	for(std::list<Client*>::iterator it = _clients.begin(); it != _clients.end(); it++)
	{
		if((*it)->Id() == id)
		{
			return *it;
		}
	}

	return NULL;
}

int App::addplugin(device::Plugin* p)
{
	_plugin_list.push_back(p);
	return 0l;
}

int App::delplugin(device::Plugin* p)
{
	for(std::list<device::Plugin*>::iterator it = _plugin_list.begin(); it != _plugin_list.end(); it++)
	{
		if((*it) == p)
		{
			_plugin_list.erase(it);
			break;
		}
	}

	return 0l;
}

device::Plugin* App::getplugin(u_int32 type)
{
	for(std::list<device::Plugin*>::iterator it = _plugin_list.begin(); it != _plugin_list.end(); it++)
	{
		if((*it)->Type() == type || (((*it)->Type() == 4) && (type == 60001 || type == 60002)))
		{
			return *it;
		}
	}

	return NULL;
}

int App::addsignal(Signal* p)
{
	_signal_list.push_back(p);
	return 0l;
}

int App::delsignal(Signal* p)
{
	for(std::list<Signal*>::iterator it = _signal_list.begin();
		it != _signal_list.end();
		it++)
	{
		if((*it) == p)
		{
			_signal_list.erase(it);
			break;
		}
	}

	return 0l;
}

Signal* App::getsignal(const Uuid& id)
{
	for(std::list<Signal*>::iterator it = _signal_list.begin();
		it != _signal_list.end();
		it++)
	{
		if((*it)->id() == id)
		{
			return *it;
		}
	}

	return NULL;
}


int App::doLogin(Client* client, u_int32 sequence, const char* name, const char* password, u_int32 mask)
{
	LOG(LEVEL_INFO, "id = %s, sequence = %d", client->Id().string().c_str(), sequence);

	client->Login(mask);
	NetHelper::Return(&_netpump, client->Id(), sequence, 0, "");

	return 0l;
}

int App::doLogout(Client* client, u_int32 sequence)
{
	LOG(LEVEL_INFO, "id = %s, sequence = %d", client->Id().string().c_str(), sequence);

	client->Logout();
	NetHelper::Return(&_netpump, client->Id(), sequence, 0, "");

	return 0l;
}

int App::doOpen(const Uuid& client, u_int32 sequence, const Uuid& signal_id, const VS_SIGNAL* signal)
{
	LOG(LEVEL_INFO, "sequence = %d, id = %s, type = %d, ipaddr = %x, port = %d, datalen = %d", 
		sequence, signal_id.string().c_str(),
		signal->type,
		signal->ipaddr,
		(u_int32)signal->port,
		signal->datalen);

	//
	device::Plugin* p = getplugin(signal->type);
	if(!p)
	{
		NetHelper::Return(&_netpump, client, sequence, -1, "Done support this type");
		return -1;
	}

	//
	Signal* s = getsignal(signal_id);
	if(s)
	{
		NetHelper::Return(&_netpump, client, sequence, -1, "");
		return -1;
	}

	//
	void* h = p->Open(signal, App::SigCB, this, signal_id);
	if(!h)
	{
		NetHelper::Return(&_netpump, client, sequence, -1, "");
		return -1;
	}

	//
	s = new Signal(signal_id, signal, h, p, sequence);
	addsignal(s);

	//
	Uuid ts;
	Message m(TRANSFER_CMD_ADD_SIGNAL, sizeof(signal_id), &signal_id);
	_transfer.Command(ts, 0, &m);

	//
	return 0l;
}

int App::doClose(const Uuid& client, u_int32 sequence, const Uuid& signal_id)
{
	LOG(LEVEL_INFO, "sequence = %d, id = %s", sequence, signal_id.string().c_str());

	//
	Signal* s = getsignal(signal_id);
	if(!s)
	{
		NetHelper::Return(&_netpump, client, sequence, 0, "");
		return 0;
	}

	// 
	Uuid ts;
	Message m(TRANSFER_CMD_DEL_SIGNAL, sizeof(signal_id), &signal_id);
	_transfer.Command(ts, 0, &m);

	//
	void* h = s->handle();	
	device::Plugin* p = s->plugin();
	
	//
	p->Close(h);

	//
	delsignal(s);
	delete s;
	LOG(LEVEL_INFO, "delsignal() success! sequence = %d, id = %s", sequence, signal_id.string().c_str());
	//
	NetHelper::Return(&_netpump, client, sequence, 0, "");
	return 0l;
}

int App::doAddOutput(const Uuid& client, u_int32 sequence, const VS_ADD_OUTPUT* output)
{
	LOG(LEVEL_INFO, "sequence = %d, group = %d, id = %s, stream = %s, block = %s, output = [%x, %d, %d, %d]", 
		sequence,
		output->group_id,
		Uuid(output->signal_id).string().c_str(),
		Uuid(output->stream_id).string().c_str(),
		Uuid(output->block_id).string().c_str(),
		(u_int32)output->output.ipaddr,
		(u_int32)output->output.output,
		(u_int32)output->output.index,
		(u_int32)output->output.port
		);

	Signal* s = getsignal(output->signal_id);
	if(!s)
	{
		Uuid id(output->signal_id);
		NetHelper::Return(&_netpump, client, sequence, -1, "");
		LOG(LEVEL_ERROR, "getsignal failed! signal_id=%s.\n", id.string().c_str());
		return -1;
	}

	//
	void* h = s->handle();
	device::Plugin* p = s->plugin();
	p->SetIFrame(h);

	// 
	Uuid ts;
	Message m(TRANSFER_CMD_ADD_OUTPUT, sizeof(VS_ADD_OUTPUT), output);
	_transfer.Command(ts, 0, &m);
	
	//
	NetHelper::Return(&_netpump, client, sequence, 0, "");
	return 0l;
}

int App::doDelOutput(const Uuid& client, u_int32 sequence, const VS_DEL_OUTPUT* output)
{
	LOG(LEVEL_INFO, "sequence = %d, group = %d, id = %s, stream = %s, block = %s, output = [%x, %d, %d, %d]", 
		sequence,
		output->group_id,
		Uuid(output->signal_id).string().c_str(),
		Uuid(output->stream_id).string().c_str(),
		Uuid(output->block_id).string().c_str(),
		(u_int32)output->output.ipaddr,
		(u_int32)output->output.output,
		(u_int32)output->output.index,
		(u_int32)output->output.port
		);

	// 
	Uuid ts;
	Message m(TRANSFER_CMD_DEL_OUTPUT, sizeof(VS_DEL_OUTPUT), output);
	_transfer.Command(ts, 0, &m);

	//
	NetHelper::Return(&_netpump, client, sequence, 0, "");
	return 0l;

}

int App::doUnicast(const Uuid& client, u_int32 sequence, const VS_SWITCH_UNICAST* unicast)
{
	LOG(LEVEL_INFO, "sequence = %d, id = %s, stream = %s, block = %s", 
		sequence,
		Uuid(unicast->signal_id).string().c_str(),
		Uuid(unicast->stream_id).string().c_str(),
		Uuid(unicast->block_id).string().c_str()
		);

	// 
	Uuid ts;
	Message m(TRANSFER_CMD_UNICAST, sizeof(VS_SWITCH_UNICAST), unicast);
	_transfer.Command(ts, 0, &m);	

	//
	NetHelper::Return(&_netpump, client, sequence, 0, "");
	return 0l;
}

int App::doMulticast(const Uuid& client, u_int32 sequence, const VS_SWITCH_MULTICAST* multicast)
{
	LOG(LEVEL_INFO, "sequence = %d, id = %s, stream = %s, block = %s, ipaddr = %x, port = %d", 
		sequence,
		Uuid(multicast->signal_id).string().c_str(),
		Uuid(multicast->stream_id).string().c_str(),
		Uuid(multicast->block_id).string().c_str(),
		multicast->ipaddr,
		(u_int32)multicast->port
		);

	// 
	Uuid ts;
	Message m(TRANSFER_CMD_MULTICAST, sizeof(VS_SWITCH_MULTICAST), multicast);
	_transfer.Command(ts, 0, &m);	

	//
	NetHelper::Return(&_netpump, client, sequence, 0, "");
	return 0l;

}

int App::doResetBase(const Uuid& client, u_int32 sequence, const VS_BASE* base)
{
	LOG(LEVEL_INFO, "sequence = %d, id = %s, group = %d",
		sequence,
		Uuid(base->signal).string().c_str(),
		base->group_id
		);

	// 
	Uuid ts;
	Message m(TRANSFER_CMD_RESET_BASE, sizeof(VS_BASE), base);
	_transfer.Command(ts, 0, &m);	

	//
	NetHelper::Return(&_netpump, client, sequence, 0, "");
	return 0l;
}

int App::doOnline(const Uuid& client, u_int32 sequence, const VS_DISP_CHANGE* disp)
{
	LOG(LEVEL_INFO, "<< sequence = %d, ipaddr = %x, output = %d, port_cnt = %d", 
		sequence,
		(u_int32)disp->ipaddr,
		(u_int32)disp->output,
		(u_int32)disp->port_cnt);

	for(unsigned int i = 0; i < disp->port_cnt; i++)
	{
		LOG(LEVEL_INFO, "<< << port = %d", (u_int32)disp->port[i]);
	}

	// 
	Uuid ts;
	Message m(TRANSFER_CMD_ONLINE, sizeof(VS_DISP_CHANGE) + disp->port_cnt * sizeof(u_int16), disp);
	_transfer.Command(ts, 0, &m);	

	//
	NetHelper::Return(&_netpump, client, sequence, 0, "");
	return 0l;
}

int App::doOffline(const Uuid& client, u_int32 sequence, const VS_DISP_CHANGE* disp)
{
	LOG(LEVEL_INFO, "<< sequence = %d, ipaddr = %x, output = %d, port_cnt = %d", 
		sequence,
		(u_int32)disp->ipaddr,
		(u_int32)disp->output,
		(u_int32)disp->port_cnt);

	for(unsigned int i = 0; i < disp->port_cnt; i++)
	{
		LOG(LEVEL_INFO, "<< << port = %d", (u_int32)disp->port[i]);
	}

	// 
	Uuid ts;
	Message m(TRANSFER_CMD_OFFLINE, sizeof(VS_DISP_CHANGE) + disp->port_cnt * sizeof(u_int16), disp);
	_transfer.Command(ts, 0, &m);	

	//
	NetHelper::Return(&_netpump, client, sequence, 0, "");
	return 0l;
}

int App::doCloseAll(const Uuid& client, u_int32 sequence)
{
	LOG(LEVEL_INFO, "sequence = %d", sequence);

	for(std::list<Signal*>::iterator it = _signal_list.begin();
		it != _signal_list.end(); )
	{
		// 
		Uuid ts;
		Uuid signal_id = (*it)->id();
		Message m(TRANSFER_CMD_DEL_SIGNAL, sizeof(signal_id), &signal_id);
		_transfer.Command(ts, 0, &m);

		//
		(*it)->plugin()->Close((*it)->handle());

		//
		delete *it;

		//
		it = _signal_list.erase(it);
	}

	//
	NetHelper::Return(&_netpump, client, sequence, 0, "");
	return 0l;
}

int App::doError(const Uuid& item, int error)
{
	LOG(LEVEL_INFO, "id = %s, error = %d", item.string().c_str(), error);

	//
	Signal* s = getsignal(item);
	if(!s)
	{
		return 0l;
	}

	//
	if(error != 0)
	{
		s->error(error);
	}	

	return 0l;
}

int App::doDesc(const Uuid& item, const P_DESC* desc)
{
	LOG(LEVEL_INFO, ">> id = %s", item.string().c_str());

	//
	O_DESC de(desc);
	for(unsigned int i = 0; i < de.audio_cnt(); i++)
	{
		LOG(LEVEL_INFO, ">> >> audio id = %s, grade = %d",
			de.audio(i)->id().string().c_str(),
			de.audio(i)->grade());
	}

	for(unsigned int i = 0; i < de.video_cnt(); i++)
	{
		const O_VIDEO_DESC* video = de.video(i);
		LOG(LEVEL_INFO, ">> >> video id = %s, isaudio = %d, resol = [%d, %d], rows = %d, cols = %d, grade = %d",
			video->id().string().c_str(),
			video->isaudio() ? 1 : 0,
			video->resol().w,
			video->resol().h,
			video->row(),
			video->col(),
			video->grade());


		for(unsigned int r = 0; r < video->row(); r++)
		{
			for(unsigned int c = 0; c < video->col(); c++)
			{
				const O_BLOCK_DESC* block = video->block(r, c);
				const P_BLOCK_DESC* p = block->get();

				LOG(LEVEL_INFO, ">> >> >> block id = %s, area = [%d, %d, %d, %d], resol = [%d, %d], valid = [%d, %d, %d, %d]",
					Uuid(p->id).string().c_str(),
					p->area.x,
					p->area.y,
					p->area.w,
					p->area.h,
					p->resol.w,
					p->resol.h,
					p->valid.x,
					p->valid.y,
					p->valid.w,
					p->valid.h);
			}
		}
	}

	//
	Signal* s = getsignal(item);
	if(!s)
	{
		return 0l;
	}

	//错误提示信息的码流描述信息 signal.id == stream_id
	//所以：如果ID不相同，说明是实际信号源的码流，需要停止/清除提示信息发送的标志reset_once
	if(!(de.video_cnt()>0 && de.video(0)->id() == item))
	{
		s->reset_once();
	}

	//	
	for(std::list<Client*>::iterator it = _clients.begin();
		it != _clients.end();
		it++)
	{
		if(!s->isret())
		{
			s->ret();
			NetHelper::Return(&_netpump, (*it)->Id(), s->sequence(), de.len(), desc);	
		}

		NetHelper::Message(&_netpump, (*it)->Id(), 0, de.len(), desc);

	}

	// 
	Uuid ts;
	Message m(TRANSFER_CMD_UPDATE_DESC, de.len(), desc);	
	_transfer.Command(ts, 0, &m);

	return 0l;
}

int App::doAudio(const Uuid& item, const P_AUDIO* audio)
{
	// 
	Uuid ts;
	Message *m = new Message(TRANSFER_CMD_AUDIO_DATA, sizeof(P_AUDIO) + audio->datalen, audio);
	_transfer.Command_RefMessage(ts, 0, m);

	return 0l;
}

int App::doVideo(const Uuid& item, const P_VIDEO* video)
{
	// 
	if(g_dif < 0)
	{
		TimeDiffer td("doVideo");
	Uuid ts;
	Message *m = new Message(TRANSFER_CMD_VIDEO_DATA, sizeof(P_VIDEO) + video->datalen, video);
	_transfer.Command_RefMessage(ts, 0, m);
	}
	else
	{
		Uuid ts;
		Message *m = new Message(TRANSFER_CMD_VIDEO_DATA, sizeof(P_VIDEO) + video->datalen, video);
		_transfer.Command_RefMessage(ts, 0, m);
	}
	
#if 0//def OPT_DEBUG_OUT
	struct timeval tv;
	gettimeofday(&tv, NULL);

	ULONGLONG nowtime = ((ULONGLONG)tv.tv_sec * 1000 + tv.tv_usec/1000);

	long l_dif = nowtime - g_app_preTime;
	g_app_preTime = nowtime;

	if(l_dif > 100 || g_dif < 0)
	{
		OPT_MEM* omem = (OPT_MEM*)video->data;
		pthread_t self_id = pthread_self();
		LOG(LEVEL_INFO, "thread=%d:%u, g_cnt(%d,%d,%d,%d, %d, %d,%d,%d,%d,%d,%d,%d), SEND >> data=%d, size=%d. stamp=%llu, nowtime=%llu, interval=%ld.\n"
			, (int)self_id.p, self_id.x
			, g_cnt[0], g_cnt[1], g_cnt[2], g_cnt[3], g_cnt[4], g_cnt[5], g_cnt[6], g_cnt[7], g_cnt[8], g_cnt[9], g_cnt[10], g_cnt[11]
			, (int)omem->memData, omem->memLen, video->timestamp, nowtime, l_dif);
	}
#endif//OPT_DEBUG_OUT
	return 0l;
}

int App::msgJoin(const VS_JOIN* output)
{
	for(std::list<Client*>::iterator it = _clients.begin();
		it != _clients.end();
		it++)
	{
		NetHelper::Message(&_netpump, (*it)->Id(), 0, output);
		LOG(LEVEL_INFO, "Client(%s), info:%s, index:%u, ipaddr:%x, port:%hu.\n",
			(*it)->Id().string().c_str(), output->info, output->output.index, output->output.ipaddr, output->output.port);
	}

	return 0l;
}

int App::msgBase(const VS_SETBASE* output)
{
	for(std::list<Client*>::iterator it = _clients.begin();
		it != _clients.end();
		it++)
	{
		NetHelper::Message(&_netpump, (*it)->Id(), 0, output);
	}

	return 0l;
}

int App::msgMulticast(const VS_MULTICAST* output)
{
	for(std::list<Client*>::iterator it = _clients.begin();
		it != _clients.end();
		it++)
	{
		NetHelper::Message(&_netpump, (*it)->Id(), 0, output);
	}

	return 0l;
}

int App::msgUnicast(const VS_UNICAST* output)
{
	for(std::list<Client*>::iterator it = _clients.begin();
		it != _clients.end();
		it++)
	{
		NetHelper::Message(&_netpump, (*it)->Id(), 0, output);
	}

	return 0l;
}














