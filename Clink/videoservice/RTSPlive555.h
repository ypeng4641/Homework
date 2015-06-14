#pragma once 

#include "Plugin.h"
#include "imp_rtsplive555.h"

namespace device
{

class RTSPlive555: public Plugin
{
private:
	enum 
	{
		TYPE = 10055,
	};

public:
	RTSPlive555(void)
	{
	}

	~RTSPlive555(void)
	{
	}

public:
	virtual const char* Name()
	{
		return "RTSPlive555";
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
		return imp_rtsplive555_open(signal, cb, context, item);
	}

	virtual int			Close(void* handle)
	{
		return imp_rtsplive555_close(handle);
	}

	virtual int			SetIFrame(void* handle)
	{
		return imp_rtsplive555_set_i_frame(handle);
	}

	virtual int			Error(void* handle)
	{
		return imp_rtsplive555_error(handle);
	}

};

};