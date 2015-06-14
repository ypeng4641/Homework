#pragma once

#include "msg.h"

class AudioSlice
{
private:
	enum 
	{
		DGRAM_SIZE = 32 * 1024,
	};

public:
	AudioSlice(u_int32 base_id, u_int32 frame_number, u_int64 timestamp, const P_AUDIO* p);
	~AudioSlice(void);

public:
	unsigned int slice_count();

	AUTOMEM* slice(unsigned int index);

private:
	std::vector<AUTOMEM*> _slice_vec;
};

