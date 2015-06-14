#ifndef WATCHDOG_H
#define WATCHDOG_H

#include "pthread.h"
#include <stdio.h>

#ifdef __linux__
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#else
#include <windows.h>
#include <process.h>
#endif

#define TIME_UNIT				100			// 100 miliseconds
#define MAX_FOOD_NUMBER			180000		// miliseconds
#define MIN_FOOD_NUMBER			0

class Watchdog
{
public:
	Watchdog()
	{
		_hungryLimit	= MAX_FOOD_NUMBER;
		_food			= _hungryLimit;
			
		pthread_mutex_init(&_mutex, NULL);
	}
	~Watchdog()
	{
		pthread_mutex_destroy(&_mutex);
	}

	bool	Init()
	{
		if(0 != pthread_create(&_thread, NULL, &Watchdog::Eating, (void*)this))
		{
			return false;
		}
		else
		{
			return true;
		}
	}

	bool	Cleanup()
	{
		Lock();
		_food	= MIN_FOOD_NUMBER;
		Unlock();
		pthread_join(_thread, NULL);

		return true;
	}
	
	void	SetHungryLimit(int limit)  // seconds 
	{
		if(limit > MIN_FOOD_NUMBER)
		{
			_hungryLimit	= limit*1000;
			Feed();
		}
	}

	void	Feed()
	{
		Lock();
		_food	= _hungryLimit;
		Unlock();
	}

	bool	GetFood()
	{
		Lock();
		if(_food > MIN_FOOD_NUMBER)
		{
			_food	-= TIME_UNIT;
			Unlock();
			return true;
		}
		else
		{
			Unlock();
			return false;
		}
	}

private:
	static	void* Eating(void* param)
	{
		Watchdog*	dog	= (Watchdog*)param;

		while(1)
		{
#ifdef __linux__
			usleep(TIME_UNIT*1000);
#else
			Sleep(TIME_UNIT);
#endif
			if(!dog->GetFood())
			{
				//WriteLog();
				_exit(-10101);
			}
		}

		return NULL;
	}

	void Lock()
	{
		while((pthread_mutex_lock(&_mutex) != 0) && errno == EINTR){}
	}

	void Unlock()
	{
		while((pthread_mutex_unlock(&_mutex) != 0) && errno == EINTR){}
	}

	static void WriteLog()
	{
		time_t	nowtime;
		time(&nowtime);
		struct tm* now = localtime(&nowtime);
		char	str[256];
		memset(str, 0, sizeof(str));
		sprintf(str, "%04d/%02d/%02d %02d:%02d:%02d pid %d exit!\n", now->tm_year+1900, now->tm_mon+1, now->tm_mday, now->tm_hour, now->tm_min, now->tm_sec, getpid());

		FILE* fp = fopen("Watchdog.log", "a+");
		if(fp)
		{
			fwrite(str, 1, strlen(str), fp);
			fclose(fp);
		}
	}

private:
	pthread_mutex_t			_mutex;
	pthread_t				_thread;
	int						_food;
	int						_hungryLimit;
};

#endif
