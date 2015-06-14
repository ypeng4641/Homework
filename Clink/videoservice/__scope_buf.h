#pragma once


class __scope_buf
{
public:
	__scope_buf(unsigned int len): _len(len)
	{
		_buf = new char[_len];
	}

	~__scope_buf()
	{
		if(_buf)
		{
			delete[] _buf;
		}
	}

public:
	unsigned int len()
	{
		return _len;
	}

	char* buf()
	{
		return _buf;
	}

private:
	unsigned int _len;
	char* _buf;

};