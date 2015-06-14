#pragma once

#include <x_vs/xvs.h>

class TfMethod
{
public:
	typedef void (*SetJoin)(const VS_JOIN* output, void* context);
	typedef void (*SetBase)(const VS_SETBASE* output, void* context);
	typedef void (*SetMulticast)(const VS_MULTICAST* output, void* context);
	typedef void (*SetUnicast)(const VS_UNICAST* output, void* context);

public:
	TfMethod(SetJoin i1, SetBase i2, SetMulticast i3, SetUnicast i4, void* c):
	  _join(i1), _base(i2), _multicast(i3), _unicast(i4), _context(c)
	{
	}

	~TfMethod(void)
	{
	}

public:
	void join(const VS_JOIN* output)
	{
		_join(output, _context);
	}

	void base(const VS_SETBASE* output)
	{
		_base(output, _context);
	}

	void multicast(const VS_MULTICAST* output)
	{
		_multicast(output, _context);
	}

	void unicast(const VS_UNICAST* output)
	{
		_unicast(output, _context);
	}

private:
	SetJoin			_join;
	SetBase			_base;
	SetMulticast	_multicast;
	SetUnicast		_unicast;
	void*			_context;

};

