#pragma once
/*********************************
*
*CLink处理器接入高分虚屏桌面信号的插件
*
**********************************/

#include "Plugin.h"
#include "clink_desktop.h"

namespace device
{

class Desktop: public Plugin
{
private:
	enum
	{
		TYPE = 6,
	};

public:
	Desktop(void)
	{
	}

	~Desktop(void)
	{
	}

public:
	virtual const char* Name()
	{
		return "Desktop";
	}

	virtual u_int32		Type()
	{
		return TYPE;
	}

	virtual int			Init()
	{
		return 0l;
	}

	virtual int			Deinit()
	{
		return 0l;
	}

	virtual void*		Open(const VS_SIGNAL* signal, SigCB cb, void* context, const Uuid& item)
	{
		return clink_desktop_open(signal, cb, context, item);
	}

	virtual int			Close(void* handle)
	{
		return clink_desktop_close(handle);
	}

	virtual int			SetIFrame(void* handle)
	{
		return clink_desktop_set_i_frame(handle);
	}

	virtual int			Error(void* handle)
	{
		return clink_desktop_error(handle);
	}

};

};