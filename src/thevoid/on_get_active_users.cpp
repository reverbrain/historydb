#include "on_get_active_users.h"

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
const char DATA_ITEM[] = "data";
const char BEGIN_TIME_ITEM[] = "begin_time";
const char END_TIME_ITEM[] = "end_time";
const char KEYS_ITEM[] = "keys";
}

void on_get_active_users::on_request(const ioremap::swarm::network_request &req, const boost::asio::const_buffer &/*buffer*/)
{
	try {
		ioremap::swarm::network_url url(req.get_url());
		ioremap::swarm::network_query_list query_list(url.query());

		if (query_list.has_item(consts::KEYS_ITEM)) {
			std::string keys_value = query_list.item_value(consts::KEYS_ITEM);
			std::vector<std::string> keys;
			boost::split(keys, keys_value, boost::is_any_of(":"));
			get_server()->get_provider()->get_active_users(keys,
			                                               std::bind(&on_get_active_users::on_finished,
			                                                         shared_from_this(),
			                                                         std::placeholders::_1)
			                                               );
		}
		else if (query_list.has_item(consts::BEGIN_TIME_ITEM) and query_list.has_item(consts::END_TIME_ITEM)) {
			get_server()->get_provider()->get_active_users(boost::lexical_cast<uint64_t>(query_list.item_value(consts::BEGIN_TIME_ITEM)),
			                                               boost::lexical_cast<uint64_t>(query_list.item_value(consts::END_TIME_ITEM)),
			                                               std::bind(&on_get_active_users::on_finished,
			                                                         shared_from_this(),
			                                                         std::placeholders::_1)
			                                               );
		}
		else
			throw std::invalid_argument("key and time are missed");
	}
	catch(ioremap::elliptics::error& e) {
		get_reply()->send_error(ioremap::swarm::network_reply::internal_server_error);
	}
	catch(...) {
		get_reply()->send_error(ioremap::swarm::network_reply::bad_request);
	}
}

void on_get_active_users::on_finished(const std::set<std::string>& active_users)
{
	rapidjson::Document d; // creates document for json serialization
	d.SetObject();

	rapidjson::Value ausers(rapidjson::kArrayType);

	for (auto it = active_users.begin(), itEnd = active_users.end(); it != itEnd; ++it) { // adds all active user with counters to json
		rapidjson::Value user(it->c_str(), it->size(), d.GetAllocator());
		ausers.PushBack(user, d.GetAllocator());
	}

	d.AddMember("active_users", ausers, d.GetAllocator());

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
	                          std::bind(&on_get_active_users::on_send_finished,
	                                    shared_from_this(),
	                                    result_str));
}

void on_get_active_users::on_send_finished(const std::string &)
{
	get_reply()->close(boost::system::error_code());
}

} /* namespace history */
