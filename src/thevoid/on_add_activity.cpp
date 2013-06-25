#include "on_add_activity.h"

namespace history {

void on_add_activity::on_request(const ioremap::swarm::network_request &/*req*/, const boost::asio::const_buffer &/*buffer*/)
{
	printf("AddActivity request\n");
}

void on_add_activity::on_close(const boost::system::error_code &/*err*/)
{
	printf("AddActivity close\n");
}

} /* namespace history */
