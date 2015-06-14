#pragma once

#include <x_types/intxx.h>

class Counter
{
private:
	Counter(void);
	~Counter(void);

public:
	int init();
	int deinit();

public:
	u_int64 tick();

public:
	static Counter* instance()
	{
		if(!_instance)
		{
			_instance = new Counter();
		}

		return _instance;
	}

private:
	static Counter* _instance;


private:
	u_int64 _freq;
};

