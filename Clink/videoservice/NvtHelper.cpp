#include "NvtHelper.h"
using namespace _NVT;

/*
**主流程监视
*/
int _NVT::g_cntB[CNT_SIZE] = {0};
int _NVT::g_cntE[CNT_SIZE] = {0};


int _NVT::gCntB(int seq)
{
	if(seq >= CNT_SIZE)
	{
		return -1;
	}
	return ++g_cntB[seq];
}

int _NVT::gCntE(int seq)
{
	if(seq >= CNT_SIZE)
	{
		return -1;
	}
	return ++g_cntE[seq];
}

std::string _NVT::showCnt(void)
{
	std::string cntStr("Global cnt:");
	for(int i = 0; i < opt_CNT_END; i++)
	{
		char s[256] = {0};
		sprintf(s, "\n\tpos(%d):B,E(%d,%d)", i, g_cntB[i], g_cntE[i]);
		cntStr.append(s);
	}
	return cntStr;
}


/*
**打印输出设置
*/
int _NVT::g_dbgClass = _NVT::opt_NoneDbg;

bool _NVT::setDBG(OPT_DEBUG_CLASS opt)
{
	g_dbgClass |= opt;
	return true;
}

bool _NVT::unsetDBG(OPT_DEBUG_CLASS opt)
{
	g_dbgClass &= ~opt;
	return true;
}

std::string _NVT::showDBG(void)
{
	std::string dbgStr("DBG class:");
	if((g_dbgClass & opt_App) != 0)
	{
		dbgStr.append("\n\tApp");
	}
	if((g_dbgClass & opt_send_buf) != 0)
	{
		dbgStr.append("\n\tsend_buf");
	}
	if((g_dbgClass & opt_Plugin) != 0)
	{
		dbgStr.append("\n\tPlugin");
	}
	if((g_dbgClass & opt_Dispatch) != 0)
	{
		dbgStr.append("\n\tDispatch");
	}
	if((g_dbgClass & opt_TfVideoOutput) != 0)
	{
		dbgStr.append("\n\tTfVideoOutput");
	}

	return dbgStr;
}


/*
**码流输出监视
*/
std::set<unsigned long long> _NVT::g_OutputList;

int _NVT::DEC_IP(unsigned long long a)
{
	return (a>>32);
}

int _NVT::DEC_PORT(unsigned long long a)
{
	return a;
}

std::string _NVT::showOutput(void)
{
	std::string outputStr("Output list:");
	int cnt = 0;

	for(std::set<unsigned long long>::iterator it = g_OutputList.begin();
		it != g_OutputList.end();
		it++)
	{
		char str[256] = {0};
		sprintf(str, "\n\tOutput(%d): ip(%#x),port(%d)", ++cnt, DEC_IP(*it), DEC_PORT(*it));
		outputStr.append(str);
	}

	return outputStr;
}


/*
**插件监视
*/
std::map<std::string/*uuid*/, SignalInfo> _NVT::g_PluginList;

bool _NVT::gAddSig(Uuid sid, const char* name, int type, int ip, unsigned short port)
{
	SignalInfo si = {sid, std::string(name), type, ip, port};

	std::pair<std::map<std::string, SignalInfo>::iterator, bool> res;
	res = g_PluginList.insert(std::make_pair(sid.string(), si));
	if(res.second)
	{
		LOG(LEVEL_INFO, "id(%s), name(%s), type(%d), ip(%#x), port(%hu).", sid.string().c_str(), name, type, ip, port);
		return true;
	}

	LOG(LEVEL_WARNING, "Failed. id(%s), name(%s), type(%d), ip(%#x), port(%hu).", sid.string().c_str(), name, type, ip, port);
	return false;
}

bool _NVT::gDelSig(Uuid sid)
{
	if(NULL != gGetSI(sid))
	{
		g_PluginList.erase(sid.string());
		LOG(LEVEL_INFO, "id(%s).", sid.string().c_str());
		return true;
	}

	LOG(LEVEL_WARNING, "Failed. id(%s).", sid.string().c_str());
	return false;
}

bool _NVT::gAddOutput(Uuid sid, int ip, unsigned short port, char output, int index)
{
	SignalInfo* si = gGetSI(sid);
	if(si != NULL)
	{
		OutputInfo oi = {ip, port, output, index};
		si->output.push_back(oi);
		LOG(LEVEL_INFO, "id(%s), ip(%#x), port(%hu), output(%hd), index(%d).", sid.string().c_str(), ip, port, (short)output, index);
		return true;
	}

	LOG(LEVEL_WARNING, "Failed. id(%s), ip(%#x), port(%hu), output(%hd), index(%d).", sid.string().c_str(), ip, port, (short)output, index);
	return false;
}

bool _NVT::gDelOutput(Uuid sid, int ip, unsigned short port, char output, int index)
{
	SignalInfo* si = gGetSI(sid);
	if(si != NULL)
	{
		OutputInfo oi = {ip, port, output, index};

		for(std::map<std::string, SignalInfo>::iterator it = g_PluginList.begin();
			it != g_PluginList.end();
			it++)
		{
			for(std::vector<OutputInfo>::iterator oiIt = it->second.output.begin();
				oiIt != it->second.output.end();
				oiIt++)
			{
				if(*oiIt == oi)
				{
					it->second.output.erase(oiIt);
					LOG(LEVEL_INFO, "id(%s), ip(%#x), port(%hu), output(%hd), index(%d).", sid.string().c_str(), ip, port, (short)output, index);
					return true;
				}
			}
		}
	}

	LOG(LEVEL_WARNING, "Failed. id(%s), ip(%#x), port(%hu), output(%hd), index(%d).", sid.string().c_str(), ip, port, (short)output, index);
	return false;
}

bool _NVT::gMulticast(Uuid sid, int ip, unsigned short port)
{
	SignalInfo* si = gGetSI(sid);
	if(si != NULL)
	{
		si->multiIp = ip;
		si->multiPort = port;
		LOG(LEVEL_INFO, "id(%s), ip(%#x), port(%hu).", sid.string().c_str(), ip, port);
		return true;
	}

	LOG(LEVEL_WARNING, "Failed. id(%s), ip(%#x), port(%hu).", sid.string().c_str(), ip, port);
	return false;
}

bool _NVT::gUnicast(Uuid sid)
{
	SignalInfo* si = gGetSI(sid);
	if(si != NULL)
	{
		si->multiIp = 0;
		si->multiPort = 0;
		LOG(LEVEL_INFO, "id(%s).", sid.string().c_str());
		return true;
	}

	LOG(LEVEL_WARNING, "Failed. id(%s).", sid.string().c_str());
	return false;
}

SignalInfo* _NVT::gGetSI(Uuid sid)
{
	std::map<std::string, SignalInfo>::iterator it = g_PluginList.find(sid.string());
	if(it != g_PluginList.end())
	{
		return &(it->second);
	}

	return NULL;
}

std::string _NVT::showPlugin(void)
{
	int cnt = 0;
	std::string pluginStr("Plugin list:");
	for(std::map<std::string, SignalInfo>::iterator it = g_PluginList.begin();
		it != g_PluginList.end();
		it++)
	{
		char bs[256] = {0};
		sprintf(bs, "\n\n\tSignal *%d(%s, %#x:%hu), id(%s)\n\tMultiOutput(%#x:%hu)\n\tOutput:", ++cnt
			, it->second.name.c_str(), it->second.srcIp, it->second.srcPort, it->second.id.string().c_str()
			, it->second.multiIp, it->second.multiPort);

		int c = 0;
		std::string outputStr;
		for(std::vector<OutputInfo>::iterator oiIt = it->second.output.begin();
			oiIt != it->second.output.end();
			oiIt++)
		{
			char os[256] = {0};
			sprintf(os, "\n\t\t(%d)(%#x:%hu), output(%hd), index(%d)", ++c, oiIt->dstIp, oiIt->dstPort, (short)oiIt->output, oiIt->index);
			outputStr.append(os);
		}

		pluginStr.append(bs).append(outputStr);
	}

	return pluginStr;
}


/*
**telnet服务端助手
*/
bool _NVT::NvtHelper::run(void)
{
#if 0//def _WIN32
	
	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD(1,1);
	int err = WSAStartup(wVersionRequested, &wsaData);
	if(err != 0)
	{
		printf("WSAStartup failed! err(%d).\n", err);
	}
	if(LOBYTE(wsaData.wVersion) != 1 || HIBYTE(wsaData.wVersion) != 1)
	{
		WSACleanup();
		return false;
	}
#endif

	m_isRun = true;
	m_pRes = pthread_create(&m_pid, NULL, threadLoop, this);

	return true;
}

bool _NVT::NvtHelper::quit(void)
{
	m_isRun = false;
	if(0 == m_pRes)
	{
		pthread_join(m_pid, NULL);
	}

#if 0//def _WIN32
	WSACleanup();
#endif

	return true;
}


void* _NVT::NvtHelper::threadLoop(void* param)
{
	NvtHelper* nh = (NvtHelper*) param;

	int sock = -1;
	int pi = nh->m_port;
	for(;
		(pi < nh->m_port + 10) && (sock < 0);
		pi++)
	{
		struct sockaddr_in 	sin;

		memset(&sin,0,sizeof(sin));

		sin.sin_addr.s_addr     = INADDR_ANY;
		sin.sin_family          = AF_INET;
		sin.sin_port            = htons(pi);

		if((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
		{
			continue;
		}
		if(bind(sock, reinterpret_cast<struct sockaddr *>(&sin), sizeof(sin)) == SOCKET_ERROR )
		{
			closesocket(sock);
			sock = -1;
			continue;
		}

		listen(sock,128);
		LOG(LEVEL_INFO, "telnet port--%d", pi);
		printf("telnet port--%d", pi);
	}
	if(sock < 0)
	{
		LOG(LEVEL_ERROR, "tcp server make error!\n");
		return 0;
	}
	Network::DefaultStream(sock);

	int client_fd = -1;

TELNET_START:
	//等待telnet客户端
	//仅支持一个客户端同时在线
	while(nh->m_isRun)
	{
		fd_set fs;
		struct timeval tv = {0, 100000};
		FD_ZERO(&fs);
		FD_SET(sock, &fs);

		int nfds = Network::Select(FD_SETSIZE, &fs, NULL, NULL, &tv);
		if(nfds > 0)
		{
			if(FD_ISSET(sock, &fs))
			{
				struct sockaddr_in  saddr;
				client_fd = Network::MakeAccept(sock, reinterpret_cast<struct sockaddr *>(&saddr));
				break;
			}
		}
		else if(nfds < 0)
		{
			LOG(LEVEL_ERROR, "tcp server listen error!\n");
			Network::Close(sock);
			return 0;
		}
	}

	//客户端未连接
	if(client_fd <= 0)
	{
		if(nh->m_isRun)
		{
			goto TELNET_START;
		}
	}
	else
	{
		Nvt tln_sv(client_fd);
		//telnet对话开始
		while(nh->m_isRun)
		{
			fd_set fs;
			struct timeval tv = {0, 100000};

			FD_ZERO(&fs);
			FD_SET(client_fd, &fs);

			int nfds = Network::Select(FD_SETSIZE, &fs, NULL, NULL, &tv);
			if(nfds > 0)
			{
				//处理telnet命令
				if(! nh->processCmd(&tln_sv))
				{
					LOG(LEVEL_WARNING, "client quit!\n");
					Network::Close(client_fd);
					client_fd = -1;
					goto TELNET_START;
				}
			}
			else if(nfds < 0)
			{
				//客户端断线
				puts("client socket error!\n");
				Network::Close(client_fd);
				client_fd = -1;
				goto TELNET_START;
			}
		}

		//线程退出
		Network::Close(client_fd);
		client_fd = -1;
	}

	//线程退出
	Network::Close(sock);
	sock = -1;
	return (void*)0;
}


bool _NVT::NvtHelper::processCmd(Nvt* nvt)
{
	std::string cmd;
	if(! nvt->Read(cmd))
	{
		//客户端断线
		return false;
	}

	if(cmd.empty())
	{
		//telnet命令未完整
	}
	else if(0 == cmd.compare(CMD))
	{
		nvt->Println(std::string("cmd:\n\tShow available command list"));
		nvt->Println(std::string("global:\n\tShow global status"));
		nvt->Println(std::string("output:\n\tShow output status\n\t#!Warning:Don't operate VWAS when use this cmd!"));
		nvt->Println(std::string("plugin:\n\tShow plugin status\n\t#!Warning:Don't operate VWAS when use this cmd!"));
		nvt->Println(std::string("debug:\n\tShow debug classes"));
		nvt->Println(std::string("enable/disable -[CLASS]:\n\tTurn on/off [CLASS] debug output\n\tUse \"class\" to get valid options"));
		nvt->Println(std::string("class:\n\tShow valid option for the [CLASS]"));
		nvt->Println(std::string("quit:\n\tClose this session"));
	}
	else if(0 == cmd.compare(GLOBAL))
	{
		nvt->Println(showCnt());
	}
	else if(0 == cmd.compare(OUTPUT))
	{
		nvt->Println(showOutput());
	}
	else if(0 == cmd.compare(PLUGIN))
	{
		nvt->Println(showPlugin());
	}
	else if (0 == cmd.compare(DEBUG))
	{
		nvt->Println(showDBG());
	}
	else if(std::string::npos != cmd.rfind(ENABLE))
	{
		if(! dbgSwitch(true, cmd.substr(strlen(ENABLE))))
		{
			goto CMD_INVALID;
		}
		else
		{
			nvt->Println(showDBG());
		}
	}
	else if(std::string::npos != cmd.rfind(DISABLE))
	{
		if(! dbgSwitch(false, cmd.substr(strlen(DISABLE))))
		{
			goto CMD_INVALID;
		}
		else
		{
			nvt->Println(std::string(showDBG()));
		}
	}
	else if (0 == cmd.compare(CLASS))
	{
		nvt->Println(std::string("For option [CLASS]:")+std::string(OPT_OPTION));
	}
	else if(0 == cmd.compare(QUIT))
	{
		//客户端退出
		return false;
	}
	else
	{
CMD_INVALID:
		nvt->Println(std::string("command invalid!\nUse \"cmd\" to get usage note."));
	}

	return true;
}


bool _NVT::NvtHelper::dbgSwitch(bool on, std::string opt)
{
	bool (*dbgFunc)(OPT_DEBUG_CLASS);
	(on) ? (dbgFunc = setDBG) : (dbgFunc = unsetDBG);

	if(0 == opt.compare(OPT_ALL))
	{
		dbgFunc(opt_AllDbg);
	}

	//
	else if(0 == opt.compare(OPT_APP))
	{
		dbgFunc(opt_App);
	}
	else if(0 == opt.compare(OPT_SEND_BUF))
	{
		dbgFunc(opt_send_buf);
	}
	else if(0 == opt.compare(OPT_PLUGIN))
	{
		dbgFunc(opt_Plugin);
	}
	else if (0 == opt.compare(OPT_DISPATCH))
	{
		dbgFunc(opt_Dispatch);
	}
	else if(0 == opt.compare(OPT_VIDEO_OUTPUT))
	{
		dbgFunc(opt_TfVideoOutput);
	}
	else
	{
		return false;
	}

	return true;
}