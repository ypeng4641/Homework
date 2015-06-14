#include "VideoSlice.h"
#include <utils/time.h>

extern u_int64		BASE_FRAMENO;

VideoSlice::VideoSlice(u_int32 base_id, u_int32 sys_frameno, u_int64 timestamp, int8 replay, u_int32 framenum, const P_VIDEO* p): 
	_ref_count(1),
	_is_i_frame(p->frametype == 0 ? true : false),
	_timestamp(timestamp)
{
#ifdef OPT_MEM_TO_PTR
	VS_VIDEO_PACKET vvp = {0};
	vvp.packet_type = 0x0;
	vvp.signal_id = p->item;
	vvp.stream_id = p->stream;
	vvp.base_id = base_id;
	vvp.framenum = framenum;
	vvp.sys_basenum = BASE_FRAMENO;
	vvp.sys_framenum = sys_frameno;
	vvp.replay = replay;
	vvp.slicecnt = 1;
	vvp.slicenum = 0;
	vvp.timestamp = timestamp;
	vvp.area = p->area;
	vvp.resol = p->resol;
	vvp.valid = p->valid;
	vvp.codec = p->codec;
	vvp.frametype = p->frametype;
	vvp.datalen = 0;

	AUTOMEM* mem = new AUTOMEM(&vvp, p);
	
	_slice_vec.push_back(mem);
#else
	//
	unsigned int DATA_SIZE = DGRAM_SIZE - sizeof(VS_VIDEO_PACKET);

	// 
	unsigned int slicecnt = p->datalen / DATA_SIZE;
	if(p->datalen % DATA_SIZE != 0)
	{
		slicecnt++;
	}

	//
	unsigned int used_len = 0;
	for(unsigned int i = 0; i < slicecnt; i++)
	{
		//
		unsigned int data_size = p->datalen - used_len;
		if(data_size > DATA_SIZE)
		{
			data_size = DATA_SIZE;
		}

		//
		unsigned int sumlen = sizeof(VS_VIDEO_PACKET) + data_size;

		AUTOMEM* mem = new AUTOMEM(sumlen);
		VS_VIDEO_PACKET* vsp = reinterpret_cast<VS_VIDEO_PACKET*>(mem->get());

		vsp->packet_type = 0x0;
		vsp->signal_id = p->item; 
		vsp->stream_id = p->stream;
		vsp->base_id = base_id;    
		vsp->framenum = framenum; 
		vsp->sys_basenum = BASE_FRAMENO;
		vsp->sys_framenum = sys_frameno;
		vsp->replay   = replay;
		vsp->slicecnt = slicecnt;   
		vsp->slicenum = i;   
		vsp->timestamp = timestamp;  
		vsp->area = p->area;       
		vsp->resol = p->resol;      
		vsp->valid = p->valid;      
		vsp->codec = p->codec;      
		vsp->frametype = p->frametype;  
		vsp->datalen = data_size;    
		memcpy(vsp->data, p->data + used_len, data_size);

		//
		used_len += data_size;
				
		//
		_slice_vec.push_back(mem);
	}
#endif//OPT_MEM_TO_PTR
}

VideoSlice::~VideoSlice(void)
{
	for(std::vector<AUTOMEM*>::iterator it = _slice_vec.begin();
		it != _slice_vec.end(); )
	{
		(*it)->release();
		it = _slice_vec.erase(it);
	}
}

VideoSlice* VideoSlice::get()
{
	_ref_count++;
	return this;
}

void VideoSlice::release()
{
	if((--_ref_count) == 0)
	{
		delete this;
	}
}

bool VideoSlice::is_i_frame()
{
	return _is_i_frame;
}

u_int64 VideoSlice::timestamp()
{
	return _timestamp;
}

unsigned int VideoSlice::slice_count()
{
	return _slice_vec.size();
}

AUTOMEM* VideoSlice::slice(unsigned int index)
{
	return _slice_vec[index];
}


