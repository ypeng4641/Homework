#pragma once

#include "Plugin.h"
#include "imp_uhi01.h"

namespace device
{

class UHI01: public Plugin
{
private:
	enum 
	{
		TYPE = 4,
	};

public:
	UHI01(void)
	{
	}

	~UHI01(void)
	{
	}

public:
	virtual const char* Name()
	{
		return "UHI01";
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
		return imp_uhi01_open(signal, cb, context, item);
	}

	virtual int			Close(void* handle)
	{
		return imp_uhi01_close(handle);
	}

	virtual int			SetIFrame(void* handle)
	{
		return imp_uhi01_set_i_frame(handle);
	}

	virtual int			Error(void* handle)
	{
		return imp_uhi01_error(handle);
	}

};

};