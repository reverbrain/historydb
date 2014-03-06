/*
 * Copyright 2013+ Kirill Smorodinnikov <shaitkir@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#include "on_get_active_users.h"

#include <swarm/url.hpp>
#include <swarm/url_query.hpp>

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
const char BEGIN_TIME_ITEM[] = "begin_time";
const char END_TIME_ITEM[] = "end_time";
const char KEYS_ITEM[] = "keys";
}

void on_get_active_users::on_request(const ioremap::swarm::http_request &req,
                                     const boost::asio::const_buffer &/*buffer*/)
{
	try {
		const auto &query = req.url().query();

		auto begin_time = query.item_value(consts::BEGIN_TIME_ITEM);
		auto end_time = query.item_value(consts::END_TIME_ITEM);

		if (auto keys_item = query.item_value(consts::KEYS_ITEM)) {
			std::vector<std::string> keys;
			boost::split(keys, *keys_item, boost::is_any_of(":"));
			server()
			->get_provider()
			->get_active_users(keys,
			                   std::bind(&on_get_active_users::on_finished,
			                             shared_from_this(),
			                             std::placeholders::_1));
		} else if (begin_time and end_time) {
			server()
			->get_provider()
			->get_active_users(boost::lexical_cast<uint64_t>(*begin_time),
			                   boost::lexical_cast<uint64_t>(*end_time),
			                   std::bind(&on_get_active_users::on_finished,
			                             shared_from_this(),
			                             std::placeholders::_1));
		}
		else
			throw std::invalid_argument("key and time are missed");
	}
	catch(ioremap::elliptics::error& e) {
		get_reply()->send_error(ioremap::swarm::http_response::internal_server_error);
	}
	catch(...) {
		get_reply()->send_error(ioremap::swarm::http_response::bad_request);
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

	ioremap::swarm::http_response reply;
	reply.set_code(ioremap::swarm::http_response::ok);

	auto &headers = reply.headers();
	headers.set_content_length(result_str.size());
	headers.set_content_type("text/json");

	get_reply()->send_headers(std::move(reply),
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
