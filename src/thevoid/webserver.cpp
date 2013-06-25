#include "webserver.h"

#include <boost/bind.hpp>

#include "on_add_log.h"
#include "on_add_activity.h"
#include "on_get_acive_users.h"
#include "on_get_user_logs.h"

namespace history {

class webserver::context
{};

webserver::webserver()
{}

void webserver::initialize()
{
	on<on_root>("/");
	on<on_add_log>("/add_log");
	on<on_add_activity>("/add_activity");
	on<on_get_active_users>("/get_active_users");
	on<on_get_user_logs>("/get_user_logs");
}

void webserver::on_root::on_request(const ioremap::swarm::network_request &/*req*/, const boost::asio::const_buffer &/*buffer*/)
{
	printf("Root request\n");

	auto conn = get_reply();

	ioremap::swarm::network_reply reply;
	reply.set_code(ioremap::swarm::network_reply::ok);
	reply.set_content_length(0);
	reply.set_content_type("text/html");

	conn->send_headers(reply, boost::asio::buffer(""), std::bind(&ioremap::thevoid::reply_stream::close, conn, std::placeholders::_1));
}

void webserver::on_root::on_close(const boost::system::error_code &/*err*/)
{
	printf("Root close\n");
}

} /* namespace history */

int main(int, char**)
{
	auto server = ioremap::thevoid::create_server<history::webserver>();

	server->listen("unix:/var/run/fastcgi2/historydb.sock");

	server->run();

	return 0;
}
