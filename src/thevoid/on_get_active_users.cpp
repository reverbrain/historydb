#include "on_get_active_users.h"

namespace history {

void on_get_active_users::on_request(const ioremap::swarm::network_request &/*req*/, const boost::asio::const_buffer &/*buffer*/)
{
	printf("GetActiveUsers request\n");
}

void on_get_active_users::on_close(const boost::system::error_code &/*err*/)
{
	printf("GetActiveUsers close\n");
}

} /* namespace history */
