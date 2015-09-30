#ifndef _OPTIMIZER_H_
#define _OPTIMIZER_H_

#include <memory>
#include <string>

#include "NvtHelper.h"
using namespace _NVT;

/*
**将内存传递替换成内存指针传递
**涉及Plugin，Dispatch，App
*/
#define OPT_MEM_TO_PTR

struct OPT_MEM
{
	char* memData;
	int memLen;
};

/*
**去除帧内片与片之间的间隔
**涉及Dispatch
*/
//#define OPT_NO_SLICE_INTERVAL

/*
**去除Timeout（）机制
**涉及Transfer, TfVideoSource, TfVideoOutput, TfAudioSource
*/
//#define OPT_NO_TIMEOUT

/*
**添加插件缓存，使帧间隔平缓
**涉及Plugin, imp_*
*/
#define OPT_PLUGIN_BUF

/*
**打印输出
**涉及Dispatch, __send_buf, App, Transfer, Plugin, TfVideoOutput
*/
//#define OPT_DEBUG_OUT

extern long g_dif;
extern unsigned long long g_app_preTime;
extern unsigned long long g_app_preTime2;
extern int g_plugin_nowtime[30];
extern int g_plugin_cnt;

/*
**测试代码段执行时间
**未使用
*/
class TimeDiffer
{
public:
	TimeDiffer(const char* msg)
	{
#ifdef OPT_DEBUG_OUT
		strcpy(m_msg, msg);
		b_time = clock();
		//sprintf(m_info, "%s: TimeDiffer BEIGN now! clock=%ld\n", m_msg, b_time);
		//OutputDebugStringA(m_info);
#endif//OPT_DEBUG_OUT
	}
	~TimeDiffer()
	{
#ifdef OPT_DEBUG_OUT
		long e_time = clock();
		sprintf(m_info, "%s: TimeDiffer END now! clock=%ld, dif=%ld\n", m_msg, e_time, e_time - b_time);
		OutputDebugStringA(m_info);
#endif//OPT_DEBUG_OUT
	}

private:
	char m_msg[256];
	char m_info[256];
	long b_time;
};

#endif//_OPTIMIZER_H_
