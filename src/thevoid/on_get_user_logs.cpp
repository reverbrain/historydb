#include "on_get_user_logs.h"

namespace history {

void on_get_user_logs::on_request(const ioremap::swarm::network_request &/*req*/, const boost::asio::const_buffer &/*buffer*/)
{
	printf("GetUserLogs request\n");
}

void on_get_user_logs::on_close(const boost::system::error_code &/*err*/)
{
	printf("GetUserLogs close\n");
}

} /* namespace history */
