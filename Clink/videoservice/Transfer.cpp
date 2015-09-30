#include "Transfer.h"

#include "TfSignal.h"

#include <x_log/msglog.h>

#include "Counter.h"

#include "optimizer.h"

Transfer::Transfer(void)
{
}


Transfer::~Transfer(void)
{
}

int Transfer::Init(Handler* h)
{
	return Component::Init(h);
}

int Transfer::Deinit()
{
	return 0l;
}

int Transfer::Start()
{
	_tfm = new TfMethod(Transfer::tfm_static_join, Transfer::tfm_static_base, Transfer::tfm_static_multicast, Transfer::tfm_static_unicast, this);

	if(BeginThread() != 0)
	{
		LOG(LEVEL_ERROR, "Begin Thread Failed");
		return -1;
	}

	return 0l;
}

int Transfer::Stop()
{
	if(EndThread() != 0)
	{
		LOG(LEVEL_ERROR, "End Thread Failed");
	}

	delete _tfm;

	return 0l;
}

int Transfer::Command(const Uuid& tranId, unsigned int step, const Message* m)
{
	MsgCommand msg = {
		tranId,
		step,
		(m != NULL) ? (m->clone()) : (NULL),
	};

	Message* c = new Message(TRANSFER_INNER_COMMAND, sizeof(msg), &msg);
	_msgQueue.push(c);
	
	return 0l;
}

int Transfer::Command_RefMessage(const Uuid& tranId, unsigned int step, Message* m)
{
	MsgCommand msg = {
		tranId,
		step,
		m
	};

	Message* c = new Message(TRANSFER_INNER_COMMAND, sizeof(msg), &msg);
	_msgQueue.push(c);
	
	return 0l;
}


int Transfer::Result(const Uuid& tranId, unsigned int step, const Uuid& compId, const Message* m)
{
	MsgResult msg = {
		tranId,
		step,
		compId,
		(m != NULL) ? (m->clone()) : (NULL),
	};

	Message* c = new Message(TRANSFER_INNER_RESULT, sizeof(msg), &msg);
	_msgQueue.push(c);
	
	return 0l;
}

int Transfer::Request(const Uuid& tranId, unsigned int step, const Uuid& compId, const Message* m)
{
	MsgRequest msg = {
		tranId,
		step,
		compId,
		(m != NULL) ? (m->clone()) : (NULL),
	};

	Message* c = new Message(TRANSFER_INNER_REQUEST, sizeof(msg), &msg);
	_msgQueue.push(c);
	
	return 0l;
}


int Transfer::BeginThread()
{
	_isrun = true;
	if(pthread_create(&_thread, NULL, Transfer::ThreadFunc, this) != 0)
	{
		return -1;
	}

	return 0l;
}

int Transfer::EndThread()
{
	_isrun = false;
	pthread_join(_thread, NULL); 
	return 0l;
}

void* Transfer::ThreadFunc(void* data)
{
	Transfer* pThis = reinterpret_cast<Transfer*>(data);
	pThis->Run();
	return NULL;
}

void Transfer::Run()
{
	u_int64 t1 = Counter::instance()->tick();

	u_int64 basetime = Counter::instance()->tick();

	while(_isrun)
	{
		Message* m = _msgQueue.pop();
		if(m)
		{
			Dispatch(m);
		}

#ifndef OPT_NO_TIMEOUT
		u_int64 nowtime = Counter::instance()->tick();
		u_int64 diff = nowtime - basetime;

		u_int64 t2 = Counter::instance()->tick();

		if((t2-t1) > 50)
		{
			LOG(LEVEL_INFO, "@@@@@@@@@@@@@@@@@@@ Warning timeout is %dms", t2 - t1);
		}

		t1 = t2;
		if(diff >= 10)
		{
			basetime = nowtime;
			Timeout();
		}
#endif//OPT_NO_TIMEOUT
	}
}

void Transfer::Dispatch(Message* m)
{
	switch(m->type())
	{
	case TRANSFER_INNER_COMMAND:
		{
			MsgCommand msg;
			memcpy(&msg, m->data(), sizeof(msg));
			
			ExecCommand(msg.tranId, msg.step, msg.message);

			if(msg.message)
			{
				delete msg.message;
			}
		}
		break;
	case TRANSFER_INNER_RESULT:
		{
			// 提取事务信息
			MsgResult msg;
			memcpy(&msg, m->data(), sizeof(msg));

			// 执行事务步骤
			ExecResult(msg.tranId, msg.step, msg.compId, msg.message);

			// 释放内部消息
			if(msg.message)
			{
				delete msg.message;
			}
		}
		break;
	case TRANSFER_INNER_REQUEST:
		{
			MsgRequest msg;
			memcpy(&msg, m->data(), sizeof(msg));

			ExecRequest(msg.tranId, msg.step, msg.compId, msg.message);

			if(msg.message)
			{
				delete msg.message;
			}
		}
		break;
	default:
		{
			LOG(LEVEL_WARNING, "Don't Support This Command, Type = %d", m->type());
		}
		break;
	}

	// 释放消息
	delete m;

}


void Transfer::Timeout()
{
	for(std::map<Uuid, TfSignal*>::iterator it = _signal_map.begin();
		it != _signal_map.end();
		it++)
	{
		it->second->timeout();
	}
}


void Transfer::ExecCommand(const Uuid& tranId, unsigned int step, const Message* m)
{
	switch(m->type())
	{
	case TRANSFER_CMD_ADD_SIGNAL:
		{
			LOG(LEVEL_INFO, "TRANSFER: type=TRANSFER_CMD_ADD_SIGNAL\r\n");

			Uuid id(*reinterpret_cast<const Uuid*>(m->data()));
			doAddSignal(tranId, step, id);
		}
		break;
	case TRANSFER_CMD_DEL_SIGNAL:
		{
			LOG(LEVEL_INFO, "TRANSFER: type=TRANSFER_CMD_DEL_SIGNAL\r\n");

			Uuid id(*reinterpret_cast<const Uuid*>(m->data()));
			doDelSignal(tranId, step, id);
		}
		break;
	case TRANSFER_CMD_ADD_OUTPUT:
		{
			LOG(LEVEL_INFO, "TRANSFER: type=TRANSFER_CMD_ADD_OUTPUT\r\n");

			const VS_ADD_OUTPUT* p = reinterpret_cast<const VS_ADD_OUTPUT*>(m->data());
			doAddOutput(tranId, step, p);
		}
		break;
	case TRANSFER_CMD_DEL_OUTPUT:
		{
			LOG(LEVEL_INFO, "TRANSFER: type=TRANSFER_CMD_DEL_OUTPUT\r\n");

			const VS_DEL_OUTPUT* p = reinterpret_cast<const VS_DEL_OUTPUT*>(m->data());
			doDelOutput(tranId, step, p);
		}
		break;
	case TRANSFER_CMD_MULTICAST:
		{
			LOG(LEVEL_INFO, "TRANSFER: type=TRANSFER_CMD_MULTICAST\r\n");

			const VS_SWITCH_MULTICAST* p = reinterpret_cast<const VS_SWITCH_MULTICAST*>(m->data());
			doSwitchMulticast(tranId, step, p);
		}
		break;
	case TRANSFER_CMD_UNICAST:
		{
			LOG(LEVEL_INFO, "TRANSFER: type=TRANSFER_CMD_UNICAST\r\n");

			const VS_SWITCH_UNICAST* p = reinterpret_cast<const VS_SWITCH_UNICAST*>(m->data());
			doSwitchUnicast(tranId, step, p);
		}
		break;
	case TRANSFER_CMD_RESET_BASE:
		{
			LOG(LEVEL_INFO, "TRANSFER: type=TRANSFER_CMD_RESET_BASE\r\n");

			const VS_BASE* p = reinterpret_cast<const VS_BASE*>(m->data());
			doResetBase(tranId, step, p);
		}
		break;
	case TRANSFER_CMD_ONLINE:
		{
			LOG(LEVEL_INFO, "TRANSFER: type=TRANSFER_CMD_ONLINE\r\n");

			const VS_DISP_CHANGE* p = reinterpret_cast<const VS_DISP_CHANGE*>(m->data());
			doOnline(tranId, step, p);
		}
		break;
	case TRANSFER_CMD_OFFLINE:
		{
			LOG(LEVEL_INFO, "TRANSFER: type=TRANSFER_CMD_OFFLINE\r\n");

			const VS_DISP_CHANGE* p = reinterpret_cast<const VS_DISP_CHANGE*>(m->data());
			doOffline(tranId, step, p);
		}
		break;
	case TRANSFER_CMD_UPDATE_DESC:
		{
			LOG(LEVEL_INFO, "TRANSFER: type=TRANSFER_CMD_UPDATE_DESC\r\n");

			const VS_SIGNAL_DESC* p = reinterpret_cast<const VS_SIGNAL_DESC*>(m->data());
			doDesc(tranId, step, p);
		}
		break;
	case TRANSFER_CMD_AUDIO_DATA:
		{
			const P_AUDIO* p = reinterpret_cast<const P_AUDIO*>(m->data());
			doAudio(tranId, step, p);
		}
		break;
	case TRANSFER_CMD_VIDEO_DATA:
		{
			const P_VIDEO* p = reinterpret_cast<const P_VIDEO*>(m->data());
			doVideo(tranId, step, p);
		}
		break;
	}

}

void Transfer::ExecResult(const Uuid& tranId, unsigned int step, const Uuid& compId, const Message* m)
{
	(void)tranId;
	(void)step;
	(void)compId;
	(void)m;
}

void Transfer::ExecRequest(const Uuid& tranId, unsigned int step, const Uuid& compId, const Message* m)
{
	(void)tranId;
	(void)step;
	(void)compId;
	(void)m;
}

void Transfer::ret_status(const Uuid& tranId, unsigned int step, int status)
{
	//
	P_STATUS data;
	data.status = status;

	//
	Message m(TRANSFER_RET_STATUS, sizeof(data), &data);

	//
	Manager()->Result(tranId, step, Id(), &m);

}

TfSignal* Transfer::get(const Uuid& id)
{
	std::map<Uuid, TfSignal*>::iterator it = _signal_map.find(id);
	if(it != _signal_map.end())
	{
		return it->second;
	}

	return NULL;
}

void Transfer::doAddSignal(const Uuid& tranId, unsigned int step, const Uuid& id)
{
	std::map<Uuid, TfSignal*>::iterator it = _signal_map.find(id);
	if(it != _signal_map.end())
	{
		ret_status(tranId, step, -1);
		return;
	}

	TfSignal* p = new TfSignal(id);
	_signal_map.insert(std::make_pair(id, p));

	ret_status(tranId, step, 0);
	return;

}

void Transfer::doDelSignal(const Uuid& tranId, unsigned int step, const Uuid& id)
{
	std::map<Uuid, TfSignal*>::iterator it = _signal_map.find(id);
	if(it == _signal_map.end())
	{
		ret_status(tranId, step, -1);
		return;
	}

	delete it->second;
	_signal_map.erase(it);

	ret_status(tranId, step, 0);
	return;

}

void Transfer::doAddOutput(const Uuid& tranId, unsigned int step, const VS_ADD_OUTPUT* p)
{
	TfSignal* s = get(p->signal_id);
	if(!s)
	{
		ret_status(tranId, step, -1);
		return;
	}

	Uuid null;
	if(p->block_id == null)		// 音频
	{
		s->addoutput(p->stream_id, p->group_id, &p->output, p->info, _tfm);
	}
	else						// 视频
	{
		s->addoutput(p->stream_id, p->block_id, p->group_id, &p->output, p->info, _tfm);
	}

	ret_status(tranId, step, 0);
	return;
}

void Transfer::doDelOutput(const Uuid& tranId, unsigned int step, const VS_DEL_OUTPUT* p)
{
	TfSignal* s = get(p->signal_id);
	if(!s)
	{
		ret_status(tranId, step, -1);
		return;
	}

	Uuid null;
	if(p->block_id == null)		// 音频
	{
		s->deloutput(p->stream_id, p->group_id, &p->output);
	}
	else						// 视频
	{
		s->deloutput(p->stream_id, p->block_id, p->group_id, &p->output);
	}
	

	ret_status(tranId, step, 0);
	return;
}


void Transfer::doSwitchMulticast(const Uuid& tranId, unsigned int step, const VS_SWITCH_MULTICAST* p)
{
	TfSignal* s = get(p->signal_id);
	if(!s)
	{
		ret_status(tranId, step, -1);
		return;
	}

	Uuid null;
	if(p->block_id == null)		// 音频
	{
		s->multicast(p->stream_id, p->ipaddr, p->port);
	}
	else						// 视频
	{
		s->multicast(p->stream_id, p->block_id, p->ipaddr, p->port);
	}	

	ret_status(tranId, step, 0);
	return;
}

void Transfer::doSwitchUnicast(const Uuid& tranId, unsigned int step, const VS_SWITCH_UNICAST* p)
{
	TfSignal* s = get(p->signal_id);
	if(!s)
	{
		ret_status(tranId, step, -1);
		return;
	}

	Uuid null;
	if(p->block_id == null)		// 音频
	{
		s->unicast(p->stream_id);
	}
	else						// 视频
	{
		s->unicast(p->stream_id, p->block_id);
	}		

	ret_status(tranId, step, 0);
	return;
}


void Transfer::doResetBase(const Uuid& tranId, unsigned int step, const VS_BASE* p)
{
	TfSignal* s = get(p->signal);
	if(!s)
	{
		ret_status(tranId, step, -1);
		return;
	}

	s->setbase(p->group_id);

	ret_status(tranId, step, 0);
	return;	
}

void Transfer::doOnline(const Uuid& tranId, unsigned int step, const VS_DISP_CHANGE* p)
{
	for(std::map<Uuid, TfSignal*>::iterator it = _signal_map.begin();
		it != _signal_map.end();
		it++)
	{
		it->second->online(p);
	}

	ret_status(tranId, step, 0);
	return;	

}

void Transfer::doOffline(const Uuid& tranId, unsigned int step, const VS_DISP_CHANGE* p)
{
	for(std::map<Uuid, TfSignal*>::iterator it = _signal_map.begin();
		it != _signal_map.end();
		it++)
	{
		it->second->offline(p);
	}

	ret_status(tranId, step, 0);
	return;	
}

void Transfer::doDesc(const Uuid& tranId, unsigned int step, const VS_SIGNAL_DESC* p)
{
	TfSignal* s = get(p->id);
	if(!s)
	{
		ret_status(tranId, step, -1);
		return;
	}

	s->desc(p);

	ret_status(tranId, step, 0);
	return;	

}

void Transfer::doAudio(const Uuid& tranId, unsigned int step, const P_AUDIO* p)
{
	TfSignal* s = get(p->item);
	if(!s)
	{
		LOG(LEVEL_ERROR, "################Can't find TfSignal.\n");

		ret_status(tranId, step, -1);
		return;
	}

	s->audio(p);

	ret_status(tranId, step, 0);
	return;	
}

void Transfer::doVideo(const Uuid& tranId, unsigned int step, const P_VIDEO* p)
{
	TfSignal* s = get(p->item);
	if(!s)
	{
		buf_rcl(p);
		LOG(LEVEL_ERROR, "################Can't find TfSignal.\n");

		ret_status(tranId, step, -1);
		return;
	}

	s->video(p);

	ret_status(tranId, step, 0);
	return;	
}

void Transfer::tfm_static_join(const VS_JOIN* output, void* context)
{
	Transfer* pThis = reinterpret_cast<Transfer*>(context);
	pThis->tfm_join(output);
}

void Transfer::tfm_static_base(const VS_SETBASE* output, void* context)
{
	Transfer* pThis = reinterpret_cast<Transfer*>(context);
	pThis->tfm_base(output);
}

void Transfer::tfm_static_multicast(const VS_MULTICAST* output, void* context)
{
	Transfer* pThis = reinterpret_cast<Transfer*>(context);
	pThis->tfm_multicast(output);
}

void Transfer::tfm_static_unicast(const VS_UNICAST* output, void* context)
{
	Transfer* pThis = reinterpret_cast<Transfer*>(context);
	pThis->tfm_unicast(output);
}

void Transfer::tfm_join(const VS_JOIN* output)
{
	Uuid ts;
	Message m(TRANSFER_REQ_JOIN, sizeof(VS_JOIN), output);

	Manager()->Request(ts, 0, Id(), &m);
}

void Transfer::tfm_base(const VS_SETBASE* output)
{
	Uuid ts;
	Message m(TRANSFER_REQ_BASE, sizeof(VS_SETBASE), output);

	Manager()->Request(ts, 0, Id(), &m);
}

void Transfer::tfm_multicast(const VS_MULTICAST* output)
{
	Uuid ts;
	Message m(TRANSFER_REQ_MULTICAST, sizeof(VS_MULTICAST), output);

	Manager()->Request(ts, 0, Id(), &m);
}

void Transfer::tfm_unicast(const VS_UNICAST* output)
{
	Uuid ts;
	Message m(TRANSFER_REQ_UNICAST, sizeof(VS_UNICAST), output);

	Manager()->Request(ts, 0, Id(), &m);
}


