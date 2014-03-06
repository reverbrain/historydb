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

#include "webserver.h"

#include <boost/bind.hpp>

#include <historydb/provider.h>
#include <elliptics/interface.h>

#include "on_add_log.h"
#include "on_add_activity.h"
#include "on_add_log_with_activity.h"
#include "on_get_active_users.h"
#include "on_get_user_logs.h"

namespace history {

webserver::webserver()
{}

bool webserver::initialize(const rapidjson::Value &config)
{
	if (!config.HasMember("remotes"))
		return false;

	if (!config.HasMember("groups"))
		return false;

	std::vector<std::string> remotes;
	std::vector<int> groups;
	std::string logfile = "/dev/stderr";
	int loglevel = DNET_LOG_INFO;
	uint32_t wait_timeout = 60, check_timeout = 60;

	if (config.HasMember("logfile"))
		logfile = config["logfile"].GetString();

	if (config.HasMember("loglevel"))
		loglevel = config["loglevel"].GetInt();

	if (config.HasMember("wait_timeout"))
		wait_timeout = config["wait_timeout"].GetUint();

	if (config.HasMember("check_timeout"))
		check_timeout = config["check_timeout"].GetUint();

	auto &remotesArray = config["remotes"];
	std::transform(remotesArray.Begin(), remotesArray.End(),
		std::back_inserter(remotes),
		std::bind(&rapidjson::Value::GetString, std::placeholders::_1));

	auto &groupsArray = config["groups"];
	std::transform(groupsArray.Begin(), groupsArray.End(),
		std::back_inserter(groups),
		std::bind(&rapidjson::Value::GetInt, std::placeholders::_1));

	uint32_t min_writes = groups.size();
	if (config.HasMember("min_writes"))
		min_writes = config["min_writes"].GetInt();

	provider_ = std::make_shared<provider>(remotes, groups, min_writes,
	                                       logfile, loglevel,
	                                       wait_timeout, check_timeout);

	on<on_root>(
		options::exact_match("/"),
		options::methods("GET")
	);
	on<on_add_log>(
		options::exact_match("/add_log"),
		options::methods("POST")
	);
	on<on_add_activity>(
		options::exact_match("/add_activity"),
		options::methods("POST")
	);
	on<on_add_log_with_activity>(
		options::exact_match("/add_log_with_activity"),
		options::methods("POST")
	);
	on<on_get_active_users>(
		options::exact_match("/get_active_users"),
		options::methods("GET")
	);
	on<on_get_user_logs>(
		options::exact_match("/get_user_logs"),
		options::methods("GET")
	);

	return true;
}

void webserver::on_root::on_request(const ioremap::swarm::http_request &/*req*/,
                                    const boost::asio::const_buffer &/*buffer*/)
{
	get_reply()->send_error(ioremap::swarm::http_response::ok);
}

} /* namespace history */

int main(int argc, char **argv)
{
	return ioremap::thevoid::run_server<history::webserver>(argc, argv);
}
