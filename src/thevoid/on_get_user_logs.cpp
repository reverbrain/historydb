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

#include "on_get_user_logs.h"

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
const char USER_ITEM[] = "user";
const char BEGIN_TIME_ITEM[] = "begin_time";
const char END_TIME_ITEM[] = "end_time";
const char KEYS_ITEM[] = "keys";
}

void on_get_user_logs::on_request(const ioremap::swarm::http_request &req, const boost::asio::const_buffer &/*buffer*/)
{
	try {
		const auto &query = req.url().query();

		auto user_item = query.item_value(consts::USER_ITEM);
		if (!user_item)
			throw std::invalid_argument("user is missed");

		auto begin_time = query.item_value(consts::BEGIN_TIME_ITEM);
		auto end_time = query.item_value(consts::END_TIME_ITEM);

		if (auto keys_item = query.item_value(consts::KEYS_ITEM)) {
			std::vector<std::string> keys;
			boost::split(keys, *keys_item, boost::is_any_of(":"));
			server()
			->get_provider()
			->get_user_logs(*user_item,
			                keys,
			                std::bind(&on_get_user_logs::on_finished,
			                          shared_from_this(),
			                          std::placeholders::_1));
		} else if(begin_time && end_time) {
			server()
			->get_provider()
			->get_user_logs(*user_item,
			                boost::lexical_cast<uint64_t>(*begin_time),
			                boost::lexical_cast<uint64_t>(*end_time),
			                std::bind(&on_get_user_logs::on_finished,
			                          shared_from_this(),
			                          std::placeholders::_1));
		}
		else
			throw std::invalid_argument("something is missed");

	}
	catch(ioremap::elliptics::error& e) {
		get_reply()->send_error(ioremap::swarm::http_response::internal_server_error);
	}
	catch(...) {
		get_reply()->send_error(ioremap::swarm::http_response::bad_request);
	}
}

bool on_get_user_logs::on_finished(const std::vector<ioremap::elliptics::data_pointer>& data)
{
	std::string result_str;
	if(!data.empty()) {
		rapidjson::Document d; // creates json document
		d.SetObject();

		rapidjson::Value user_logs(rapidjson::kArrayType); // creates vector value for user one's day log

		for (auto it = data.begin(), end = data.end(); it != end; ++it) {
			rapidjson::Value user_log(it->data<char>(), it->size(), d.GetAllocator()); // creates vector value for user one's day log
			user_logs.PushBack(user_log, d.GetAllocator());
		}

		d.AddMember("logs", user_logs, d.GetAllocator()); // adds logs array to json document

		rapidjson::StringBuffer buffer; // creates string buffer for serialized json
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer); // creates json writer
		d.Accept(writer); // accepts writer by json document

		result_str = buffer.GetString();
	}

	ioremap::swarm::http_response reply;
	reply.set_code(ioremap::swarm::http_response::ok);

	auto &headers = reply.headers();
	headers.set_content_length(result_str.size());
	headers.set_content_type("text/json");

	get_reply()->send_headers(std::move(reply),
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

} /* namespace history */
