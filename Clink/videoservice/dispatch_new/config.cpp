#include "config.h"
#include "defvalue.h"

#include <Windows.h>

#include <x_log/msglog.h>

static std::string WORK_DIR;

unsigned short SERVICE_PORT = SELF_PORT;


int initConfig(int argc, char** argv)
{
	(void)argc;
	(void)argv;

	char binPath[MAX_PATH];

	if(!GetModuleFileNameA(NULL, binPath, sizeof(binPath)))
	{
		LOG(LEVEL_ERROR, "Get Work Dir Failed(%d", GetLastError());
		return -1;
	}

	char sDrive[_MAX_DRIVE];
	char sDir[_MAX_DIR];
	char sFname[_MAX_FNAME];
	char sExt[_MAX_EXT];

	_splitpath_s(binPath, sDrive, sDir, sFname, sExt);

	WORK_DIR = sDrive;
	WORK_DIR += sDir;

	if(argc >= 3)
	{
		if(strcmp(argv[1], "-port")==0 || strcmp(argv[1], "-p")==0)
		{
			char *end;

			SERVICE_PORT = strtol(argv[2], &end, 10);
			if(SERVICE_PORT<=0 || SERVICE_PORT >= 65535)
			{
				SERVICE_PORT = SELF_PORT;
			}
		}
	}

	return 0;
}

std::string getWork()
{
	return WORK_DIR;
}



