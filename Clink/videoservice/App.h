#pragma once

#include <list>
#include <vector>

#include <x_pattern/controler.h>
#include <x_pattern/netpump.h>
#include <x_util/uuid.h>

#include "Plugin.h"
#include "Transfer.h"

#include "msg.h"
#include "background.h"
#include "watchdog.h"

using namespace std;
using namespace x_pattern;
using namespace x_util;


class Signal;
class Client;
class App: public Controler
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

	struct MsgService {
		int				err;
		SOCKET			s;
		Uuid			item;
	};

	struct MsgClient {
		int				err;
		SOCKET			s;
		Uuid			item;
	};

	struct MsgPacket {
		int				err;
		Uuid			item;
		Packet*			packet;
	};

public:
	App();
	~App();

public:
	virtual int Init(Handler* h);
	virtual int Deinit();
	virtual int Start();
	virtual int Stop();

public:
	virtual int Command(const Uuid& tranId, unsigned int step, const Message* m);
	virtual int Result(const Uuid& tranId, unsigned int step, const Uuid& compId, const Message* m);
	virtual int Request(const Uuid& tranId, unsigned int step, const Uuid& compId, const Message* m);

public:
	static int ServiceCB(int err, SOCKET s, void* ptr, const Uuid& item);
	int ServiceCB(int err, SOCKET s, const Uuid& item);
	static int ClientCB (int err, SOCKET s, void* ptr, const Uuid& item);
	int ClientCB(int err, SOCKET s, const Uuid& item);
	static int PacketCB(int err, const Uuid& id, const Packet* p, void* ptr);
	int PacketCB(int err, const Uuid& id, const Packet* p);

private:
	static int SigCB(const device::Result* m, void* context, const Uuid& item);
	int SigCB(const device::Result* m, const Uuid& item);

private:
	int BeginThread();
	int EndThread();

private:
	static void* ThreadFunc(void* data);

private:
	void Run();

private:
	void Dispatch(Message* m);
	void alive();
	void check();
	void background();

private:
	void ExecCommand(const Uuid& tranId, unsigned int step, const Message* m);
	void ExecResult(const Uuid& tranId, unsigned int step, const Uuid& compId, const Message* m);
	void ExecRequest(const Uuid& tranId, unsigned int step, const Uuid& compId, const Message* m);
	void ExecService(int err, SOCKET s, const Uuid& item);
	void ExecClient(int err, SOCKET s, const Uuid& item);
	void ExecPacket(int err, const Uuid& item, const Packet* p);

private:
	void install();
	void uninstall();

private:
	int addclient(Client* p);
	int delclient(Client* p);
	Client* getclient(const Uuid& id);

	int addplugin(device::Plugin* p);
	int delplugin(device::Plugin* p);
	device::Plugin* getplugin(u_int32 type);

	int addsignal(Signal* p);
	int delsignal(Signal* p);
	Signal* getsignal(const Uuid& id);

private:
	int doLogin(Client* client, u_int32 sequence, const char* name, const char* password, u_int32 mask);
	int doLogout(Client* client, u_int32 sequence);
	int doOpen(const Uuid& client, u_int32 sequence, const Uuid& signal_id, const VS_SIGNAL* signal);
	int doClose(const Uuid& client, u_int32 sequence, const Uuid& signal_id);
	int doAddOutput(const Uuid& client, u_int32 sequence, const VS_ADD_OUTPUT* output);
	int doDelOutput(const Uuid& client, u_int32 sequence, const VS_DEL_OUTPUT* output);
	int doUnicast(const Uuid& client, u_int32 sequence, const VS_SWITCH_UNICAST* unicast);
	int doMulticast(const Uuid& client, u_int32 sequence, const VS_SWITCH_MULTICAST* multicast);
	int doResetBase(const Uuid& client, u_int32 sequence, const VS_BASE* base);
	int doOnline(const Uuid& client, u_int32 sequence, const VS_DISP_CHANGE* disp);
	int doOffline(const Uuid& client, u_int32 sequence, const VS_DISP_CHANGE* disp);
	int doCloseAll(const Uuid& client, u_int32 sequence);

public:
	int doError	(const Uuid& item, int error);
	int doDesc	(const Uuid& item, const P_DESC* desc);
	int doAudio	(const Uuid& item, const P_AUDIO* audio);
	int doVideo	(const Uuid& item, const P_VIDEO* video);

private:
	int msgJoin(const VS_JOIN* output);
	int msgBase(const VS_SETBASE* output);
	int msgMulticast(const VS_MULTICAST* output);
	int msgUnicast(const VS_UNICAST* output);



private:
	Netpump	_netpump;

private:
	Uuid _serviceId;
	SOCKET _serviceSocket;

private:
	std::list<Client*> _clients;

private:
	pthread_t	_thread;
	volatile bool _isrun;

private:
	MessageQueue _msgQueue;

private:
	std::list<device::Plugin*> _plugin_list;
	std::list<Signal*> _signal_list;

private:
	Transfer _transfer;

	Background _background;

	//看门狗，防止僵尸进程
	Watchdog m_doggy;
};

