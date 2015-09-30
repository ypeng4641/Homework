#pragma once
#include <set>
#include <vector>
#include <map>
#include "pthread.h"

#include <x_log\msglog.h>
#include <x_util\uuid.h>
#include "Nvt.h"
#include "defvalue.h"

namespace _NVT
{
	using namespace x_util;

/*
**主流程监视
*/
enum OPT_CNT_POS
{
	opt_App_Run,
	opt_App_Dispatch,
	opt_App_Dispatch_COMMAND,
	opt_App_Dispatch_RESULT,
	opt_App_Dispatch_REQUEST,
	opt_App_Dispatch_REQUEST_ERROR,
	opt_App_Dispatch_REQUEST_DESC,
	opt_App_Dispatch_REQUEST_AUDIO,
	opt_App_Dispatch_REQUEST_VIDEO,
	opt_App_Dispatch_REQUEST_JOIN,
	opt_App_Dispatch_REQUEST_BASE,
	opt_App_Dispatch_REQUEST_MULTICAST,
	opt_App_Dispatch_REQUEST_UNICAST,
	opt_App_Dispatch_SERVICE,
	opt_App_Dispatch_CLIENT,
	opt_App_Dispatch_PACKET,
	opt_App_Dispatch_PACKET_LOGIN,
	opt_App_Dispatch_PACKET_LOGOUT,
	opt_App_Dispatch_PACKET_OPEN_SIGNAL,
	opt_App_Dispatch_PACKET_CLOSE_SIGNAL,
	opt_App_Dispatch_PACKET_ADD_OUTPUT,
	opt_App_Dispatch_PACKET_DEL_OUTPUT,
	opt_App_Dispatch_PACKET_UNICAST,
	opt_App_Dispatch_PACKET_MULTICAST,
	opt_App_Dispatch_PACKET_RESET_BASE,
	opt_App_Dispatch_PACKET_ONLINE,
	opt_App_Dispatch_PACKET_OFFLINE,
	opt_App_Dispatch_PACKET_CLOSEALL,
	opt_App_alive,
	opt_App_check,
	opt_App_background,

	opt_AUTOMEM_Output,

	opt_CNT_END,//opt_CNT_END < CNT_SIZE
};//OPT_CNT_POS

#define CNT_SIZE (50)//OPT_CNT_POS size

extern int g_cntB[CNT_SIZE];//OPT_CNT_POS count
extern int g_cntE[CNT_SIZE];//OPT_CNT_POS count

int gCntB(int seq);
int gCntE(int seq);
std::string showCnt(void);


/*
**打印输出设置
*/
enum OPT_DEBUG_CLASS
{
	opt_NoneDbg			= 0x0000,

	opt_App				= 0x0001,
	opt_send_buf		= 0x0002,
	opt_Plugin			= 0x0004,
	opt_Dispatch		= 0x0008,
	opt_TfVideoOutput	= 0x0010,

	opt_AllDbg			= 0xFFFF,
};//OPT_DEBUG_CLASS

extern int g_dbgClass;//OPT_DEBUG_CLASS

bool setDBG(OPT_DEBUG_CLASS opt);
bool unsetDBG(OPT_DEBUG_CLASS opt);
std::string showDBG(void);


/*
**码流输出监视
*/
//#define ITEMID(a,b) ((((u_int64)a)<<32) | ((u_int64)b))
int DEC_IP(unsigned long long a);
int DEC_PORT(unsigned long long a);

std::string showOutput(void);

extern std::set<unsigned long long> g_OutputList;


/*
**插件监视
*/
struct OutputInfo
{
	int dstIp;
	unsigned short dstPort;
	char output;
	int index;

public:
	bool operator ==(OutputInfo& oi)
	{
		if(dstIp == oi.dstIp
			&& dstPort == oi.dstPort
			&& output == oi.output
			&& index == oi.index
			)
		{
			return true;
		}

		return false;
	}
};

struct SignalInfo
{
	//Signal
	Uuid id;

	//Plugin
	std::string name;
	int typeCode;
	int srcIp;
	unsigned short srcPort;

	//Output
	std::vector<OutputInfo> output;

	//Multicast
	int multiIp;
	unsigned short multiPort;
};

extern std::map<std::string/*uuid*/, SignalInfo> g_PluginList;

bool gAddSig(Uuid sid, const char* name, int type, int ip, unsigned short port);
bool gDelSig(Uuid sid);
bool gAddOutput(Uuid sid, int ip, unsigned short port, char output, int index);
bool gDelOutput(Uuid sid, int ip, unsigned short port, char output, int index);
bool gMulticast(Uuid sid, int ip, unsigned short port);
bool gUnicast(Uuid sid);

SignalInfo* gGetSI(Uuid sid);
std::string showPlugin(void);



/*
**telnet服务端助手
*/
class NvtHelper
{
public:
#define CMD				("cmd")
#define GLOBAL			("global")
#define OUTPUT			("output")
#define PLUGIN			("plugin")
#define DEBUG			("debug")
#define ENABLE			("enable -")
#define DISABLE			("disable -")
#define CLASS			("class")
#define QUIT			("quit")

#define OPT_ALL				("AllDbg")
#define OPT_APP				("App")
#define OPT_SEND_BUF		("send_buf")
#define OPT_PLUGIN			("Plugin")
#define OPT_DISPATCH		("Dispatch")
#define OPT_VIDEO_OUTPUT	("TfVideoOutput")
#define OPT_OPTION			("\n\tAllDbg" "\n\tApp" "\n\tDispatch" "\n\tPlugin" "\n\tsend_buf" "\n\tTfVideoOutput")

public:
	NvtHelper(int port): m_port(port)
		, m_isRun(true), m_pRes(-1)
	{
	}

	~NvtHelper(void)
	{
	}

	bool run(void);
	bool quit(void);

private:
	static void* threadLoop(void* param);

	bool processCmd(Nvt* nvt);

	bool dbgSwitch(bool on, std::string opt);

private:
	int m_port;

	int m_pRes;
	pthread_t m_pid;
	bool m_isRun;
};//NvtHelper

};//_NVT
