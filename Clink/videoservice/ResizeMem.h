#ifndef _RESIZE_MEM_H_
#define _RESIZE_MEM_H_



class ResizeMem
{
public:
	ResizeMem():mem_(NULL),max_(0),size_(0)
	{
	}
	void copy(char* data, int size)
	{
		if(max_ < size)
		{
			if(mem_) delete[] mem_;
			mem_ = new char[size];
			max_ = size;
		}

		memcpy(mem_, data, size);
		size_ = size;
	}
	void merg(char*data1, char* data2, int size1, int size2)
	{
		if(max_ < (size1+size2))
		{
			if(mem_) delete[] mem_;
			mem_ = new char[size1+size2];
			max_ = size1+size2;
		}

		memcpy(mem_,       data1, size1);
		memcpy(mem_+size1, data2, size2);
		size_ = size1+size2;
	}
	char* mem()
	{
		return mem_;
	}
	int  size()
	{
		return size_;
	}
private:
	char* mem_;
	int   max_;
	int   size_;
};


#endif