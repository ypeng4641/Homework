// videoservice.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"

#include <signal.h>
#include <x_log/msglog.h>

#include "config.h"
#include "App.h"

#include <stdlib.h>

//#ifdef _DEBUG
//#define CRTDBG_MAP_ALLOC
//#include <crtdbg.h>
//#endif

#include <Windows.h>
#include <Dbghelp.h>

int g_var[50];

#include "Counter.h"

static bool isrun = true;

void sigHandle(int signo)
{
	isrun = false;
}


LONG WINAPI exception_filter(struct _EXCEPTION_POINTERS* ExceptionInfo)
{
	char buf[64];

	struct timeval tv;
	gettimeofday(&tv, NULL);

	sprintf_s(buf, "%u", tv.tv_sec % 5);

	std::string workpath = getWork() + "crash_dump_" + buf;
    HANDLE lhDumpFile = CreateFile(workpath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL ,NULL);
 
    MINIDUMP_EXCEPTION_INFORMATION loExceptionInfo;
    loExceptionInfo.ExceptionPointers = ExceptionInfo;
    loExceptionInfo.ThreadId = GetCurrentThreadId();
    loExceptionInfo.ClientPointers = TRUE;
    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),lhDumpFile, MiniDumpWithFullMemory, &loExceptionInfo, NULL, NULL);
 
    CloseHandle(lhDumpFile);
 
    return EXCEPTION_EXECUTE_HANDLER;
}

bool g_running = true;

UINT __stdcall PrintThread(void* arg)
{
#if 0
	while (g_running)
	{
		printf("=================================\n");
		for (int i = 0; i < 10; i++)
		{
			printf("var[%d] = %d\n", i, g_var[i]);
		}

		Sleep(2000);
	}
#endif

	return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
	SetErrorMode(SEM_NOGPFAULTERRORBOX);

	HANDLE hThread = (HANDLE)::_beginthreadex(0, 0, PrintThread, 0, 0, 0);

	WSADATA data;
	if(WSAStartup(MAKEWORD(2, 2), &data) != 0)
	{
		return -1;
	}

	log_init(NULL);

	if(Counter::instance()->init() != 0)
	{
		LOG(LEVEL_ERROR, "Init Counter failed");
		return -1;
	}

	if(initConfig(argc, argv) != 0)
	{
		LOG(LEVEL_ERROR, "Init Config failed");
		return -1;
	}

	LOG(LEVEL_INFO, "Init Config Success");

	// 
	SetUnhandledExceptionFilter(exception_filter);

	App app;
	if(app.Init(NULL) != 0)
	{
		LOG(LEVEL_ERROR, "Init Application failed");
		return -1;
	}

	LOG(LEVEL_INFO, "Init Application Success");

	if(app.Start() != 0)
	{
		LOG(LEVEL_ERROR, "Start Application failed");
		return -1;
	}

	LOG(LEVEL_INFO, "Start Application Success");
		
	signal(SIGBREAK, sigHandle);
	signal(SIGTERM, sigHandle);
	signal(SIGINT,  sigHandle);

	while(isrun)
	{
		Sleep(1000);
	}

	app.Stop();
	LOG(LEVEL_INFO, "Stop Application Success");

	app.Deinit();
	LOG(LEVEL_INFO, "Deinit Application Success");

	Counter::instance()->deinit();

	log_deinit();

#ifdef _DEBUG
	_CrtDumpMemoryLeaks();
#endif 

	WSACleanup();

	g_running = false;

	::WaitForSingleObject(hThread, INFINITE);

	return 0;
}
