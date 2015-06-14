#pragma once

#include "Plugin.h"
#include "imp_ri01.h"


namespace device
{

class RI01: public Plugin
{
private:
	enum 
	{
		TYPE = 2,
	};

public:
	RI01(void)
	{
	}

	~RI01(void)
	{
	}

public:
	virtual const char* Name()
	{
		return "RI01";
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
		return imp_ri01_open(signal, cb, context, item);
	}

	virtual int			Close(void* handle)
	{
		return imp_ri01_close(handle);
	}

	virtual int			SetIFrame(void* handle)
	{
		return imp_ri01_set_i_frame(handle);
	}

	virtual int			Error(void* handle)
	{
		return imp_ri01_error(handle);
	}

};

};

