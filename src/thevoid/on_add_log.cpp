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

#include "on_add_log.h"

#include <swarm/url.hpp>
#include <swarm/url_query.hpp>

#include <historydb/provider.h>
#include <elliptics/error.hpp>

#include <boost/lexical_cast.hpp>

namespace history {

namespace consts {
const char USER_ITEM[] = "user";
const char KEY_ITEM[] = "key";
const char TIME_ITEM[] = "time";
const char DATA_ITEM[] = "data";
}

void on_add_log::on_request(const ioremap::swarm::http_request &/*req*/,
                            const boost::asio::const_buffer &buffer)
{
	try {
		std::string request(boost::asio::buffer_cast<const char*>(buffer),
		                    boost::asio::buffer_size(buffer));
		ioremap::swarm::url_query query(request);

		auto user_item = query.item_value(consts::USER_ITEM);
		if(!user_item)
			throw std::invalid_argument("user is missed");

		auto data_item = query.item_value(consts::DATA_ITEM);
		if(!data_item)
			throw std::invalid_argument("data is missed");

		if(auto key_item = query.item_value(consts::KEY_ITEM)) {
			server()
			->get_provider()
			->add_log(*user_item,
			          *key_item,
			          ioremap::elliptics::data_pointer::copy(*data_item),
			          std::bind(&on_add_log::on_finish,
			                    shared_from_this(),
			                    std::placeholders::_1));
		} else if(auto time_item = query.item_value(consts::TIME_ITEM)) {
			server()
			->get_provider()
			->add_log(*user_item,
			          boost::lexical_cast<uint64_t>(*time_item),
			          ioremap::elliptics::data_pointer::copy(*data_item),
			          std::bind(&on_add_log::on_finish,
			                    shared_from_this(),
			                    std::placeholders::_1));
		}
		else
			throw std::invalid_argument("Key and time are missed");
	}
	catch(ioremap::elliptics::error&) {
		get_reply()->send_error(ioremap::swarm::http_response::internal_server_error);
	}
	catch(...) {
		get_reply()->send_error(ioremap::swarm::http_response::bad_request);
	}
}

void on_add_log::on_finish(bool added)
{
	if(!added)
		get_reply()->send_error(ioremap::swarm::http_response::internal_server_error);
	else
		get_reply()->send_error(ioremap::swarm::http_response::ok);
}

} /* namespace history */
