#include "on_add_log.h"

namespace history {

void on_add_log::on_request(const ioremap::swarm::network_request &/*req*/, const boost::asio::const_buffer &/*buffer*/)
{
	printf("AddLog request\n");
}

void on_add_log::on_close(const boost::system::error_code &/*err*/)
{
	printf("AddLog close\n");
}

} /* namespace history */
