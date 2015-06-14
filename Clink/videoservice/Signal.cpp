#include "Signal.h"


Signal::Signal(const Uuid& id, const VS_SIGNAL* signal, void* handle, device::Plugin* plugin, u_int32 sequence): 
	_id(id), 
	_handle(handle), 
	_plugin(plugin), 
	_is_ret(false),
	_is_once(false),
	_sequence(sequence),
	_error(0)
{
	//
	unsigned int len = sizeof(VS_SIGNAL) + signal->datalen;

	//
	_signal = reinterpret_cast<VS_SIGNAL*>(new char[len]);

	//
	memcpy(_signal, signal, len);

	_timeout = GetTickCount64();

}

Signal::~Signal(void)
{
	delete[] reinterpret_cast<char*>(_signal);
}

Uuid Signal::id()
{
	return _id;
}

void Signal::handle(void* h)
{
	_handle = h;
}

void* Signal::handle()
{
	return _handle;
}

const VS_SIGNAL* Signal::signal()
{
	return _signal;
}

device::Plugin* Signal::plugin()
{
	return _plugin;
}

bool Signal::isret()
{
	return _is_ret;
}

void Signal::ret()
{
	_is_ret = true;
}

bool Signal::isonce()
{
	return _is_once;
}

void Signal::once()
{
	_is_once = true;
}

void Signal::reset_once()
{
	_is_once = false;
}

bool Signal::desc_timeout()
{
	if(GetTickCount64() - _timeout >= 6000)
	{
		return true;
	}

	return false;
}

u_int32 Signal::sequence()
{
	return _sequence;
}

int	Signal::error()
{
	return _error;
}

void Signal::error(int e)
{
	_error = e;
}






