#include "Stamp2Showtime.h"
#include "config.h"

#include <x_log/msglog.h>
#include <sys/types.h>
#include <sys/timeb.h>


extern u_int64		BASE_FRAMENO;

Stamp2Showtime::Stamp2Showtime(const Uuid& id): _id(id), _isfirst(true),_adjust_unit(0),_adjust_count(0), _frameno(0)
{
	_prev_sys_frameno  = 0;
	_prev_sys_showtime = 0;
}


Stamp2Showtime::~Stamp2Showtime(void)
{
}

Uuid Stamp2Showtime::id()
{
	return _id;
}

void Stamp2Showtime::getinterval()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	u_int64 nowtime = ((u_int64)tv.tv_sec) * 1000 + tv.tv_usec / 1000;

	_vec_time[_frameno%SIZE] = nowtime;

	if(_frameno <= 5)
	{
		_interval_val = INTERVAL_VAL;
		_interval_div = INTERVAL_DIV;
	}
	else if(_frameno < SIZE)
	{
		_interval_val = nowtime - _vec_time[0];
		_interval_div = _frameno;
	}
	else
	{
		_interval_val = nowtime - _vec_time[(_frameno+1)%SIZE];
		_interval_div = SIZE-1;
	}

	_frameno++;
}

bool Stamp2Showtime::getshowtime(u_int64 timestamp, u_int64& showtime, u_int32& frameno, int8& replay, u_int32& framenum, bool isVirtual, u_int64 sysTime)
{
	u_int64 tmp_time, sys_showtime;
	u_int32 sys_frameno;
	
	//先时间戳均匀
	toshowtime(timestamp, tmp_time, isVirtual, sysTime);

	//映射到全系统时间戳
	map2sys(tmp_time, sys_frameno, sys_showtime);
	//LOG(LEVEL_INFO, "s2s(%d), timestamp=%llu, tmp_time=%llu, sys_showtime=%llu, sys_frameno=%u, sysTime=%llu, isVirtual=%s.\n", this, timestamp, tmp_time, sys_showtime, sys_frameno, sysTime, (isVirtual)?("true"):("false"));
	//LOG(LEVEL_INFO, "############### sys_frameno=%d, sys_showtime=%lld, baseno=%lld", sys_frameno, sys_showtime, BASE_FRAMENO);

	framenum = _frameno;

#if 0  
	//连续帧编号的算法 
	if(_prev_sys_frameno == 0)
	{
		replay = 0;
		_prev_sys_frameno  = frameno  = sys_frameno;
		_prev_sys_showtime = showtime = sys_showtime;
	}
	else
	{
		if(sys_frameno == (_prev_sys_frameno+1))
		{			
			replay = 0;
			_prev_sys_frameno  = frameno  = sys_frameno;
			_prev_sys_showtime = showtime = sys_showtime;
		}
		else if(sys_frameno > (_prev_sys_frameno+1) )
		{
			replay   = sys_frameno - _prev_sys_frameno - 1;
			frameno  = _prev_sys_frameno + 1;
			tosystime(frameno, showtime);

			_prev_sys_frameno  = sys_frameno;
			tosystime(_prev_sys_frameno, _prev_sys_showtime);
		}
		else if(sys_frameno == _prev_sys_frameno)
		{
			replay = 0;
			frameno  = _prev_sys_frameno + 1;
			tosystime(frameno, showtime);

			_prev_sys_frameno  = frameno;
			_prev_sys_showtime = showtime;
		}
		else if(sys_frameno < _prev_sys_frameno)
		{
			replay = -1;
			_prev_sys_frameno  = frameno  = _prev_sys_frameno;
			_prev_sys_showtime = showtime = _prev_sys_showtime;

		}
	}
#else
	//非连续帧编号的算法 
	if(_prev_sys_frameno == 0)
	{
		replay = 0;
		_prev_sys_frameno  = frameno  = sys_frameno;
		_prev_sys_showtime = showtime = sys_showtime;
	}
	else
	{
		if(sys_frameno == (_prev_sys_frameno+1))
		{			
			replay = 0;
			_prev_sys_frameno  = frameno  = sys_frameno;
			_prev_sys_showtime = showtime = sys_showtime;
		}
		else if(sys_frameno == _prev_sys_frameno)
		{
			replay = 0;
			frameno  = _prev_sys_frameno + 1;
			tosystime(frameno, showtime);

			_prev_sys_frameno  = frameno;
			_prev_sys_showtime = showtime;
		}
		else if(sys_frameno < _prev_sys_frameno)
		{
			replay = -1;
			_prev_sys_frameno  = frameno  = _prev_sys_frameno;
			_prev_sys_showtime = showtime = _prev_sys_showtime;
		}
		else
		{
			replay   = 0;
			_prev_sys_frameno  = frameno  = sys_frameno;
			_prev_sys_showtime = showtime = sys_showtime;
		}
	}
#endif

	if((sys_frameno % 1000) == 0)
	{
		struct timeval tv;
		gettimeofday(&tv, NULL);
		u_int64 nowtime = ((u_int64)tv.tv_sec) * 1000 + tv.tv_usec / 1000;

		LOG(LEVEL_INFO, "replay=%d, frameno=%d, showtime=%lld, rate=%d, diff=%lld", (int)replay, frameno, showtime, _interval_val/_interval_div, showtime-nowtime);
	}

	return true;
}


bool Stamp2Showtime::toshowtime(u_int64 timestamp, u_int64& showtime, bool isVirtual, u_int64 sysTime)
{
	double d_showtime;

	getinterval();

	//不使用均匀时间戳算法
	if(! isVirtual)
	{
		// 第一次计算显示时间的话需要设置时间戳到显示时间的转换基准
		if(_isfirst)
		{
			_isfirst = false;
			reset(timestamp, sysTime);	
		}

		d_showtime = _prev_showtime + (timestamp - _prev_timestamp);

		// 如果转换出的显示时间与当前时间差距大于1s会认为时间戳异常，重置转换基准
		//显示时间小于系统时间，时间戳异常，重置转换基准
		u_int64 diff2 = d_showtime > sysTime ? (d_showtime - sysTime) : (sysTime - d_showtime);
		if((diff2 >= 500) || (d_showtime < sysTime))
		{
			reset(timestamp, sysTime);
			//显示时间重置
			d_showtime = _base_showtime;
			LOG(LEVEL_INFO, "*********************** Reset Showtime");
		}

		_prev_timestamp = timestamp;
		showtime = _prev_showtime = d_showtime;
		return true;
	}

	// 获取当前时间
#if 1
	struct timeval tv;
	gettimeofday(&tv, NULL);
	u_int64 nowtime = ((u_int64)tv.tv_sec) * 1000 + tv.tv_usec / 1000;
#else
	struct _timeb tb;
	_ftime(&tb);
	u_int64 nowtime = ((u_int64)tb.time) * 1000 + tb.millitm;
#endif

	// 第一次计算显示时间的话需要设置时间戳到显示时间的转换基准
	if(_isfirst)
	{
		_isfirst = false;
		reset(timestamp, nowtime);	
	}
	
	//按统计到帧间隔计算显示时间
	d_showtime = _prev_showtime + (double)_interval_val/_interval_div;

	//判断延迟情况
	__int64 delay = d_showtime - nowtime;
	if(delay > CACHE_TIME*150/100)
	{
		_adjust_unit = -2;
		_adjust_count = CACHE_TIME*50/100;
	}
	else if(delay < CACHE_TIME*50/100)
	{
		_adjust_unit = 2;
		_adjust_count = CACHE_TIME*50/100/2;
	}
	else if(abs(delay - CACHE_TIME) < 30)
	{
		_adjust_unit = 0;
		_adjust_count = 0;
	}

	//计算显示时间
	if(_adjust_count-- > 0)
	{	
		d_showtime = _prev_showtime + (double)_interval_val/_interval_div + _adjust_unit;
	}
	else
	{
		d_showtime = _prev_showtime + (double)_interval_val/_interval_div;
	}

	// 如果转换出的显示时间与当前时间差距大于1s会认为时间戳异常，重置转换基准
	u_int64 diff2 = d_showtime > nowtime ? (d_showtime - nowtime) : (nowtime - d_showtime);
	if(diff2 >= 500)// || (d_showtime < nowtime))
	{
		reset(timestamp, nowtime);
		//显示时间重置
		d_showtime = _base_showtime;
		LOG(LEVEL_INFO, "*********************** Reset Showtime");
	}
	
	showtime       = d_showtime;

	//LOG(LEVEL_INFO, "showtime=%lld, %lld interval=%d, %d, %d", showtime, (u_int64)_prev_showtime, _interval, _interval_val, _interval_div);

	_prev_showtime = d_showtime;

	return true;
}

bool Stamp2Showtime::toshowtime2(u_int64 timestamp, u_int64& showtime)
{
	// 获取当前时间
#if 1
	struct timeval tv;
	gettimeofday(&tv, NULL);
	u_int64 nowtime = ((u_int64)tv.tv_sec) * 1000 + tv.tv_usec / 1000;
#else
	struct _timeb tb;
	_ftime(&tb);
	u_int64 nowtime = ((u_int64)tb.time) * 1000 + tb.millitm;
#endif

	// 第一次计算显示时间的话需要设置时间戳到显示时间的转换基准
	if(_isfirst)
	{
		_isfirst = false;
		reset(timestamp, nowtime);	
	}
	
	// 如果当前时间戳与前一帧时间戳差距大于1秒会认为时间戳异常，重置转换基准
	u_int64 diff1 = timestamp > _prev_timestamp ? (timestamp - _prev_timestamp) : (_prev_timestamp - timestamp);
	if(diff1 >= 1000)
	{
		reset(timestamp, nowtime);
	}

	// 如果转换出的显示时间与当前时间差距大于1s会认为时间戳异常，重置转换基准
	u_int64 result = _base_showtime + timestamp - _base_timestamp;
	u_int64 diff2 = result > nowtime ? (result - nowtime) : (nowtime - result);
	if(diff2 >= 1000)
	{
		reset(timestamp, nowtime);
	}

	// 计算显示时间
	showtime = _base_showtime + timestamp - _base_timestamp;
	
	_prev_timestamp = timestamp > _prev_timestamp ? timestamp : _prev_timestamp;

	return true;
}

void Stamp2Showtime::reset(u_int64 timestamp, u_int64 nowtime)
{
	_base_timestamp  = timestamp;
	_base_showtime   = nowtime + CACHE_TIME;
	_prev_timestamp  = _base_timestamp;
		
	_prev_showtime   = _base_showtime;

	_prev_sys_frameno= 0;


	LOG(LEVEL_INFO, "base timestamp = %llu, base showtime = %f", _base_timestamp, _base_showtime);
}

void Stamp2Showtime::map2sys(u_int64 time, u_int32& sys_frameno, u_int64& sys_showtime)
{
	u_int64 frameno  = time * INTERVAL_DIV / INTERVAL_VAL;

	sys_showtime = frameno * INTERVAL_VAL / INTERVAL_DIV;

	sys_frameno = frameno - BASE_FRAMENO;
}

void Stamp2Showtime::tosystime(u_int32 sys_frameno, u_int64& sys_showtime)
{
	sys_showtime = (sys_frameno + BASE_FRAMENO) * INTERVAL_VAL / INTERVAL_DIV;
}
