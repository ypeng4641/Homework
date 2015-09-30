/*
 * =====================================================================================
 * 
 *       Filename:  Nvt.h
 * 
 *    Description:               
 * 
 *        Version:  1.0
 *        Created:  2008年06月24日 10时29分31秒 CST
 *       Revision:  none
 *       Compiler:  gcc
 * 
 *         Author:  aitecx@gmail.com
 *        Company:  
 * 
 * =====================================================================================
 */
#include "network.h"
#include <string>

#ifndef  NVT_INC
#define  NVT_INC

class Nvt
{
public:
	Nvt(int sock)
	{
		m_sock = sock;
	}
public:
	int		GetSock()
	{
		return m_sock;
	}
public:
	void	Println(std::string str)
	{ 
		Write(str);
		Write("\n"); 
	}

	void 	Print(std::string str)
	{ 
		Write(str); 
	}
public:
	bool	Read(std::string& str)
	{
		char ch = 0;
		if(Network::Readn(m_sock,&ch,sizeof(ch)) == sizeof(ch))
		{ 
			m_chs.push_back(ch);
		} 
		else
		{
			return false;
		}

		/* 提取命令串，以"\r\n"分隔 */ 
		unsigned int loc = m_chs.find("\r\n");
		if(loc != std::string::npos)
		{
			str = m_chs.substr(0,loc);
			m_chs = m_chs.substr(loc+2);
		}
		else
		{
			str.clear();// = std::string();
		}
		return true; 
	}
	bool	Write(const std::string& str)
	{ 
		char buf[] = {'\r','\n'}; 

		for(unsigned int i=0;i<str.length();i++)
		{
			char ch = str[i]; 
			if(str[i] == '\n')
			{ 
				Network::Writen(m_sock,buf,sizeof(buf)); 
			}
			else
			{
				Network::Writen(m_sock,&ch,sizeof(ch));
			}
		}
		return true;
	}
private:
	std::string		m_chs;
	int			m_sock;
};

#endif   /* ----- #ifndef NVT_INC  ----- */

