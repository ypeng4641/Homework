#include "Client.h"


Client::Client(SOCKET s): _s(s), _mask(0), _islogin(false)
{
	_id.Generate();
}


Client::~Client()
{
}

Uuid Client::Id()
{
	return _id;
}

SOCKET Client::Socket()
{
	return _s;
}

bool Client::isLogin()
{
	return _islogin;
}
void Client::Login(u_int32 mask)
{
	_islogin = true;
	_mask = mask;
}
void Client::Logout()
{
	_islogin = false;
	_mask = 0;
}

u_int32 Client::Mask()
{
	return _mask;
}
