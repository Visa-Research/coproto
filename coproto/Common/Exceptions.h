#pragma once
#include <exception>
#include <string>
#include "Defines.h"
#include "error_code.h"

namespace coproto
{

	struct BadReceiveBufferSize : public std::system_error
	{
		u64 mBufferSize, mReceivedSize;

		BadReceiveBufferSize(u64 bufferSize, u64 receivedSize, std::string msg = {})
			: std::system_error(code::badBufferSize,
				std::string(
					"local buffer size:   " + std::to_string(bufferSize) +
					" bytes\ntransmitted size: " + std::to_string(receivedSize) +
					" bytes\n" + msg
				))
			, mBufferSize(bufferSize)
			, mReceivedSize(receivedSize)
		{}
	};


}