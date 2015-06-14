#include "AudioSlice.h"


AudioSlice::AudioSlice(u_int32 base_id, u_int32 frame_number, u_int64 timestamp, const P_AUDIO* p)
{
	unsigned int DATA_SIZE = DGRAM_SIZE - sizeof(VS_AUDIO_PACKET);


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
		unsigned int sumlen = sizeof(VS_AUDIO_PACKET) + data_size;
		AUTOMEM* mem = new AUTOMEM(sumlen);
		VS_AUDIO_PACKET* vsp = reinterpret_cast<VS_AUDIO_PACKET*>(mem->get());

		//
		vsp->packet_type = 0x1;
		vsp->signal_id = p->item;
		vsp->stream_id = p->stream;
		vsp->base_id = base_id;
		vsp->framenum = frame_number;
		vsp->slicecnt = slicecnt;
		vsp->slicenum = i;
		vsp->timestamp = timestamp;
		vsp->codec = p->codec;
		vsp->samples = p->samples;
		vsp->channels = p->channels;
		vsp->bit_per_samples = p->bit_per_samples;
		vsp->datalen = data_size;
		memcpy(vsp->data, p->data + used_len, data_size);

		//
		used_len += data_size;

		//
		_slice_vec.push_back(mem);
	}
}


AudioSlice::~AudioSlice(void)
{
	for(std::vector<AUTOMEM*>::iterator it = _slice_vec.begin();
		it != _slice_vec.end(); )
	{
		(*it)->release();
		it = _slice_vec.erase(it);
	}
}

unsigned int AudioSlice::slice_count()
{
	return _slice_vec.size();
}


AUTOMEM* AudioSlice::slice(unsigned int index)
{
	return _slice_vec[index];
}


