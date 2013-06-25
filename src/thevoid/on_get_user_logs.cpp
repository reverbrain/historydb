#include "on_get_user_logs.h"

#include <swarm/network_url.h>
#include <swarm/network_query_list.h>

#include <historydb/provider.h>
#include <elliptics/error.hpp>

#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>

#include "../fastcgi/rapidjson/document.h"
#include "../fastcgi/rapidjson/writer.h"
#include "../fastcgi/rapidjson/stringbuffer.h"

namespace history {

void on_get_user_logs::on_request(const ioremap::swarm::network_request &req, const boost::asio::const_buffer &/*buffer*/)
{
	printf("GetUserLogs request\n");
	try {
		ioremap::swarm::network_url url(req.get_url());
		ioremap::swarm::network_query_list query_list(url.query());

		if (!query_list.has_item("user") || !query_list.has_item("begin_time") || !query_list.has_item("end_time")) // checks required parameters
			throw std::invalid_argument("user or begin_time or end_time is missed");

		auto user = query_list.item_value("user"); // gets user parameter
		auto begin_time = boost::lexical_cast<uint64_t>(query_list.item_value("begin_time")); // gets begin_time parameter
		auto end_time = boost::lexical_cast<uint64_t>(query_list.item_value("end_time")); // gets end_time parameter

		get_server()->get_provider()->get_user_logs(user,
		                                            begin_time,
		                                            end_time,
		                                            std::bind(&on_get_user_logs::on_finished,
		                                                      shared_from_this(),
		                                                      std::placeholders::_1));

	}
	catch(ioremap::elliptics::error& e) {
		get_reply()->send_error(ioremap::swarm::network_reply::internal_server_error);
	}
	catch(...) {
		get_reply()->send_error(ioremap::swarm::network_reply::bad_request);
	}
}

bool on_get_user_logs::on_finished(const std::vector<char>& data)
{
	rapidjson::Document d; // creates json document
	d.SetObject();

	rapidjson::Value user_logs(&data.front(), data.size(), d.GetAllocator()); // creates vector value for user one's day log

	d.AddMember("logs", user_logs, d.GetAllocator()); // adds logs array to json document

	rapidjson::StringBuffer buffer; // creates string buffer for serialized json
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer); // creates json writer
	d.Accept(writer); // accepts writer by json document

	const std::string result_str = buffer.GetString();

	ioremap::swarm::network_reply reply;
	reply.set_code(ioremap::swarm::network_reply::ok);
	reply.set_content_length(result_str.size());
	reply.set_content_type("text/json");
	get_reply()->send_headers(reply,
	                          boost::asio::buffer(result_str),
	                          std::bind(&on_get_user_logs::on_send_finished,
	                                    shared_from_this(),
	                                    result_str));
	return false;
}

void on_get_user_logs::on_send_finished(const std::string &)
{
	get_reply()->close(boost::system::error_code());
}

void on_get_user_logs::on_close(const boost::system::error_code &/*err*/)
{
	printf("GetUserLogs close\n");
}

} /* namespace history */
