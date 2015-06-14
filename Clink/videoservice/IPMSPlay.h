#pragma once


#include "Plugin.h"
#include "imp_ipms_play.h"

namespace device
{

class IPMSPlay: public Plugin
{
private:
	enum 
	{
		TYPE = 0,
	};

public:
	IPMSPlay(void)
	{
	}

	~IPMSPlay(void)
	{
	}

public:
	virtual const char* Name()
	{
		return "IPMSPlay";
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
		return imp_ipms_play_open(signal, cb, context, item);
	}

	virtual int			Close(void* handle)
	{
		return imp_ipms_play_close(handle);
	}

	virtual int			SetIFrame(void* handle)
	{
		return imp_ipms_play_set_i_frame(handle);
	}

	virtual int			Error(void* handle)
	{
		return imp_ipms_play_error(handle);
	}

};

};