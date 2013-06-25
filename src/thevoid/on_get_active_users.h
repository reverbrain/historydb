#ifndef HISTORY_SRC_THEVOID_ON_GET_ACTIVE_USERS_H
#define HISTORY_SRC_THEVOID_ON_GET_ACTIVE_USERS_H

#include "webserver.h"

#include <set>

namespace history {

	struct on_get_active_users : public ioremap::thevoid::simple_request_stream<webserver>, public std::enable_shared_from_this<on_get_active_users>
	{
		virtual void on_request(const ioremap::swarm::network_request &req, const boost::asio::const_buffer &buffer);
		void on_finished(const std::set<std::string>& active_users);
		void on_send_finished(const std::string &);
		virtual void on_close(const boost::system::error_code &) {}
	};

} /* namespace history */

#endif //HISTORY_SRC_THEVOID_ON_GET_ACTIVE_USERS_H
