#ifndef HISTORY_SRC_THEVOID_ON_GET_USER_LOGS_H
#define HISTORY_SRC_THEVOID_ON_GET_USER_LOGS_H

#include "webserver.h"

namespace history {

	struct on_get_user_logs :
		public ioremap::thevoid::simple_request_stream<webserver>,
		public std::enable_shared_from_this<on_get_user_logs>
	{
		virtual void on_request(const ioremap::swarm::network_request &req,
		                        const boost::asio::const_buffer &buffer);
		bool on_finished(const std::vector<char>& data);
		void on_send_finished(const std::string &);
		virtual void on_close(const boost::system::error_code &) {}
	};

} /* namespace history */

#endif //HISTORY_SRC_THEVOID_ON_GET_USER_LOGS_H
