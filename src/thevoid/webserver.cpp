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

	if (config.HasMember("logfile"))
		logfile = config["logfile"].GetString();

	if (config.HasMember("loglevel"))
		loglevel = config["loglevel"].GetInt();

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

	provider_ = std::make_shared<provider>(remotes,
	                                       groups ,
	                                       min_writes,
	                                       logfile,
	                                       loglevel);

	on<on_root>("/");
	on<on_add_log>("/add_log");
	on<on_add_activity>("/add_activity");
	on<on_add_log_with_activity>("/add_log_with_activity");
	on<on_get_active_users>("/get_active_users");
	on<on_get_user_logs>("/get_user_logs");

	return true;
}

void webserver::on_root::on_request(const ioremap::swarm::network_request &/*req*/,
                                    const boost::asio::const_buffer &/*buffer*/)
{
	get_reply()->send_error(ioremap::swarm::network_reply::ok);
}

} /* namespace history */

int main(int argc, char **argv)
{
	return ioremap::thevoid::run_server<history::webserver>(argc, argv);
}
