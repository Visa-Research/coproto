
#include "AsioSocket.h"

namespace coproto
{
#ifdef COPROTO_ENABLE_BOOST
	namespace detail
	{
		optional<GlobalIOContext> global_asio_io_context;
		std::mutex global_asio_io_context_mutex;


	}
	//std::mutex ggMtx;
	//std::vector<std::string> ggLog;
#endif
}