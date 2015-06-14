#pragma once

#include "Plugin.h"
#include "imp_ipms_replay.h"

namespace device
{

class IPMSReplay: public Plugin
{
private:
	enum 
	{
		TYPE = 1,
	};

public:
	IPMSReplay(void)
	{
	}

	~IPMSReplay(void)
	{
	}

public:
	virtual const char* Name()
	{
		return "IPMSReplay";
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
		return imp_ipms_replay_open(signal, cb, context, item);
	}

	virtual int			Close(void* handle)
	{
		return imp_ipms_replay_close(handle);
	}

	virtual int			SetIFrame(void* handle)
	{
		return imp_ipms_replay_set_i_frame(handle);
	}

	virtual int			Error(void* handle)
	{
		return imp_ipms_replay_error(handle);
	}

};

};