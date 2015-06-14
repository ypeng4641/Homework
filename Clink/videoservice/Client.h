#pragma once

#include <WinSock2.h>
#include <x_util/uuid.h>
#include <x_types/intxx.h>

using namespace x_util;

class Client
{
public:
	Client(SOCKET s);
	~Client();

public:
	Uuid Id();
	SOCKET Socket();

public:
	u_int32 Mask();
	bool isLogin();
	void Login(u_int32 mask);
	void Logout();

private:
	Uuid	_id;
	SOCKET	_s;
	u_int32 _mask;
	bool _islogin;
};

