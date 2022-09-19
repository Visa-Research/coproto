

#include "coproto/Common/error_code.h"


namespace { // anonymous namespace

	struct coprotoCodeCategory : std::error_category
	{
		const char* name() const noexcept override
		{
			return "coproto_error_code";
		}

		std::string message(int err) const override
		{
			switch (static_cast<coproto::code>(err))
			{
			case coproto::code::success:
				return "coproto::code::success: The io operation completed successfully";
			case coproto::code::suspend:
				return "coproto::code::suspend: The protocol has been suspended for some reason.";

			case coproto::code::cancel:
				return "coproto::code::cancel: The operation has been canceled by the local party.";
			//case coproto::code::remoteCancel:
			//	return "coproto::code::remoteCancel: The operation has been cancels by the remote party.";
			case coproto::code::closed:
				return "coproto::code::closed: The connection has been closed by the local party.";
			case coproto::code::remoteClosed:
				return "coproto::code::remoteClosed: The connection has been closed by the remote party.";
			case coproto::code::operation_aborted:
				return "coproto::code::operation_aborted: The operation has been canceled by the local party.";
			case coproto::code::ioError:
				return "coproto::code::ioError: The io operation failed and the caller should abort";
			case coproto::code::badBufferSize:
				return "coproto::code::badBufferSize: Bad buffer size. The reciever's buffer size does not match the number of bytes sent.";
			case coproto::code::sendLengthZeroMsg:
				return "coproto::code::sendLengthZeroMsg: Sending a zero length message is not allowed.";
			case coproto::code::noAsyncSupport:
				return "coproto::code::noAsyncSupport: The requested operation for not support async sockets.";
			case coproto::code::uncaughtException:
				return "coproto::code::uncaughtException: An uncaught exception was thrown during the protocol. It can be obtained by calling getException() on the protocol object.";
			case coproto::code::invalidArguments:
				return "coproto::code::invalidArguments: The function was called with invalid arguments";
			case coproto::code::uninitialized:
				return "coproto::code::uninitialized: result was uninitialized.";
			case coproto::code::badCoprotoMessageHeader:
				return "coproto::code::badCoprotoMessageHeader: Received a corrupted/invalid protocol message header";
			case coproto::code::DEBUG_ERROR:
				return "coproto::code::DEBUG_ERROR: an artificial error used for debugging.";
			default:
				return "unknown error_code of type coproto::code";
			}
		}
	};
	const coprotoCodeCategory theCoprotoCodeCategory{};
} // anonymous namespace

namespace coproto
{
	error_code make_error_code(code e)
	{
		auto ee = static_cast<int>(e);
		return { ee, theCoprotoCodeCategory };
	}
}