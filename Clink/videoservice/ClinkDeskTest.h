#pragma once


#include "Plugin.h"
#include "imp_desktop.h"

namespace device
{

	class ClinkDeskPlay: public Plugin
	{
	private:
		enum 
		{
			TYPE = 1234,
		};

	public:
		ClinkDeskPlay(void)
		{
		}

		~ClinkDeskPlay(void)
		{
		}

	public:
		virtual const char* Name()
		{
			return "ClinkDeskPlay";
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
			return imp_clink_play_open(signal, cb, context, item);
		}

		virtual int			Close(void* handle)
		{
			return imp_clink_play_close(handle);
		}

		virtual int			SetIFrame(void* handle)
		{
			return imp_clink_play_set_i_frame(handle);
		}

		virtual int			Error(void* handle)
		{
			return imp_clink_play_error(handle);
		}

	};

};