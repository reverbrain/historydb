#ifndef HISTORY_SRC_THEVOID_WEBSERVER_H
#define HISTORY_SRC_THEVOID_WEBSERVER_H

#include <thevoid/server.hpp>

namespace history {

class webserver : public ioremap::thevoid::server<webserver>
{
public:
	webserver();
	virtual void initialize();

	struct on_root : public ioremap::thevoid::simple_request_stream<webserver>
	{
		virtual void on_request(const ioremap::swarm::network_request &req, const boost::asio::const_buffer &buffer);
		virtual void on_close(const boost::system::error_code &err);
	};

private:
	class context;
	std::shared_ptr<context> context_;
};

} /* namespace history */

#endif //HISTORY_SRC_THEVOID_WEBSERVER_H
