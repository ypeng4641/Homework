#pragma once

#include <x_util/uuid.h>
#include <x_vs/xvs.h>

#include "Plugin.h"

using namespace x_util;

class Signal
{
public:
	Signal(const Uuid& id, const VS_SIGNAL* signal, void* handle, device::Plugin* plugin, u_int32 sequence);
	~Signal(void);

public:
	Uuid id();

	void handle(void* h);
	void* handle();

	const VS_SIGNAL* signal();
	device::Plugin* plugin();

public:
	bool isret();
	void ret();
	bool isonce();
	void once();
	void reset_once();

public:
	bool desc_timeout();

public:
	u_int32 sequence();

public:
	int	error();
	void error(int e);

private:
	Uuid _id;
	VS_SIGNAL* _signal;
	void* _handle;
	device::Plugin* _plugin;

private:
	bool _is_ret;
	bool _is_once;

private:
	u_int64 _timeout;

private:
	u_int32 _sequence;

private:
	int	_error;
};

