#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <list>
/*提供AAC-ADTS文件的访问接口*/
using namespace std;

class AudioFile{
	public:
		/*strFileName [in]: 音频文件的路径名字符串
              * boolRepeat [in]: 当遇到文件尾时是否从问价文件开始位置重复获取
		*/
		AudioFile(char * strFileName, bool boolRepeat);
		/*获取下一帧音频流数据
		* 参数说明:buf [in]: 帧音频数据的缓冲区[预先固定分配,要保证其长度大于任意一帧数据]
		*                        dataLen [in]:预先分配的帧缓冲区长度
		*                        dataLen [out]: 读到的长度 
	       *  返回: 0:成功；1: 未获取到帧数据,但已获取到文件尾；-1: 失败
		*/
		int GetNextFrame(char * buf, int & dataLen);

	private:
		/*获取音频采样率
		* 参数说明: sampleRate [out]:采样率
		*           bitsPerSample [out]: 每采样占用bit数
		*返回: 0: 成功；-1:失败
		*/
		int GetSampleInfo(int & sampleRate, int & bitsPerSample);
		/*寻找下一个section的开始(0x00000001为标记)
		* 返回: >=0: 标记开始位置在文件的位置
		*              <0: 已找到文件尾，仍未能找到
		* 注:调用该函数期间，不要修改文件读写位置
		*/
		int SeekNextSectionFlag(void);
		/*判断是否是视频帧数据段
         * 算法: int type = flag & 0x1f，若type为5，则为I帧；若type为1，则为B/P帧
		*/
		bool IsFrameSection(unsigned char flag);
		/*生成帧位置索引*/
		int GenerateFramePosIndex(void);

	private:
		bool				m_boolRepeat;	//当遇到文件尾时是否从问价文件开始位置重复获取
		FILE	*			m_audioFile;	
		list<int>			m_framePosIndex; //帧位置索引。记录每一帧在文件中的起始位置
		list<int>::iterator	m_it;	//当前m_framePosIndex的迭代器
};


