#pragma once

#include "msg.h"

class VideoSlice
{
private:
	enum 
	{
		DGRAM_SIZE = 32 * 1024,
	};

public:
	VideoSlice(u_int32 base_id, u_int32 sys_frameno, u_int64 timestamp, int8 replay, u_int32 framenum, const P_VIDEO* p);

private:
	~VideoSlice(void);

public:
	VideoSlice* get();
	void release();

public:
	bool is_i_frame();
	u_int64 timestamp();

public:
	unsigned int slice_count();

	AUTOMEM* slice(unsigned int index);

private:
	unsigned int _ref_count;

private:

	std::vector<AUTOMEM*> _slice_vec;

private:
	bool _is_i_frame;
	u_int64 _timestamp;

};

