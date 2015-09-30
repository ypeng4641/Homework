#pragma once

#include "Plugin.h"
#include "imp_gb28181.h"


namespace device
{

class GB28181: public Plugin
{
private:
	enum 
	{
		TYPE = 10104,
	};

public:
	GB28181(void)
	{
	}

	~GB28181(void)
	{
	}

public:
	virtual const char* Name()
	{
		return "GB28181";
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
		return imp_gb28181_open(signal, cb, context, item);
	}

	virtual int			Close(void* handle)
	{
		return imp_gb28181_close(handle);
	}

	virtual int			SetIFrame(void* handle)
	{
		return imp_gb28181_set_i_frame(handle);
	}

	virtual int			Error(void* handle)
	{
		return imp_gb28181_error(handle);
	}

};

};

