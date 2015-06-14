#include "Counter.h"

#include "Transfer.h"

#include <x_log/msglog.h>

Counter* Counter::_instance = NULL;

Counter::Counter(void)
{
}


Counter::~Counter(void)
{
}

int Counter::init()
{
	if(!QueryPerformanceFrequency((LARGE_INTEGER *)&_freq))
	{
		return -1;
	}

	return 0;
}

int Counter::deinit()
{
	return 0l;
}

u_int64 Counter::tick()
{
	u_int64 v;
	QueryPerformanceCounter((LARGE_INTEGER *)&v);

	return u_int64(double(v) / _freq * 1000);
}

