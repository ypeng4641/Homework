
#ifndef _H264_H_
#define _H264_H_
#include <stdint.h>

// NAL unit types
typedef enum {
	NAL_SLICE=1,        // 1
	NAL_DPA,            // 2
	NAL_DPB,            // 3
	NAL_DPC,            // 4
	NAL_IDR_SLICE,      // 5
	NAL_SEI,            // 6
	NAL_SPS,            // 7
	NAL_PPS,            // 8
	NAL_AUD,            // 9
	NAL_END_SEQUENCE,   // 10
	NAL_END_STREAM,     // 11
	NAL_FILLER_DATA,    // 12
	NAL_SPS_EXT,        // 13
	NAL_AUXILIARY_SLICE=19
}NALType;

typedef struct  
{
	uint8_t forbidden_bit;				// 0
	uint8_t nal_reference_bit;			// nal	优先级
	uint8_t nal_unit_type;				// nal 类型

	uint8_t * nal_ptr;					// 原始的nal数据部分
	uint32_t nal_length;				// 原始的nal数据部分长度
}NALHeader;

class NalRbsp
{
public:
	explicit NalRbsp(uint32_t length){
		rbsp_ =new uint8_t[length];
		length_=0;
	}
	virtual ~NalRbsp(){
		delete [] rbsp_;
	}
	uint8_t *rbsp_;
	uint32_t length_;
};

enum AVColorPrimaries{
	AVCOL_PRI_BT709      =1, ///< also ITU-R BT1361 / IEC 61966-2-4 / SMPTE RP177 Annex B
	AVCOL_PRI_UNSPECIFIED=2,
	AVCOL_PRI_BT470M     =4,
	AVCOL_PRI_BT470BG    =5, ///< also ITU-R BT601-6 625 / ITU-R BT1358 625 / ITU-R BT1700 625 PAL & SECAM
	AVCOL_PRI_SMPTE170M  =6, ///< also ITU-R BT601-6 525 / ITU-R BT1358 525 / ITU-R BT1700 NTSC
	AVCOL_PRI_SMPTE240M  =7, ///< functionally identical to above
	AVCOL_PRI_FILM       =8,
	AVCOL_PRI_NB           , ///< Not part of ABI
};

enum AVColorTransferCharacteristic{
	AVCOL_TRC_BT709      =1, ///< also ITU-R BT1361
	AVCOL_TRC_UNSPECIFIED=2,
	AVCOL_TRC_GAMMA22    =4, ///< also ITU-R BT470M / ITU-R BT1700 625 PAL & SECAM
	AVCOL_TRC_GAMMA28    =5, ///< also ITU-R BT470BG
	AVCOL_TRC_NB           , ///< Not part of ABI
};

enum AVColorSpace{
	AVCOL_SPC_RGB        =0,
	AVCOL_SPC_BT709      =1, ///< also ITU-R BT1361 / IEC 61966-2-4 xvYCC709 / SMPTE RP177 Annex B
	AVCOL_SPC_UNSPECIFIED=2,
	AVCOL_SPC_FCC        =4,
	AVCOL_SPC_BT470BG    =5, ///< also ITU-R BT601-6 625 / ITU-R BT1358 625 / ITU-R BT1700 625 PAL & SECAM / IEC 61966-2-4 xvYCC601
	AVCOL_SPC_SMPTE170M  =6, ///< also ITU-R BT601-6 525 / ITU-R BT1358 525 / ITU-R BT1700 NTSC / functionally identical to above
	AVCOL_SPC_SMPTE240M  =7,
	AVCOL_SPC_NB           , ///< Not part of ABI
};

/**
 * rational number numerator/denominator
 */
typedef struct AVRational{
    int num; ///< numerator
    int den; ///< denominator
} AVRational;

/**
 * Sequence parameter set
 */
typedef struct SPS{
    int profile_idc;
    int level_idc;
    int chroma_format_idc;
    int transform_bypass;              ///< qpprime_y_zero_transform_bypass_flag
    int log2_max_frame_num;            ///< log2_max_frame_num_minus4 + 4
    int poc_type;                      ///< pic_order_cnt_type
    int log2_max_poc_lsb;              ///< log2_max_pic_order_cnt_lsb_minus4
    int delta_pic_order_always_zero_flag;
    int offset_for_non_ref_pic;
    int offset_for_top_to_bottom_field;
    int poc_cycle_length;              ///< num_ref_frames_in_pic_order_cnt_cycle
    int ref_frame_count;               ///< num_ref_frames
    int gaps_in_frame_num_allowed_flag;
    int mb_width;                      ///< pic_width_in_mbs_minus1 + 1
    int mb_height;                     ///< pic_height_in_map_units_minus1 + 1
    int frame_mbs_only_flag;
    int mb_aff;                        ///<mb_adaptive_frame_field_flag
    int direct_8x8_inference_flag;
    int crop;                   ///< frame_cropping_flag
    unsigned int crop_left;            ///< frame_cropping_rect_left_offset
    unsigned int crop_right;           ///< frame_cropping_rect_right_offset
    unsigned int crop_top;             ///< frame_cropping_rect_top_offset
    unsigned int crop_bottom;          ///< frame_cropping_rect_bottom_offset
    int vui_parameters_present_flag;
    AVRational sar;
    int video_signal_type_present_flag;
    int full_range;
    int colour_description_present_flag;
    enum AVColorPrimaries color_primaries;
    enum AVColorTransferCharacteristic color_trc;
    enum AVColorSpace colorspace;
    int timing_info_present_flag;
    uint32_t num_units_in_tick;
    uint32_t time_scale;
    int fixed_frame_rate_flag;
    short offset_for_ref_frame[256]; //FIXME dyn aloc?
    int bitstream_restriction_flag;
    int num_reorder_frames;
    int scaling_matrix_present;
    uint8_t scaling_matrix4[6][16];
    uint8_t scaling_matrix8[6][64];
    int nal_hrd_parameters_present_flag;
    int vcl_hrd_parameters_present_flag;
    int pic_struct_present_flag;
    int time_offset_length;
    int cpb_cnt;                       ///< See H.264 E.1.2
    int initial_cpb_removal_delay_length; ///< initial_cpb_removal_delay_length_minus1 +1
    int cpb_removal_delay_length;      ///< cpb_removal_delay_length_minus1 + 1
    int dpb_output_delay_length;       ///< dpb_output_delay_length_minus1 + 1
    int bit_depth_luma;                ///< bit_depth_luma_minus8 + 8
    int bit_depth_chroma;              ///< bit_depth_chroma_minus8 + 8
    int residual_color_transform_flag; ///< residual_colour_transform_flag
    int constraint_set_flags;          ///< constraint_set[0-3]_flag
}SPS;

/**
 * Picture parameter set
 */
typedef struct PPS{
    unsigned int sps_id;
    int cabac;                  ///< entropy_coding_mode_flag
    int pic_order_present;      ///< pic_order_present_flag
    int slice_group_count;      ///< num_slice_groups_minus1 + 1
    int mb_slice_group_map_type;
    unsigned int ref_count[2];  ///< num_ref_idx_l0/1_active_minus1 + 1
    int weighted_pred;          ///< weighted_pred_flag
    int weighted_bipred_idc;
    int init_qp;                ///< pic_init_qp_minus26 + 26
    int init_qs;                ///< pic_init_qs_minus26 + 26
    int chroma_qp_index_offset[2];
    int deblocking_filter_parameters_present; ///< deblocking_filter_parameters_present_flag
    int constrained_intra_pred; ///< constrained_intra_pred_flag
    int redundant_pic_cnt_present; ///< redundant_pic_cnt_present_flag
    int transform_8x8_mode;     ///< transform_8x8_mode_flag
    uint8_t scaling_matrix4[6][16];
    uint8_t scaling_matrix8[6][64];
    uint8_t chroma_qp_table[2][64];  ///< pre-scaled (with chroma_qp_index_offset) version of qp_table
    int chroma_qp_diff;
}PPS;

class H264Parse
{
public:
	// 检测缓冲中是否有NAL头及NAL基本信息
	static bool CheckNal(uint8_t * buffer, uint32_t buffer_length, NALHeader * header);

	// 定位NAL开始位置, 返回当前NAL起始位置
	static bool FindNAL(uint8_t * buffer, uint32_t buffer_length, NALHeader * header, uint32_t *use_length);

	// 获取NAL对应的原始码流
	static bool GetRBSP(NALHeader * header, NalRbsp * rbsp);

	// 获取SPS
	static bool GetSeqParameterSet(NalRbsp * rbsp, SPS * sps);
    
	// 获取码流尺寸
	static bool GetFrameSize(SPS sps, uint16_t *width, uint16_t *height);

	// 解释分辨率
	static bool GetResolution(uint8_t* buffer, uint32_t buffer_lenght, uint16_t* width, uint16_t* height);
};

#endif

