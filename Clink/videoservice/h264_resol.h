#include <stdio.h>
#include <stdlib.h>    
#include <string.h>
#include <assert.h>

//extern ReadResolution* ReadResolution::m_once;
//must be inintialize in sth.cpp

#ifndef _H264_RESOL_H
#define _H264_RESOL_H

class ReadResolution
{
public:
	ReadResolution(void):m_pStart(NULL), m_nLength(0), m_nCurrentBit(0)
	{

	}

	~ReadResolution(void)
	{

	}
	
public:
	static void FindJPGFileResolution(char *cpFileName, int *ipWidth, int *ipHeight) 
	{
		if(NULL == m_once)
		{
			m_once = new ReadResolution;
		}
		else
		{
			m_once->m_pStart = NULL;
			m_once->m_nLength = 0;
			m_once->m_nCurrentBit = 0;
		}

		int i;

		FILE *fp = fopen(cpFileName,"rb");
		fseek(fp,0,SEEK_END);
		long len = ftell(fp);
		fseek(fp,0,SEEK_SET);

		unsigned char *ucpInBuffer = (unsigned char*) malloc (len+1);
		fread(ucpInBuffer,1,len,fp);
		fclose(fp);

		printf("\n\nBuffer size %ld\n", len);   
		for(i=0;i<len;i++)
		{           
			if( 
				(ucpInBuffer[i]==0x00) && (ucpInBuffer[i+1]==0x00) && 
				(ucpInBuffer[i+2]==0x00) && (ucpInBuffer[i+3]==0x01) 
			  )
			{
				if(ucpInBuffer[i+4] & 0x07 == 0x07)
				{
					int Width, Height;
					m_once->Parse(&ucpInBuffer[i+5], len, Width, Height);              
					break;
				}
			}       
		}

		free(ucpInBuffer);
		return;
	}

	static bool FindJPGFileResolution(const unsigned char * pStart, unsigned short nLen, int& Width, int& Height) 
	{   
		if(NULL == m_once)
		{
			m_once = new ReadResolution;
		}
		else
		{
			m_once->m_pStart = NULL;
			m_once->m_nLength = 0;
			m_once->m_nCurrentBit = 0;
		}

		int i;  
		for(i=0;i<nLen;i++)
		{           
			if( 
				(pStart[i]==0x00) && (pStart[i+1]==0x00) && 
				(pStart[i+2]==0x00) && (pStart[i+3]==0x01) 
			  )
			{
				if(pStart[i+4] & 0x07 == 0x07)
				{
					m_once->Parse(&pStart[i+5], nLen, Width, Height);              
					return true;
				}
			}
		}
		return false;
	}

private:
	const unsigned char * m_pStart;
	unsigned short m_nLength;
	int m_nCurrentBit;

	static ReadResolution* m_once;

private:
	unsigned int ReadBit()
	{
		assert(m_nCurrentBit <= m_nLength * 8);
		int nIndex = m_nCurrentBit / 8;
		int nOffset = m_nCurrentBit % 8 + 1;

		m_nCurrentBit ++;
		return (m_pStart[nIndex] >> (8-nOffset)) & 0x01;
	}

	unsigned int ReadBits(int n)
	{
		int r = 0;
		int i;
		for (i = 0; i < n; i++)
		{
			r |= ( ReadBit() << ( n - i - 1 ) );
		}
		return r;
	}

	unsigned int ReadExponentialGolombCode()
	{
		int r = 0;
		int i = 0;

		while( (ReadBit() == 0) && (i < 32) )
		{
			i++;
		}

		r = ReadBits(i);
		r += (1 << i) - 1;
		return r;
	}

	unsigned int ReadSE() 
	{
		int r = ReadExponentialGolombCode();
		if (r & 0x01)
		{
			r = (r+1)/2;
		}
		else
		{
			r = -(r/2);
		}
		return r;
	}

	bool Parse(const unsigned char * pStart, unsigned short nLen, int& Width, int& Height)
	{
		m_pStart = pStart;
		m_nLength = nLen;
		m_nCurrentBit = 0;

		int frame_crop_left_offset=0;
		int frame_crop_right_offset=0;
		int frame_crop_top_offset=0;
		int frame_crop_bottom_offset=0;

		int profile_idc = ReadBits(8);          
		int constraint_set0_flag = ReadBit();   
		int constraint_set1_flag = ReadBit();   
		int constraint_set2_flag = ReadBit();   
		int constraint_set3_flag = ReadBit();   
		int constraint_set4_flag = ReadBit();   
		int constraint_set5_flag = ReadBit();   
		int reserved_zero_2bits  = ReadBits(2); 
		int level_idc = ReadBits(8);            
		int seq_parameter_set_id = ReadExponentialGolombCode();


		if( profile_idc == 100 || profile_idc == 110 ||
			profile_idc == 122 || profile_idc == 244 ||
			profile_idc == 44 || profile_idc == 83 ||
			profile_idc == 86 || profile_idc == 118 )
		{
			int chroma_format_idc = ReadExponentialGolombCode();

			if( chroma_format_idc == 3 )
			{
				int residual_colour_transform_flag = ReadBit();         
			}
			int bit_depth_luma_minus8 = ReadExponentialGolombCode();        
			int bit_depth_chroma_minus8 = ReadExponentialGolombCode();      
			int qpprime_y_zero_transform_bypass_flag = ReadBit();       
			int seq_scaling_matrix_present_flag = ReadBit();        

			if (seq_scaling_matrix_present_flag) 
			{
				int i=0;
				for ( i = 0; i < 8; i++) 
				{
					int seq_scaling_list_present_flag = ReadBit();
					if (seq_scaling_list_present_flag) 
					{
						int sizeOfScalingList = (i < 6) ? 16 : 64;
						int lastScale = 8;
						int nextScale = 8;
						int j=0;
						for ( j = 0; j < sizeOfScalingList; j++) 
						{
							if (nextScale != 0) 
							{
								int delta_scale = ReadSE();
								nextScale = (lastScale + delta_scale + 256) % 256;
							}
							lastScale = (nextScale == 0) ? lastScale : nextScale;
						}
					}
				}
			}
		}

		int log2_max_frame_num_minus4 = ReadExponentialGolombCode();
		int pic_order_cnt_type = ReadExponentialGolombCode();
		if( pic_order_cnt_type == 0 )
		{
			int log2_max_pic_order_cnt_lsb_minus4 = ReadExponentialGolombCode();
		}
		else if( pic_order_cnt_type == 1 )
		{
			int delta_pic_order_always_zero_flag = ReadBit();
			int offset_for_non_ref_pic = ReadSE();
			int offset_for_top_to_bottom_field = ReadSE();
			int num_ref_frames_in_pic_order_cnt_cycle = ReadExponentialGolombCode();
			int i;
			for( i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++ )
			{
				ReadSE();
				//sps->offset_for_ref_frame[ i ] = ReadSE();
			}
		}

		int max_num_ref_frames = ReadExponentialGolombCode();
		int gaps_in_frame_num_value_allowed_flag = ReadBit();
		int pic_width_in_mbs_minus1 = ReadExponentialGolombCode();
		int pic_height_in_map_units_minus1 = ReadExponentialGolombCode();
		int frame_mbs_only_flag = ReadBit();
		if( !frame_mbs_only_flag )
		{
			int mb_adaptive_frame_field_flag = ReadBit();
		}
		int direct_8x8_inference_flag = ReadBit();
		int frame_cropping_flag = ReadBit();
		if( frame_cropping_flag )
		{
			frame_crop_left_offset = ReadExponentialGolombCode();
			frame_crop_right_offset = ReadExponentialGolombCode();
			frame_crop_top_offset = ReadExponentialGolombCode();
			frame_crop_bottom_offset = ReadExponentialGolombCode();
		}
		int vui_parameters_present_flag = ReadBit();
		pStart++;

		Width = ((pic_width_in_mbs_minus1 +1)*16) - frame_crop_right_offset*2 - frame_crop_left_offset*2;
		Height = ((2 - frame_mbs_only_flag)* (pic_height_in_map_units_minus1 +1) * 16) - (frame_crop_bottom_offset * 2) - (frame_crop_top_offset * 2);

		printf("\n\nWxH = %dx%d\n\n",Width,Height);
		return true;
	}

};//class ReadResolution

#endif//_H264_RESOL_H