#ifndef	__VIEW_H__
#define	__VIEW_H__

#define	WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace clink
{
typedef int (__stdcall *RUN_SRV_PROC)(unsigned short port);
typedef int (__stdcall *END_SRV_PROC)();
typedef int (__stdcall *PUT_DAT_PROC)(const void* data, int size);
typedef int (__stdcall *RUNNING_PROC)();

class ds
{
public:
	ds()
	{
		_mod = ::LoadLibraryA("datsvr.dll");
		if (_mod)
		{
			_run_srv = (RUN_SRV_PROC)::GetProcAddress(_mod, "run_srv");
			_end_srv = (END_SRV_PROC)::GetProcAddress(_mod, "end_srv");
			_put_dat = (PUT_DAT_PROC)::GetProcAddress(_mod, "put_dat");
			_running = (RUNNING_PROC)::GetProcAddress(_mod, "running");
		}
		else
		{
			printf("LoadLibrary('datsvr.dll') failed!\n");
		}
	}

	~ds()
	{
		if (_mod)
		{
			::FreeLibrary(_mod);
		}
	}

	int run(unsigned short port)
	{
		if (_run_srv)
		{
			return _run_srv(port);
		}

		return -1000;
	}

	int end()
	{
		if (_end_srv)
		{
			return _end_srv();
		}

		return -1000;
	}

	bool is_running()
	{
		if (_running)
		{
			return _running() ? true : false;
		}

		return false;
	}

	int put(const void* data, int size)
	{
		if (_put_dat)
		{
			return _put_dat(data, size);
		}

		return -1000;
	}

private:
	HMODULE			_mod;
	RUN_SRV_PROC	_run_srv;
	END_SRV_PROC	_end_srv;
	PUT_DAT_PROC	_put_dat;
	RUNNING_PROC	_running;
};

} // namespace

inline int post_view_data(unsigned short port, const void* data, int size)
{
	static clink::ds s_ds;

	if (!s_ds.is_running())
	{
		s_ds.run(port);
	}

	return s_ds.put(data, size);

	return 0;
}

#endif	// __VIEW_H__