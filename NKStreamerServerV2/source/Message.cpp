#include "Message.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <memory>
#include <fstream>

Message::Message()
{
	MessageCode = 0;
	ContentSize = 0;
	Received = 0;
}

int Message::ReadMessageFromData(const char* data, size_t size)
{
	if (Received == 0)
	{
		//---------------------------------------
		// start read header of message
		//---------------------------------------
		MessageCode = data[0];
		ContentSize = (uint32_t)data[1] |
			(uint32_t)data[2] << 8 |
			(uint32_t)data[3] << 16 |
			(uint32_t)data[4] << 24;
		//printf("[Recv] got new message code: %i size: %i.\n", MessageCode, ContentSize);
		Received += 5;
		//---------------------------------------
		if (Received == size)
		{
			return 0;
		}
		//---------------------------------------
		Content = (char*)malloc(sizeof(char) * ContentSize);
		if (ContentSize >= size)
		{
			memcpy(Content, data + 5, (size - 5));
			Received += size - 5;
			return ContentSize > size ? -1 : 0;
		}
		else
		{
			memcpy(Content, data + 5, (ContentSize - 5));
			Received += ContentSize - 5;
			return ContentSize;
		}
	}
	else
	{
		if (size + Received >= ContentSize)
		{
			int cutOffset = ContentSize - Received;
			memcpy(Content + (Received - 5), data, cutOffset);
			Received += cutOffset;
			if (size - cutOffset == 0) return 0;
			return cutOffset;
		}
		else
		{
			memcpy(Content + (Received - 5), data, size);
			Received += size;
		}
	}
	return -1;
}

//-----------------------
// input package helper
//-----------------------

char Message::GetFirstByte() const
{
	if (this->Content != nullptr && this->ContentSize > 5 && this->MessageCode != IMAGE_PACKET)
	{
		return this->Content[0];
	}
	return 0;
}

int Message::GetFirstInt() const
{
	if (this->Content != nullptr && this->ContentSize > 5 && this->MessageCode != IMAGE_PACKET)
	{
		int value = (uint32_t)this->Content[0] |
			(uint32_t)this->Content[1] << 8 |
			(uint32_t)this->Content[2] << 16 |
			(uint32_t)this->Content[3] << 24;
		return value;
	}
	return -1;
}

Message::~Message()
{
	if(this->Content != nullptr)
		free(this->Content);
}
