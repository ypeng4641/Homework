#pragma once

#include "Plugin.h"
#include "imp_vi04.h"

namespace device
{

class VI04: public Plugin
{
private:
	enum 
	{
		TYPE = 3,
	};

public:
	VI04(void)
	{
	}

	~VI04(void)
	{
	}

public:
	virtual const char* Name()
	{
		return "VI04";
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
		return imp_vi04_open(signal, cb, context, item);
	}

	virtual int			Close(void* handle)
	{
		return imp_vi04_close(handle);
	}

	virtual int			SetIFrame(void* handle)
	{
		return imp_vi04_set_i_frame(handle);
	}

	virtual int			Error(void* handle)
	{
		return imp_vi04_error(handle);
	}

};

};
