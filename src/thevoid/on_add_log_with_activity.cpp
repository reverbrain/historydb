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

#include "on_add_log_with_activity.h"

#include <swarm/network_url.h>
#include <swarm/network_query_list.h>

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

void on_add_log_with_activity::on_request(const ioremap::swarm::network_request &/*req*/,
                                          const boost::asio::const_buffer &buffer)
{
	try {
		std::string request(boost::asio::buffer_cast<const char*>(buffer),
		                    boost::asio::buffer_size(buffer));
		ioremap::swarm::network_query_list query_list(request);

		if(!query_list.has_item(consts::USER_ITEM) ||
		   !query_list.has_item(consts::DATA_ITEM) ||
		   (!query_list.has_item(consts::TIME_ITEM) &&
		    !query_list.has_item(consts::KEY_ITEM)))
			throw std::invalid_argument("user, data, time or key");

		if(query_list.has_item(consts::KEY_ITEM)) {
			get_server()
			->get_provider()
			->add_log_with_activity(query_list.item_value(consts::USER_ITEM),
			                        query_list.item_value(consts::KEY_ITEM),
			                        ioremap::elliptics::data_pointer::copy(query_list.item_value(consts::DATA_ITEM)),
			                        std::bind(&on_add_log_with_activity::on_finish,
			                                  shared_from_this(),
			                                  std::placeholders::_1));
		}
		else if(query_list.has_item("time")) {
			get_server()
			->get_provider()
			->add_log_with_activity(query_list.item_value(consts::USER_ITEM),
			                        boost::lexical_cast<uint64_t>(query_list.item_value(consts::TIME_ITEM)),
			                        ioremap::elliptics::data_pointer::copy(query_list.item_value(consts::DATA_ITEM)),
			                        std::bind(&on_add_log_with_activity::on_finish,
			                                  shared_from_this(),
			                                  std::placeholders::_1));
		}
		else
			throw std::invalid_argument("Key and time are missed");
	}
	catch(ioremap::elliptics::error&) {
		get_reply()->send_error(ioremap::swarm::network_reply::internal_server_error);
	}
	catch(...) {
		get_reply()->send_error(ioremap::swarm::network_reply::bad_request);
	}
}

void on_add_log_with_activity::on_finish(bool added)
{
	if(!added)
		get_reply()->send_error(ioremap::swarm::network_reply::internal_server_error);
	else
		get_reply()->send_error(ioremap::swarm::network_reply::ok);
}

} /* namespace history */
