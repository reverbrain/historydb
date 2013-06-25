#ifndef HISTORY_SRC_THEVOID_ON_ADD_ACTIVITY_H
#define HISTORY_SRC_THEVOID_ON_ADD_ACTIVITY_H

#include "webserver.h"

namespace history {

	struct on_add_activity : public ioremap::thevoid::simple_request_stream<webserver>, public std::enable_shared_from_this<on_add_activity>
	{
		virtual void on_request(const ioremap::swarm::network_request &req, const boost::asio::const_buffer &buffer);
		void on_finished(bool added);
		virtual void on_close(const boost::system::error_code &err);
	};

} /* namespace history */

#endif //HISTORY_SRC_THEVOID_ON_ADD_ACTIVITY_H
