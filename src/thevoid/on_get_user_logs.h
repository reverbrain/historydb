#ifndef HISTORY_SRC_THEVOID_ON_GET_USER_LOGS_H
#define HISTORY_SRC_THEVOID_ON_GET_USER_LOGS_H

#include <thevoid/server.hpp>

namespace history {

	struct on_get_user_logs : public ioremap::thevoid::simple_request_stream<webserver>
	{
		virtual void on_request(const ioremap::swarm::network_request &req, const boost::asio::const_buffer &buffer);
		virtual void on_close(const boost::system::error_code &err);
	};

} /* namespace history */

#endif //HISTORY_SRC_THEVOID_ON_GET_USER_LOGS_H
