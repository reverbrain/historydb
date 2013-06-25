#include "on_add_activity.h"

#include <swarm/network_url.h>
#include <swarm/network_query_list.h>

#include <historydb/provider.h>
#include <elliptics/error.hpp>

#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>

namespace history {

void on_add_activity::on_request(const ioremap::swarm::network_request &req, const boost::asio::const_buffer &/*buffer*/)
{
	printf("AddActivity request\n");

	try {
		ioremap::swarm::network_url url(req.get_url());
		ioremap::swarm::network_query_list query_list(url.query());

		if (!query_list.has_item("user"))
			throw std::invalid_argument("user is missed");

		auto user = query_list.item_value("user");

		auto provider = get_server()->get_provider();

		if(query_list.has_item("key")) {
			auto key = query_list.item_value("key");

			provider->add_activity(user,
			                       key,
			                       std::bind(&on_add_activity::on_finished,
			                                 shared_from_this(),
			                                 std::placeholders::_1));
		}
		else if(query_list.has_item("time")) {
			auto time = boost::lexical_cast<uint64_t>(query_list.item_value("time"));
			provider->add_activity(user,
			                       time,
			                       std::bind(&on_add_activity::on_finished,
			                                 shared_from_this(),
			                                 std::placeholders::_1));
		}
		else {
			throw std::invalid_argument("key and time are missed");
		}
	}
	catch(ioremap::elliptics::error& e) {
		get_reply()->send_error(ioremap::swarm::network_reply::internal_server_error);
	}
	catch(...) {
		get_reply()->send_error(ioremap::swarm::network_reply::bad_request);
	}
}

void on_add_activity::on_finished(bool added)
{
	if(!added)
		get_reply()->send_error(ioremap::swarm::network_reply::internal_server_error);
	else
		get_reply()->send_error(ioremap::swarm::network_reply::ok);
}

void on_add_activity::on_close(const boost::system::error_code &/*err*/)
{
	printf("AddActivity close\n");
}

} /* namespace history */
