#include "on_get_user_logs.h"

#include <swarm/network_url.h>
#include <swarm/network_query_list.h>

#include <historydb/provider.h>
#include <elliptics/error.hpp>

#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include "../fastcgi/rapidjson/document.h"
#include "../fastcgi/rapidjson/writer.h"
#include "../fastcgi/rapidjson/stringbuffer.h"

namespace history {

namespace consts {
const char USER_ITEM[] = "user";
const char BEGIN_TIME_ITEM[] = "begin_time";
const char END_TIME_ITEM[] = "end_time";
const char KEYS_ITEM[] = "keys";
}

void on_get_user_logs::on_request(const ioremap::swarm::network_request &req, const boost::asio::const_buffer &/*buffer*/)
{
	try {
		ioremap::swarm::network_url url(req.get_url());
		ioremap::swarm::network_query_list query_list(url.query());

		if (!query_list.has_item(consts::USER_ITEM) || (!query_list.has_item(consts::KEYS_ITEM) && (!query_list.has_item(consts::BEGIN_TIME_ITEM) || !query_list.has_item(consts::END_TIME_ITEM)))) // checks required parameters
			throw std::invalid_argument("user or begin_time or end_time is missed");

		if (query_list.has_item(consts::KEYS_ITEM)) {
			std::string keys_value = query_list.item_value(consts::KEYS_ITEM);
			std::vector<std::string> keys;
			boost::split(keys, keys_value, boost::is_any_of(":"));
			get_server()->get_provider()->get_user_logs(query_list.item_value(consts::USER_ITEM),
			                                            keys,
			                                            std::bind(&on_get_user_logs::on_finished,
			                                                      shared_from_this(),
			                                                      std::placeholders::_1)
			                                            );
		}
		else if(query_list.has_item(consts::BEGIN_TIME_ITEM) && query_list.has_item(consts::END_TIME_ITEM)) {
			get_server()->get_provider()->get_user_logs(query_list.item_value(consts::USER_ITEM),
			                                            boost::lexical_cast<uint64_t>(query_list.item_value(consts::BEGIN_TIME_ITEM)),
			                                            boost::lexical_cast<uint64_t>(query_list.item_value(consts::END_TIME_ITEM)),
			                                            std::bind(&on_get_user_logs::on_finished,
			                                                      shared_from_this(),
			                                                      std::placeholders::_1)
			                                            );
		}
		else
			throw std::invalid_argument("something is missed");

	}
	catch(ioremap::elliptics::error& e)
		get_reply()->send_error(ioremap::swarm::network_reply::internal_server_error);
	catch(...)
		get_reply()->send_error(ioremap::swarm::network_reply::bad_request);
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
	                                    result_str)
	                          );
	return false;
}

void on_get_user_logs::on_send_finished(const std::string &)
{
	get_reply()->close(boost::system::error_code());
}

} /* namespace history */
