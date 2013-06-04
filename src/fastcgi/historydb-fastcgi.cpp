#include "historydb-fastcgi.h"
#include <iostream>
#include <stdexcept>

#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>

#include <fastcgi2/logger.h>
#include <fastcgi2/config.h>
#include <fastcgi2/stream.h>
#include <fastcgi2/request.h>
#include <fastcgi2/component_factory.h>

#include <historydb/provider.h>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#define ADD_HANDLER(script, func) m_handlers.insert(std::make_pair(script, boost::bind(&handler::func, this, _1, _2)));

namespace history { namespace fcgi {

	handler::handler(fastcgi::ComponentContext* context)
	: fastcgi::Component(context)
	, m_logger(NULL)
	{
		init_handlers(); // Inits handlers map
	}

	handler::~handler()
	{}

	void handler::onLoad()
	{
		const auto xpath = context()->getComponentXPath();
		auto config = context()->getConfig();
		const std::string logger_component_name = config->asString(xpath + "/logger"); // get logger name
		m_logger = context()->findComponent<fastcgi::Logger>(logger_component_name); // get logger component

		if (!m_logger) {
			throw std::runtime_error("cannot get component " + logger_component_name);
		}

		std::vector<std::string> subs;
		std::vector<server_info> servers;
		std::vector<std::string> addrs;
		m_logger->debug("HistoryDB handler loaded\n");

		config->subKeys(xpath + "/elliptics", subs);
		addrs.reserve(subs.size());

		for(auto it = subs.begin(), itEnd = subs.end(); it != itEnd; ++it) {
			addrs.emplace_back(config->asString(*it + "/addr"));
			server_info info;
			info.addr = *(addrs.rbegin());
			info.port = config->asInt(*it + "/port");
			info.family = config->asInt(*it + "/family");
			servers.emplace_back(info);
		}

		const auto log_file		= config->asString(xpath + "/log_file"); // gets historydb log file path from config
		const auto log_level	= config->asString(xpath + "/log_level"); // gets historydb log level from config

		m_logger->debug("HistoryDB provider has been created\n");

		std::vector<int> groups;

		m_logger->debug("Setting elliptics groups:\n");
		subs.clear();
		config->subKeys(xpath + "/group", subs); // gets set of groups keys in config

		int group = 0;
		for (auto it = subs.begin(), itEnd = subs.end(); it != itEnd; ++it) {
			group = config->asInt(*it); // gets group number from config
			groups.push_back(group); // adds the group number to vector
			m_logger->debug("Added %d group\n", group);
		}

		int min_writes = config->asInt(xpath + "/min_writes");

		m_provider = std::make_shared<history::provider>(servers, groups, min_writes, log_file, history::get_log_level(log_level)); // creates historydb provider instance
	}

	void handler::onUnload()
	{
		m_logger->debug("Unloading HistoryDB handler\n");
		m_provider.reset(); // destroys provider
		m_logger->debug("HistoryDB provider has been destroyed\n");
	}

	void handler::handleRequest(fastcgi::Request* req, fastcgi::HandlerContext* context)
	{
		auto script_name = req->getScriptName();
		m_logger->debug("Handle request: URI:%s\n", script_name.c_str());
		auto it = m_handlers.find(script_name); // finds handler for the script
		if (it != m_handlers.end()) { // if handler has been found
			it->second(req, context); // call handler
			return;
		}

		handle_wrong_uri(req, context); // calls wrong uri handler
	}

	void handler::init_handlers()
	{
		ADD_HANDLER("/",					handle_root);
		ADD_HANDLER("/add_log",				handle_add_log);
		ADD_HANDLER("/add_activity",		handle_add_activity);
		//ADD_HANDLER("/add_user_activity",	handle_add_user_activity);
		ADD_HANDLER("/get_active_users",	handle_get_active_users);
		ADD_HANDLER("/get_user_logs",		handle_get_user_logs);
	}

	void handler::handle_root(fastcgi::Request* req, fastcgi::HandlerContext*)
	{
		m_logger->debug("Handle root request\n");
		req->setStatus(200);
	}

	void handler::handle_wrong_uri(fastcgi::Request* req, fastcgi::HandlerContext*)
	{
		m_logger->error("Handle request for unknown/unexpected uri:%s\n", req->getURI().c_str());
		req->setStatus(404); // Sets 404 status for respone - wrong uri code
	}

	void handler::handle_add_log(fastcgi::Request* req, fastcgi::HandlerContext*)
	{
		m_logger->debug("Handle add log request\n");

		if(!req->hasArg("user") || !req->hasArg("data") || !req->hasArg("time")) {
			m_logger->error("Required parameter 'data' or 'user' or 'time' is missing\n");
			req->setStatus(404);
			return;
		}

		fastcgi::RequestStream stream(req);

		auto user = req->getArg("user");
		auto time = boost::lexical_cast<uint64_t>(req->getArg("time"));
		auto data = req->getArg("data");

		std::vector<char> std_data(data.begin(), data.end());

		m_provider->add_log(user, time, std_data);
	}

	void handler::handle_add_activity(fastcgi::Request* req, fastcgi::HandlerContext*)
	{
		m_logger->debug("Handle add activity request\n");

		if(!req->hasArg("user")) {
			m_logger->error("Required parameter 'user' is missing\n");
			req->setStatus(404);
			return;
		}

		fastcgi::RequestStream stream(req);

		auto user = req->getArg("user");

		if(req->hasArg("key") && !req->getArg("key").empty()) {
			auto key = req->getArg("key");
			m_provider->add_activity(user, key);
		}
		else if(req->hasArg("time")) {
			auto time = boost::lexical_cast<uint64_t>(req->getArg("time"));
			m_provider->add_activity(user, time);
		}
		else {
			m_provider->add_activity(user, ::time(NULL));
		}
	}

	/*void handler::handle_add_user_activity(fastcgi::Request* req, fastcgi::HandlerContext*)
	{
		m_logger->debug("Handle add user activity request\n");
		fastcgi::RequestStream stream(req);

		if (!req->hasArg("data") || !req->hasArg("user")) { // checks required parameters
			m_logger->error("Required parameter 'data' or 'user' is missing\n");
			req->setStatus(404);
			return;
		}

		auto user = req->getArg("user"); // gets user parameter
		auto data = req->getArg("data"); // gets data parameter

		std::string key = std::string();
		if (req->hasArg("key")) // checks optional parameter key
			key = req->getArg("key"); // gets key parameter

		uint64_t tm = time(NULL);
		if (req->hasArg("time")) // checks optional parameter time
			tm = boost::lexical_cast<uint64_t>(req->getArg("time")); // gets time parameter

		m_logger->debug("Add user activity: (%s, %d, ..., %d, %s\n", user.c_str(), tm, data.size(), key.c_str());
		m_provider->add_user_activity(user, tm, const_cast<char*>(data.c_str()), data.size(), key); // calls provider function to add user activity
	}*/

	void handler::handle_get_active_users(fastcgi::Request* req, fastcgi::HandlerContext*)
	{
		m_logger->debug("Handle get active user request\n");
		fastcgi::RequestStream stream(req);

		std::set<std::string> res;

		if (req->hasArg("key") && !req->getArg("key").empty()) { // checks optional parameter key
			auto key = req->getArg("key"); // gets key parameter
			m_logger->debug("Gets active users by key: %s\n", key.c_str());
			res = m_provider->get_active_users(key); // gets active users by key
		}
		else if(req->hasArg("time")) { // checks optional parameter time
			auto time = boost::lexical_cast<uint64_t>(req->getArg("time")); // gets time parameter
			m_logger->debug("Gets active users by time: %d\n", time);
			res = m_provider->get_active_users(time); // gets active users by time
		}
		else { // if key and time aren't among the parameters
			m_logger->error("Key and time are missing\n");
			req->setStatus(404);
			return;
		}

		rapidjson::Document d; // creates document for json serialization
		d.SetObject();

		rapidjson::Value active_users(rapidjson::kArrayType);

		for (auto it = res.begin(), itEnd = res.end(); it != itEnd; ++it) { // adds all active user with counters to json
			rapidjson::Value user(it->c_str(), it->size(), d.GetAllocator());
			active_users.PushBack(user, d.GetAllocator());
		}

		d.AddMember("active_users", active_users, d.GetAllocator());

		rapidjson::StringBuffer buffer; // creates string buffer for serialized json
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer); // creates json writer
		d.Accept(writer); // accepts writer by json document

		auto json = buffer.GetString();

		m_logger->debug("Result json: %s\n", json);

		stream << json; // write result json to fastcgi stream
	}

	void handler::handle_get_user_logs(fastcgi::Request* req, fastcgi::HandlerContext*)
	{
		m_logger->debug("Handlle get user logs request\n");
		fastcgi::RequestStream stream(req);

		if (!req->hasArg("user") || !req->hasArg("begin_time") || !req->hasArg("end_time")) { // checks required parameters
			m_logger->error("Required parameter 'user' or 'begin_time' or 'end_time' is missing\n");
			req->setStatus(404);
			return;
		}

		auto user = req->getArg("user"); // gets user parameter
		auto begin_time = boost::lexical_cast<uint64_t>(req->getArg("begin_time")); // gets begin_time parameter
		auto end_time = boost::lexical_cast<uint64_t>(req->getArg("end_time")); // gets end_time parameter

		m_logger->debug("Gets user logs for: (%s, %d, %d)\n", user.c_str(), begin_time, end_time);
		auto res = m_provider->get_user_logs(user, begin_time, end_time); // gets user logs from historydb library

		rapidjson::Document d; // creates json document
		d.SetObject();

		rapidjson::Value user_logs(rapidjson::kArrayType); // array value which will contain all user logs for specified time

		for (auto it = res.begin(), itEnd = res.end(); it != itEnd; ++it) {
			rapidjson::Value vec(&it->front(), it->size(), d.GetAllocator()); // creates vector value for user one's day log
			user_logs.PushBack(vec, d.GetAllocator()); // adds daily logs to result array
		}

		d.AddMember("logs", user_logs, d.GetAllocator()); // adds logs array to json document

		rapidjson::StringBuffer buffer; // creates string buffer for serialized json
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer); // creates json writer
		d.Accept(writer); // accepts writer by json document

		auto json = buffer.GetString();

		m_logger->debug("Result json: %s\n", json);

		stream << json; // writes result json to fastcgi stream
	}

	FCGIDAEMON_REGISTER_FACTORIES_BEGIN()
		FCGIDAEMON_ADD_DEFAULT_FACTORY("historydb", handler)
	FCGIDAEMON_REGISTER_FACTORIES_END()

} } /* namespace history { namespace fastcgi */
