#pragma once

#include <vector>
#include <map>
#include <set>
#include <x_types/intxx.h>
#include <x_pattern/netpump.h>
#include "msg.h"

#include "optimizer.h"

using namespace std;
using namespace x_pattern;

#define ITEMID(a,b) ((((u_int64)a)<<32) | ((u_int64)b))
class Outport;

class Dispatch
{
#define CAPACITY (3)
private:
	Dispatch(void);
	~Dispatch(void);

public:
	int Init();
	int Deinit();

public:
	int add(u_int32 ipaddr, u_int16 port);
	int del(u_int32 ipaddr, u_int16 port);
	int push(u_int32 ipaddr, u_int16 port, AUTOMEM* mem);

public:
	static Dispatch* instance()
	{
		if(!_instance)
		{
			_instance = new Dispatch();
		}

		return _instance;
	}

private:
	static Dispatch* _instance;

	typedef std::set<u_int64> ITEMS;
	typedef std::map<Outport*, ITEMS> OUTPORTS;
	OUTPORTS _outportCan;
	pthread_rwlock_t _opLock;

};//Dispatch

class Outport
{
private:
	enum 
	{
		SOCKET_POOL_SIZE = 16,
		FIRST_PORT = 34200,

		TICK_COUNT = 200,					//调整缓存临界
		TICK_TIME = 1000000 / TICK_COUNT,	//发送数据间隔

		UDP_SLICE_LEN = 32 * 1024,			//udp分片大小
		VDATA_SLICE_LEN = UDP_SLICE_LEN - sizeof(VS_VIDEO_PACKET), //每片包含视频数据大小
	};

	enum 
	{
		cmd_add,
		cmd_del,
		cmd_push
	};

	struct data_common
	{
		u_int32	cmd;
	};

	struct data_add
	{
		u_int32	cmd;
		u_int32 ipaddr;
		u_int16	port;
	};

	struct data_del
	{
		u_int32	cmd;
		u_int32 ipaddr;
		u_int16	port;
	};

	struct data_push
	{
		u_int32	cmd;
		u_int32 ipaddr;
		u_int16	port;
		u_int32	datalen;
		char*	data;
	};

	struct packet
	{
		u_int32 datalen;
		char*	data;
	};
	
	struct item
	{
		u_int32 ipaddr;
		u_int16 port;
		std::list<packet*>	packet_list;
	};

public:
	Outport(int i);
	~Outport(void);
	
public:
	int Init();
	int Deinit();

public:
	int add(u_int32 ipaddr, u_int16 port);
	int del(u_int32 ipaddr, u_int16 port);
	int push(u_int32 ipaddr, u_int16 port, AUTOMEM* mem);

	int SplitFrame_Push(VS_VIDEO_PACKET* vsp, std::list<packet*>* plp);

private:
	int command(data_common* d);
	data_common* command();
	int exitloop();

private:
	void process(data_common* d);
	void send(int loop);

private:
	int BeginThread();
	int EndThread();

private:
	static void* ThreadFunc(void* data);

private:
	void Run();

private:
	evutil_socket_t _pipe[2];
	evutil_socket_t _exit[2];

private:
	pthread_t	_thread;

private:
	pthread_mutex_t _mutex;

private:
	std::list<data_common*> _command_list;

private:
	std::vector<SOCKET>	_socket_pool;
	std::map<u_int64, item*> _item_list;
	
	int _port_segment;
	//测试使用
private:
	int m_preFrameNo;

};//Outport