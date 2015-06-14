#include <stdlib.h>
#include "audiofile.h"
/*提供AAC-ADTS文件的访问接口*/

/*strFileName [in]: 音频文件的路径名字符串
 * boolRepeat [in]: 当遇到文件尾时是否从问价文件开始位置重复获取
*/
AudioFile::AudioFile(char * strFileName, bool boolRepeat)
{
	m_boolRepeat = boolRepeat;
	m_audioFile = fopen(strFileName, "rb");
	if(NULL == m_audioFile)
	{
		printf("========>[APP] Error! In AudioFile::AudioFile(): fopen() failed at %s:%d\n", __FILE__, __LINE__);
	}
	GenerateFramePosIndex();
	m_it = m_framePosIndex.begin();
	
	return;
}

/*获取音频采样率
* 参数说明: sampleRate [out]:采样率(HZ)
*           bitsPerSample [out]: 每采样占用bit数
*返回: 0: 成功；-1:失败
*/
int AudioFile::GetSampleInfo(int & sampleRate, int & bitsPerSample)
{
	sampleRate = 44100;
	bitsPerSample = 8;
	return 0;
}

/*获取下一帧音频流数据
* 参数说明:buf [in]: 帧音频数据的缓冲区[预先固定分配,要保证其长度大于任意一帧数据]
*                        dataLen [in]:预先分配的帧缓冲区长度
*                        dataLen [out]: 读到的长度 
   *  返回: 0:成功；1: 未获取到帧数据,但已获取到文件尾，需再次尝试读取；-1: 失败
*/
int AudioFile::GetNextFrame(char * buf, int & dataLen)
{
	//MyTime	myTime((char *)"AudioFile::GetNextFrame()");
	int ret = -1;
	int curPos = *m_it;
	m_it++;
	if(m_it == m_framePosIndex.end()) //已取到最后一帧
	{
		if(m_boolRepeat)
		{
			m_it = m_framePosIndex.begin();
			return 1;
		}
		return -1;
	}

	int nextPos = *m_it;

	fseek(m_audioFile, curPos, SEEK_SET);
	int len = nextPos - curPos;
	if(len > dataLen)
	{
		printf("========>[APP] Error! In AudioFile::GetNextFrame(): frameBuf size(%d) is too small at %s:%d\n", dataLen,  __FILE__, __LINE__);
		return -1;
	}
	ret = fread(buf, 1, len, m_audioFile);
	if(ret < 0)
	{
		printf("========>[APP] Error! In AudioFile::GetNextFrame(): fread() failed at %s:%d\n", __FILE__, __LINE__);
		return -1;
	}
	else
	{
		dataLen = len;
		//printf(">>>>>>>>[APP]: In AudioFile::GetNextFrame(): buf[0~4] = [0x%x, 0x%x, 0x%x, 0x%x, 0x%x], dataLen = %d\n", buf[0], buf[1], buf[2], buf[3], buf[4], dataLen);
		return 0;
	}

	return -1;	
}

/*寻找下一个section的开始(0xFFF91080为标记)[相距文件头的字节数偏移]
* 返回: >=0: 标记开始位置在文件的位置
*              <0: 已找到文件尾，仍未能找到
* 注:调用该函数期间，不要修改文件读写位置
*/
int AudioFile::SeekNextSectionFlag(void)
{
	if(NULL == m_audioFile)
	{
		printf("========>[APP]: Warning! In AudioFile::SeekNextSectionFlag(): AudioFile is NULL!\n");
		return -1;
	}

	/*寻找方法: 每次向后偏移1个字节，读取4个字节，若得到0xFFFX，则说明找到了标记*/
	#define  FLAG_SIZE  4
	//int flag = 0x00000001;
	char content[FLAG_SIZE+1] = {0};
	int ret = -1;
	
	do{
		ret = fread(content, 1, FLAG_SIZE, m_audioFile);
		if(FLAG_SIZE != ret)
		{
			return -1;
		}
		
		//if( (0xFF == content[0]) &&(0xF9 == content[1]) && (0x10 == content[2]) && (0x80 == content[3]) )
		//if((0xFF == content[0]) && ( (content[1] & 0xF0) == 0xF0))
		if( (0xFF == content[0]) &&(0xF1 == content[1]) && (0x50 == content[2]) && (0x80 == content[3]) )
		{
			return (ftell(m_audioFile) - FLAG_SIZE);
		}
		else
		{
			fseek(m_audioFile, -(FLAG_SIZE-1), SEEK_CUR); //逐字节查找 
		}
	}while(0 < ret);

	return -1;
}

/*判断是否是视频帧数据段
*/
bool AudioFile::IsFrameSection(unsigned char flag)
{
	return false;
}

/*生成帧位置索引*/
int AudioFile::GenerateFramePosIndex(void)
{
	int pos = -1;
	do{
		pos = SeekNextSectionFlag();
		if(0 <= pos)
		{
			//printf("========>[APP]: In AudioFile::GenerateFramePosIndex(): m_framePosIndex.push_back(%d)...\n", pos);
			m_framePosIndex.push_back(pos);
		}
	}while(0 <= pos);
	fseek(m_audioFile, 0, SEEK_SET); //文件读写指针跳回到文件起始处
	
	return 0;
}


