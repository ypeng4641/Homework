#pragma once

#include <x_util/uuid.h>
#include <x_types/intxx.h>

#include <x_pattern/controler.h>

using namespace x_util;

class Stamp2Showtime
{

#define SIZE		30//200

#define CACHE_TIME	200 //unit: ms

public:
	Stamp2Showtime(const Uuid& id);
	~Stamp2Showtime(void);

public:
	Uuid id();

public:
	bool toshowtime2(u_int64 timestamp, u_int64& showtime);
	bool toshowtime (u_int64 timestamp, u_int64& showtime, bool isVirtual, u_int64 sysTime);
	bool getshowtime (u_int64 timestamp, u_int64& showtime, u_int32& frameno, int8& replay, u_int32& framenum, bool isVirtual, u_int64 sysTime);

public:
	void reset(u_int64 timestamp, u_int64 nowtime);

	void getinterval();

	void map2sys(u_int64 time, u_int32& sys_frameno, u_int64& sys_showtime);

	void tosystime(u_int32 sys_frameno, u_int64& sys_showtime);

private:
	Uuid _id;

private:
	bool		_isfirst;

	int32		_adjust_unit;
	int32		_adjust_count;
	u_int32		_frameno;
	u_int64		_vec_time[SIZE];


private:
	u_int64		_base_timestamp;
	double		_base_showtime;
	double		_prev_showtime;
	double		_prev_timestamp;

private:
	u_int32		_prev_sys_frameno;
	u_int64		_prev_sys_showtime;
	u_int32		_interval_val;
	u_int32		_interval_div;

};

