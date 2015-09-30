#pragma once


#include <list>
#include <vector>
#include <map>

#include <x_pattern/controler.h>
#include <x_pattern/netpump.h>
#include <x_util/uuid.h>

#include "TfMethod.h"

#include "msg.h"

using namespace std;
using namespace x_pattern;
using namespace x_util;


class TfSignal;
class Transfer : public Component
{
private:
	struct MsgCommand {
		Uuid			tranId;
		unsigned int	step;
		Message*		message;
	};
	struct MsgResult {
		Uuid			tranId;
		unsigned int	step;
		Uuid			compId;
		Message*		message;
	};

	struct MsgRequest {
		Uuid			tranId;
		unsigned int	step;
		Uuid			compId;
		Message*		message;
	};

public:
	Transfer(void);
	~Transfer(void);

public:
	virtual int Init(Handler* h);
	virtual int Deinit();
	virtual int Start();
	virtual int Stop();

public:
	virtual int Command(const Uuid& tranId, unsigned int step, const Message* m);
	virtual int Command_RefMessage(const Uuid& tranId, unsigned int step, Message* m);
	virtual int Result(const Uuid& tranId, unsigned int step, const Uuid& compId, const Message* m);
	virtual int Request(const Uuid& tranId, unsigned int step, const Uuid& compId, const Message* m);

private:
	int BeginThread();
	int EndThread();

private:
	static void* ThreadFunc(void* data);

private:
	void Run();

private:
	void Dispatch(Message* m);
	void Timeout();

private:
	void ExecCommand(const Uuid& tranId, unsigned int step, const Message* m);
	void ExecResult(const Uuid& tranId, unsigned int step, const Uuid& compId, const Message* m);
	void ExecRequest(const Uuid& tranId, unsigned int step, const Uuid& compId, const Message* m);

private:
	void ret_status(const Uuid& tranId, unsigned int step, int status);

private:
	TfSignal* get(const Uuid& id);

private:
	void doAddSignal(const Uuid& tranId, unsigned int step, const Uuid& id);
	void doDelSignal(const Uuid& tranId, unsigned int step, const Uuid& id);
	void doAddOutput(const Uuid& tranId, unsigned int step, const VS_ADD_OUTPUT* p);
	void doDelOutput(const Uuid& tranId, unsigned int step, const VS_DEL_OUTPUT* p);
	void doSwitchMulticast(const Uuid& tranId, unsigned int step, const VS_SWITCH_MULTICAST* p);
	void doSwitchUnicast(const Uuid& tranId, unsigned int step, const VS_SWITCH_UNICAST* p);
	void doResetBase(const Uuid& tranId, unsigned int step, const VS_BASE* p);
	void doOnline(const Uuid& tranId, unsigned int step, const VS_DISP_CHANGE* p);
	void doOffline(const Uuid& tranId, unsigned int step, const VS_DISP_CHANGE* p);
	void doDesc(const Uuid& tranId, unsigned int step, const VS_SIGNAL_DESC* p);
	void doAudio(const Uuid& tranId, unsigned int step, const P_AUDIO* p);
	void doVideo(const Uuid& tranId, unsigned int step, const P_VIDEO* p);

private:
	static void tfm_static_join(const VS_JOIN* output, void* context);
	static void tfm_static_base(const VS_SETBASE* output, void* context);
	static void tfm_static_multicast(const VS_MULTICAST* output, void* context);
	static void tfm_static_unicast(const VS_UNICAST* output, void* context);

private:
	void tfm_join(const VS_JOIN* output);
	void tfm_base(const VS_SETBASE* output);
	void tfm_multicast(const VS_MULTICAST* output);
	void tfm_unicast(const VS_UNICAST* output);

	void buf_rcl(const P_VIDEO* p)
#ifdef OPT_MEM_TO_PTR
	{
		OPT_MEM* om = (OPT_MEM*)p->data;
		delete[] om->memData;
		om->memData = NULL;
		om->memLen = 0;
	}
#else
	{
		return;
	}
#endif//OPT_MEM_TO_PTR

private:
	pthread_t	_thread;
	volatile bool _isrun;

private:
	MessageQueue _msgQueue;

private:
	std::map<Uuid, TfSignal*> _signal_map;

private:
	TfMethod *_tfm;

};

