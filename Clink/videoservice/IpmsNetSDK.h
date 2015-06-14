#ifndef	__IPMS_NET_SDK_H__
#define	__IPMS_NET_SDK_H__


#include <stdlib.h>

#ifdef __cplusplus
#define	IPMS_EXTERN_C	extern "C"
#else
#define	IPMS_EXTERN_C
#endif


#ifdef WIN32	/* win32 */
	#ifdef IPMS_NET_SDK_EXPORTS
		#define IPMS_API IPMS_EXTERN_C __declspec(dllexport)
	#elif defined IPMS_NET_SDK_TEST
		#define IPMS_API
	#else
		#define IPMS_API IPMS_EXTERN_C __declspec(dllimport)
	#endif 
#else	/* linux */
	#define IPMS_API IPMS_EXTERN_C
	#define	__stdcall
#endif


// 码流接收协议
#define	IPMS_NET_TCP							1
#define	IPMS_NET_UDP							2
#define	IPMS_NET_MULTICAST						3


// 请求类型
#define	IPMS_PREVIEW							0
#define	IPMS_REPLAY								1
#define	IPMS_DOWNLOAD							2


// 错误码定义

#define	IPMS_ERROR_SUCCESS							0			/*<	操作成功						*/
#define	IPMS_ERROR_DOWNLOAD_COMPLETE				1			/*< 下载完成						*/
#define	IPMS_ERROR_COMMON_FAILURE					-97000		/*< 操作失败						*/
#define	IPMS_ERROR_COMMON_INVALID_USER_HANDLE		-97001		/*< 用户句柄无效					*/
#define	IPMS_ERROR_COMMON_INVALID_USERNAME			-97002		/*< 用户名无效(指针为空或太长)		*/
#define	IPMS_ERROR_COMMON_INVALID_PASSWORD			-97003		/*< 密码无效(指针为空或太长)		*/
#define	IPMS_ERROR_COMMON_INVALID_TRANS_TYPE		-97004		/*< 无效的码流传输类型				*/
#define	IPMS_ERROR_COMMON_INVALID_CHANNEL			-97005		/*< 通道编号无效					*/
#define	IPMS_ERROR_COMMON_INVALID_TYPE				-97006		/*< 类型无效						*/
#define	IPMS_ERROR_COMMON_MEMORY_ALLOC_FAILED		-97007		/*< 内存分配失败					*/
#define	IPMS_ERROR_COMMON_NO_SIGNAL_IN_REGION		-97008		/*< 在指定的区域范围内没涉及任何信号	*/


// 与管理节点交互时出现的错误
#define	IPMS_ERROR_MNODE_INVALID_IPADDR				-97200		/*< 管理节点的IP地址无效			*/
#define	IPMS_ERROR_MNODE_INVALID_PORT				-97201		/*< 管理节点的端口号无效			*/
#define	IPMS_ERROR_MNODE_INVALID_RESPONSE			-97202		/*< 管理节点返回了错误的响应包		*/
#define	IPMS_ERROR_MNODE_INVALID_CHANNEL_DESC		-97203		/*< 无效的通道描述信息				*/
#define	IPMS_ERROR_MNODE_CONNECT_FAILED				-97204		/*< 连接管理服务器失败				*/
#define	IPMS_ERROR_MNODE_DISCONNECTD				-97205		/*< 与管理服务器的连接已断开		*/
#define	IPMS_ERROR_MNODE_NOT_LOGIN					-97206		/*< 因尚未登录,导致无权限操作		*/
#define	IPMS_ERROR_MNODE_EMPTY_CHANNEL				-97207		/*< 通道中的信号源数量为0			*/
#define	IPMS_ERROR_MNODE_GET_SESSION_FAILED			-97208		/*< 获取会话ID失败				*/

// 与转码节点交互时出现的错误
#define	IPMS_ERROR_TNODE_INVALID_IPADDR				-97300		/*< 转码节点的IP地址无效			*/
#define	IPMS_ERROR_TNODE_INVALID_PORT				-97301		/*< 转码节点的端口号无效			*/
#define	IPMS_ERROR_TNODE_INVALID_RESPONSE			-97302		/*< 转码节点返回了错误的响应包		*/
#define	IPMS_ERROR_TNODE_INVALID_STREAM_DESC		-97303		/*< 转码节点返回的信号分块信号无效	*/
#define	IPMS_ERROR_TNODE_INVALID_TRANS_TYPE			-97304		/*< [内部错误]无效的码流传输类型	*/
#define	IPMS_ERROR_TNODE_INVALID_PACKET_LEN			-97305		/*< 无效的数据包长度(>50M)			*/
#define	IPMS_ERROR_TNODE_INVALID_UDP_PACKET			-97306		/*< 转码节点发来的UDP数据包无效		*/
#define	IPMS_ERROR_TNODE_CONNECT_FAILED				-97307		/*< 连接转码节点失败				*/
#define	IPMS_ERROR_TNODE_DISCONNECTD				-97308		/*< 与转码节点的连接已断开			*/
#define	IPMS_ERROR_TNODE_ALREADY_RUNNING			-97309		/*< [内部错误]重复接收转码节点数据	*/
#define	IPMS_ERROR_TNODE_UNKNOWN_RESPONSE			-97310		/*< [内部错误]未知的转码节点响应包	*/
#define	IPMS_ERROR_TNODE_GET_LOCAL_ADDR_FAILED		-97311		/*< 获取本机地址失败				*/
#define	IPMS_ERROR_TNODE_ALLOC_UDP_PORT_FAILED		-97312		/*< 分配UDP服务端口失败			*/
#define	IPMS_ERROR_TNODE_SIG_NOT_SUPPORT_MULTICAST	-97313		/*< 当前信号不支持多播传输			*/
#define	IPMS_ERROR_TNODE_MULTICAST_PORT_OCCUPIED	-97314		/*< 多播端口被占用, 无法接收数据	*/
#define	IPMS_ERROR_TNODE_JOIN_MULTICAST_FAILED		-97315		/*< 加入多播组失败				*/
#define	IPMS_ERROR_TNODE_RECV_FRAME_FAILED			-97316		/*< 读取帧数据失败				*/
#define	IPMS_ERROR_TNODE_REQUEST_STREAM_FAILED		-97317		/*< 请求码流失败。				*/
#define	IPMS_ERROR_TNODE_MEDIA_FORMAT_UNKNOWN		-97318		/*< 未知道的数据格式				*/
#define	IPMS_ERROR_TNODE_PROGRAM_ERROR				-97319		/*< 程序内部错误					*/
#define	IPMS_ERROR_TNODE_RECV_UDP_STREAM_FAILED		-97320		/*< 读UDP数据失败					*/
#define	IPMS_ERROR_TNODE_MAKE_UDP_SERVER_FAILED		-97321		/*< 创建UDP监听服务失败			*/


// 与存储节点交互时出现的错误
#define	IPMS_ERROR_SNODE_INVALID_IPADDR				-97400		/*< 存储节点的IP地址无效			*/
#define	IPMS_ERROR_SNODE_INVALID_PORT				-97401		/*< 存储节点的端口号无效			*/
#define	IPMS_ERROR_SNODE_INVALID_RESPONSE			-97402		/*< 存储节点返回了错误的响应包		*/
#define	IPMS_ERROR_SNODE_INVALID_STREAM_DESC		-97403		/*< 存储节点返回的信号分块信号无效	*/
#define	IPMS_ERROR_SNODE_INVALID_TRANS_TYPE			-97404		/*< [内部错误]无效的码流传输类型	*/
#define	IPMS_ERROR_SNODE_INVALID_PACKET_LEN			-97405		/*< 无效的数据包长度(>50M)			*/
#define	IPMS_ERROR_SNODE_CONNECT_FAILED				-97407		/*< 连接存储节点失败				*/
#define	IPMS_ERROR_SNODE_DISCONNECTD				-97408		/*< 与存储节点的连接已断开			*/
#define	IPMS_ERROR_SNODE_ALREADY_RUNNING			-97409		/*< [内部错误]重复接收存储节点数据	*/
#define	IPMS_ERROR_SNODE_UNKNOWN_RESPONSE			-97410		/*< [内部错误]未知的存储节点响应包	*/
#define	IPMS_ERROR_SNODE_GET_LOCAL_ADDR_FAILED		-97411		/*< 获取本机地址失败				*/
#define	IPMS_ERROR_SNODE_RECV_FRAME_FAILED			-97416		/*< 读取帧数据失败				*/
#define	IPMS_ERROR_SNODE_REQUEST_STREAM_FAILED		-97417		/*< 请求码流失败。				*/
#define	IPMS_ERROR_SNODE_MEDIA_FORMAT_UNKNOWN		-97418		/*< 未知道的数据格式				*/
#define	IPMS_ERROR_SNODE_PROGRAM_ERROR				-97419		/*< 程序内部错误					*/



#define	IPMS_ERROR_SDK_MIN_ERROR_CODE				-97000		/*< 本SDK最小的错误码值			*/
#define	IPMS_ERROR_SDK_MAX_ERROR_CODE				-99000		/*< 本SDK最大的错误码值			*/


#pragma pack(push)
#pragma pack(1)

/** 通道编号或码流编号(与uuid_t的内存结构一致) */
typedef	struct	_IPMS_UUID_t
{
	unsigned char	data[16];
}IPMS_UUID_t;


/** 归一化区域 */
typedef	struct	_IPMS_ZONE_t
{
	double			x;		// 须保证x <= 1 && x >= 0
	double			y;		// 须保证y <= 1 && y >= 0
	double			w;		// 须保证 w >= 0 && x + w <= 1
	double			h;		// 须保证 h >= 0 && y + h <= 1
}IPMS_ZONE_t;


/** 码流类型 */
typedef	enum _IPMS_MEDIA_FMT_t
{
	IPMS_MEDIA_FMT_UNKNOWN	= 0,
	IPMS_MEDIA_FMT_YV12		= 0x32315659,
	IPMS_MEDIA_FMT_I420		= 0x30323449,
	IPMS_MEDIA_FMT_H264		= 0x34363248,
	IPMS_MEDIA_FMT_MPEG4	= 0x2034504D,
	IPMS_MEDIA_FMT_G726		= 0x36323747,
	IPMS_MEDIA_FMT_AAC		= 0x20434141,
	IPMS_MEDIA_FMT_OGG		= 0x2047474F
}IPMS_MEDIA_FMT_t;


/** 帧类型 */
typedef	enum _IPMS_FRAME_TYPE_t
{
	IPMS_FRAME_NONE			= 0,
	IPMS_FRAME_IFRAME		= 100,
	IPMS_FRAME_PFRAME		= 200,
	IPMS_FRAME_BFRAME		= 300
}IPMS_FRAME_TYPE_t;


typedef	struct	_IPMS_VIDEO_t
{
	IPMS_FRAME_TYPE_t		frame_type;				// 帧类型
	IPMS_ZONE_t				region;					// [视频参数]图像块在通道上的位置和大小, 最大[0.0, 0.0, 1.0, 1.0]
	IPMS_ZONE_t				clip;					// [视频参数]图像块的有效的像素区域(用于去杂边), 最大[0.0, 0.0, 1.0, 1.0]
	int						width;					// 图像宽
	int						height;					// 图像高
}IPMS_VIDEO_t;


typedef	struct	_IPMS_AUDIO_t
{
	int						sample_rate;			// [音频参数]采样率
	int						channels;				// [音频参数]声道数
	int						bits_per_sample;		// [音频参数]单样本位数
}IPMS_AUDIO_t;



/** 帧信息 */
typedef	struct	_IPMS_MEDIA_FRAME_t
{
	int						stream_id;				// 流编号

	IPMS_MEDIA_FMT_t		media_fmt;				// 数据格式
	int						is_video;				// 是否为视频帧

	unsigned long long		timestamp;				// 时间戳（以毫秒为单位）
	unsigned long long		position;				// 回放位置(回放时有效)

	IPMS_VIDEO_t			video;					// 图像帧信息(is_video非0时有效)
	IPMS_AUDIO_t			audio;					// 音频帧信息(is_video为0时有效)

	unsigned char*			frame_data;				// 帧数据首指针
	unsigned int			frame_size;				// 帧数据长度
}IPMS_FRAME_t;


// 流信息
typedef	struct	_IPMS_STREAM_t
{
	int						stream_id;				// 流编号

	IPMS_MEDIA_FMT_t		format;					// 数据格式
	int						is_video;				// 是否为视频帧

	IPMS_VIDEO_t			video;					// 图像帧信息(is_video非0时有效)
	IPMS_AUDIO_t			audio;					// 音频帧信息(is_video为0时有效)
}IPMS_STREAM_t;


typedef	struct	_IPMS_REPLAY_PERIOD_t
{
	unsigned long long		begin_time;
	unsigned long long		end_time;
}IPMS_REPLAY_PERIOD_t;


typedef	struct {int unused;}*	IPMS_USER_t;

typedef void (__stdcall *IPMS_FRAME_CB_t)(IPMS_FRAME_t* frame, void* context);
typedef	void (__stdcall *IPMS_ERROR_CB_t)(IPMS_UUID_t* channel, int error, const char* description, const char* addr, unsigned short port, void* context);
typedef	void (__stdcall *IPMS_STREAM_LISTENER_t)(IPMS_UUID_t* id /*preview.channel | replay.stream*/, int stream_num, IPMS_STREAM_t streams[], void* context);




IPMS_API	IPMS_USER_t	__stdcall	IPMS_NET_Login				(const char* addr, unsigned short port, const char* user, const char* pwd, int timeout = -1, int* error = NULL);
IPMS_API	IPMS_USER_t	__stdcall	IPMS_NET_Login2				(const char* addr, unsigned short port, long long session, int timeout = -1, int* error = NULL);
IPMS_API	int			__stdcall	IPMS_NET_Logout				(IPMS_USER_t user);

IPMS_API	int			__stdcall	IPMS_NET_SetFrameCB			(IPMS_USER_t user, IPMS_FRAME_CB_t frame_cb, void* context);
IPMS_API	int			__stdcall	IPMS_NET_SetErrorCB			(IPMS_USER_t user, IPMS_ERROR_CB_t error_cb, void* context);

IPMS_API	int			__stdcall	IPMS_NET_SetStreamListener	(IPMS_USER_t user, IPMS_STREAM_LISTENER_t listener, void* context);

IPMS_API	int			__stdcall	IPMS_NET_OpenStream			(IPMS_USER_t user, IPMS_UUID_t* channel, int protocol, int type = IPMS_PREVIEW, IPMS_REPLAY_PERIOD_t* period = NULL);
IPMS_API	int			__stdcall	IPMS_NET_OpenStream2		(IPMS_USER_t user, IPMS_UUID_t* channel, int protocol, IPMS_ZONE_t* region, int type = IPMS_PREVIEW, IPMS_REPLAY_PERIOD_t* period = NULL, IPMS_UUID_t* stream = NULL);
IPMS_API	int			__stdcall	IPMS_NET_CloseStream		(IPMS_USER_t user);


#pragma pack(pop)

#endif	// __IPMS_NET_SDK_H__