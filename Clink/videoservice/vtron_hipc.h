#ifndef _VTRON_HIPC_H_
#define _VTRON_HIPC_H_
#include <network/network.h>
#include <x_log/msglog.h>

#define Uint8 unsigned char
typedef char Int8;              ///< Signed  8-bit integer

#define Uint16 unsigned short
#define Int16  short
typedef unsigned int Uint32;    ///< Unsigned 32-bit integer
typedef	char CHAR;

//与x1000定义不同
#define    HIPC_CODEC_PCM				0
#define    HIPC_CODEC_G711				1
#define    HIPC_CODEC_AAC				2
#define    HIPC_CODEC_G726				3

#pragma pack(push)
#pragma pack(1)

//namespace HIPC
//{
typedef struct
{
	char proxyserver[32];
	Uint32 channel;
	Uint8 streamID;
	char OSD[48];
}PARAMS_DEF;

typedef struct
{                      
	Uint32    sync;         //协议版本号
	Uint32    msgType;      //消息类型 ID
	Uint32    msgCode;      //消息编号
	Uint32	  len;          //消息长度
	Uint32    errCode;      //错误码
}T_HIPC_NET_HEADER;

typedef struct _IPC_CMD_VIDEO_INFO
{//请求0x8201，应答0x8202
    T_HIPC_NET_HEADER	   head;      //协议头
    Uint32	sig_chan;			//板卡的处理通道标号
	Uint8	streamID;			//每个处理通道的码流标号
    Int8    video_encode_type;	//0--264;1--mepg2;2--MJPEG
    Int16   width;				//宽度
    Int16   height;				//高度 
	Uint32	reserved;			//保留字节
}T_IPC_VIDEO_INFO_REQ, T_IPC_VIDEO_INFO_RESP;

typedef struct _IPC_CMD_AUDIO_INFO
{//请求0x8209，应答0x8210
	T_HIPC_NET_HEADER	head;
	Uint32				sig_chan;			//板卡的处理通道标号
	Uint16				audio_encode_type;	//音频编码格式0--PCM;1—G.711;2--AAC
	Uint32				audio_sample_rate;	//音频采样频率率
	Uint8				audio_sample_bit;	//采样位数
	Uint8				audio_sample_chan;	//音频声道数0—左声道，1—右声道，2—双声道
	Uint32				reserved;			//保留字节
}T_IPC_AUDIO_INFO_REQ, T_IPC_AUDIO_INFO_RESP;

typedef  struct _T_IPC_VIDEO_CONNECT
{//请求0x8203，应答0x8204
    T_HIPC_NET_HEADER	head;
	Uint32				sig_chan;		//板卡的处理通道标号
	Uint8				streamID;		//每个处理通道的码流标号
	Uint8				ipAddress[64];	// 解码端自己的ip地址(当请求RTSP时，回码为rtsp的URL地址)
	Uint16				port;			//如果是tcp解码端使用该端口连接视频数据
	Uint8				mode;			//视频发送方式 0:UDP组播 1: UDP 2: TCP 3:RTSP.
	Uint32				reserved;		//保留字节
}T_IPC_VIDEO_CONNECT_REQ, T_IPC_VIDEO_CONNECT_RESP;

typedef  struct _T_IPC_VIDEO_DISCONNECT
{//请求0x8205，应答0x8206
    T_HIPC_NET_HEADER	head;
	Uint32				sig_chan;		//板卡的处理通道标号
	Uint8				streamID;		//每个处理通道的码流标号
	Uint8				ipAddress [64];	// 解码端自己的ip地址
	Uint16				port;			//如果是tcp解码端使用该端口连接视频数据
	Uint8				mode;			//视频发送方式0:UDP组播 1: UDP 2: TCP 3:RTSP.
	Uint32				reserved;		//保留字节
}T_IPC_VIDEO_DISCONNECT_REQ, T_IPC_VIDEO_DISCONNECT_RESP;

typedef  struct _T_IPC_AUDIO_CONNECT
{//请求0x8300，应答0x8301
    T_HIPC_NET_HEADER	head;
	Uint32				sig_chan;		//板卡的处理通道标号  
	Uint8				audioID;		//与对应视频码流相关联的音频流
	Uint8				ipAddress[64];	//解码端自己的ip地址。
	Uint16				port;			//如果是tcp解码端使用该端口连接音频数据
	Uint8				mode;			//视频发送方式 0:UDP组播 1: UDP 2: TCP 3:RTSP.
	Uint32				reserved;		//保留字节
}T_IPC_AUDIO_CONNECT_REQ, T_IPC_AUDIO_CONNECT_RESP;

typedef  struct _T_IPC_AUDIO_DISCONNECT
{//请求0x8302，应答0x8303
    T_HIPC_NET_HEADER	head;
	Uint32				sig_chan;		//板卡的处理通道标号  
	Uint8				audioID;		//与对应视频码流相关联的音频流
	Uint8				ipAddress[64];	//解码端自己的ip地址
	Uint16				port;			//如果是tcp解码端使用该端口连接编码
	Uint8				mode;			//视频发送方式 0:UDP组播 1: UDP 2: TCP 3:RTSP.
	Uint32				reserved;		//保留字节
}T_IPC_AUDIO_DISCONNECT_REQ, T_IPC_AUDIO_DISCONNECT_RESP;

typedef struct _T_IPC_VIDEO_I_FRAME
{//请求0x8207，应答0x8208
	T_HIPC_NET_HEADER	head;
	Uint32				sig_chan;	//板卡的处理通道标号
	Uint8				streamID;	//每个处理通道的码流标号      
}T_IPC_VIDEO_I_FRAME_REQ, T_IPC_VIDEO_I_FRAME_RESP;

typedef struct{
    Uint32 sync;                //同步头
    Uint32 len;                 //整个包长度
    Uint32 frame_num;           //帧计数
    Uint32 frame_size;          //当前帧的大小
    Uint32 stream_id;           //第几路视频
    Uint32 packet_num;          //分包计数
    Uint32 packet_size;         //当前包frame_data的大小
    Uint32 packet_last;         //是否是一帧的最后一个包
    Uint32 frame_gop;           //gop 大小
    Uint32 frame_type;          //帧类型,1-I frmae, 0-P frame   
    Uint32 timestamp;           //时间戳
    Uint16 all_w;               //总宽度
    Uint16 all_h;               //总高度
    Uint16 valid_x;             //起始点：X位置
    Uint16 valid_y;             //起始点：Y位置
    Uint16 valid_w;             //图像有效宽度 ：X+W<= width_t
    Uint16 valid_h;             //图像有效高度 ：Y+h<= height_t
    Uint8  frame_data[8 * 1024];//视频有效数据

}HIPC_V_PACKET;


typedef struct{
	Uint32 sync;				//同步头
	Uint32 len;					//整个包长度
	Uint32 frame_num;           //帧计数
	Uint32 frame_size;          //当前帧的大小
	Uint32 stream_id;           //第几路音频
	Uint32 packet_num;          //分包计数
	Uint32 packet_size;         //当前包frame_data的大小
	Uint32 packet_last;         //是否是一帧的最后一个包
	Uint32 timestamp;           //时间戳
	Uint8  frame_data[2048];	//音频有效数据
}HIPC_A_PACKET;

//2015/05/29 add

typedef struct
{
    T_HIPC_NET_HEADER		head;
    Uint32					sig_chan;
    Uint8					streamID;
    Uint16					framerate;     //子码流的帧率
}T_HIPC_CMD_VIDEO_FRAME_RATE_REQ, T_HIPC_CMD_VIDEO_FRAME_RATE_RESP;


//};//namespace HIPC

#pragma pack(pop)

#endif
