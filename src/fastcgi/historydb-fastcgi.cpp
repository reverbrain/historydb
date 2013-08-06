#include "historydb-fastcgi.h"
#include <iostream>
#include <stdexcept>

#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string.hpp>

#include <fastcgi2/logger.h>
#include <fastcgi2/config.h>
#include <fastcgi2/stream.h>
#include <fastcgi2/request.h>
#include <fastcgi2/component_factory.h>

#include <historydb/provider.h>
#include <elliptics/error.hpp>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#define ADD_HANDLER(script, func) m_handlers.insert(\
		std::make_pair(script, boost::bind(&handler::func, this, _1, _2)));

namespace history { namespace fcgi {

namespace consts {
const char USER_ITEM[] = "user";
const char KEY_ITEM[] = "key";
const char TIME_ITEM[] = "time";
const char DATA_ITEM[] = "data";
const char BEGIN_TIME_ITEM[] = "begin_time";
const char END_TIME_ITEM[] = "end_time";
const char KEYS_ITEM[] = "keys";
}

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

	if (!m_logger)
		throw std::runtime_error("cannot get component " + logger_component_name);

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

	const auto log_file = config->asString(xpath + "/log_file"); // gets historydb log file path from config
	const auto log_level = config->asString(xpath + "/log_level"); // gets historydb log level from config

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

	m_provider = std::make_shared<history::provider>(servers, // creates historydb provider instance
	                                                 groups,
	                                                 min_writes,
	                                                 log_file,
	                                                 history::get_log_level(log_level));
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
	ADD_HANDLER("/get_active_users",	handle_get_active_users);
	ADD_HANDLER("/get_user_logs",		handle_get_user_logs);
}

void handler::handle_root(fastcgi::Request* req, fastcgi::HandlerContext*)
{
	m_logger->debug("Handle root request\n");
	req->setHeader("Content-Length", "0");
	req->setStatus(200);
}

void handler::handle_wrong_uri(fastcgi::Request* req, fastcgi::HandlerContext*)
{
	m_logger->error("Handle request for unknown/unexpected uri:%s\n", req->getURI().c_str());
	req->setHeader("Content-Length", "0");
	req->setStatus(404); // Sets 404 status for respone - wrong uri code
}

void handler::handle_add_log(fastcgi::Request* req, fastcgi::HandlerContext*)
{
	m_logger->debug("Handle add log request\n");
	req->setHeader("Content-Length", "0");

	try {
		if (!req->hasArg("user") ||
		    !req->hasArg(consts::DATA_ITEM) ||
		    (!req->hasArg(consts::TIME_ITEM) &&
		     !req->hasArg(consts::KEY_ITEM)))
			throw std::invalid_argument("Required parameters are missing");

		auto data = req->getArg(consts::DATA_ITEM);
		std::vector<char> std_data(data.begin(), data.end());

		if (req->hasArg(consts::KEY_ITEM)) {
			m_provider->add_log(req->getArg(consts::USER_ITEM),
			                    req->getArg(consts::KEY_ITEM),
			                    std_data);
		}
		else if (req->hasArg(consts::TIME_ITEM)) {
			m_provider->add_log(req->getArg(consts::USER_ITEM),
			                    boost::lexical_cast<uint64_t>(req->getArg(consts::TIME_ITEM)),
			                    std_data);
		}
		else
			throw std::invalid_argument("Required parameters are missing");

		req->setStatus(200);
	}
	catch(ioremap::elliptics::error&) {
		req->setStatus(500);
	}
	catch(...) {
		req->setStatus(400);
	}
}

void handler::handle_add_activity(fastcgi::Request* req, fastcgi::HandlerContext*)
{
	m_logger->debug("Handle add activity request\n");
	req->setHeader("Content-Length", "0");

	try {
		if(!req->hasArg(consts::USER_ITEM))
			throw std::invalid_argument("Required parameters are missing");

		if(req->hasArg(consts::KEY_ITEM) &&
		   !req->getArg(consts::KEY_ITEM).empty()) {
			m_provider->add_activity(req->getArg(consts::USER_ITEM),
			                         req->getArg(consts::KEY_ITEM));
		}
		else if(req->hasArg(consts::TIME_ITEM)) {
			m_provider->add_activity(req->getArg(consts::USER_ITEM),
			                         boost::lexical_cast<uint64_t>(req->getArg(consts::TIME_ITEM)));
		}
		else
			throw std::invalid_argument("Required parameters are missing");

		req->setStatus(200);
	}
	catch(ioremap::elliptics::error&) {
		req->setStatus(500);
	}
	catch(...) {
		req->setStatus(400);
	}
}

void handler::handle_get_active_users(fastcgi::Request* req, fastcgi::HandlerContext*)
{
	m_logger->debug("Handle get active user request\n");
	try {
		fastcgi::RequestStream stream(req);

		std::set<std::string> res;

		if (req->hasArg(consts::KEYS_ITEM) &&
		    !req->getArg(consts::KEYS_ITEM).empty()) { // checks optional parameter key
			std::string keys_value = req->getArg(consts::KEYS_ITEM);
			std::vector<std::string> keys;
			boost::split(keys, keys_value, boost::is_any_of(":"));
			m_logger->debug("Gets active users by key: %s\n", keys.front().c_str());

			res = m_provider->get_active_users(keys); // gets active users by key
		}
		else if(req->hasArg(consts::BEGIN_TIME_ITEM) &&
		        req->hasArg(consts::END_TIME_ITEM)) { // checks optional parameter time
			res = m_provider->get_active_users(boost::lexical_cast<uint64_t>(req->getArg(consts::BEGIN_TIME_ITEM)),
											   boost::lexical_cast<uint64_t>(req->getArg(consts::END_TIME_ITEM))); // gets active users by time
		}
		else
			throw std::invalid_argument("Required parameters are missing");

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
		req->setHeader("Content-Length", boost::lexical_cast<std::string>(buffer.Size()));

		stream << json; // write result json to fastcgi stream
	}
	catch(ioremap::elliptics::error&) {
		req->setHeader("Content-Length", "0");
		req->setStatus(500);
	}
	catch(...) {
		req->setHeader("Content-Length", "0");
		req->setStatus(400);
	}
}

void handler::handle_get_user_logs(fastcgi::Request* req, fastcgi::HandlerContext*)
{
	m_logger->debug("Handlle get user logs request\n");
	try {
		fastcgi::RequestStream stream(req);

		if (!req->hasArg(consts::USER_ITEM))
			throw std::invalid_argument("Required parameters are missing");

		std::vector<char> res;

		if(req->hasArg(consts::KEYS_ITEM)) {
			std::string keys_value = req->getArg(consts::KEYS_ITEM);
			std::vector<std::string> keys;
			boost::split(keys, keys_value, boost::is_any_of(":"));

			res = m_provider->get_user_logs(req->getArg(consts::USER_ITEM),
											keys); // gets user logs from historydb library
		}
		else if(req->hasArg(consts::BEGIN_TIME_ITEM) && req->hasArg(consts::END_TIME_ITEM)) {
			res = m_provider->get_user_logs(req->getArg(consts::USER_ITEM),
											boost::lexical_cast<uint64_t>(req->getArg(consts::BEGIN_TIME_ITEM)),
											boost::lexical_cast<uint64_t>(req->getArg(consts::END_TIME_ITEM))); // gets user logs from historydb library
		}
		else
			throw std::invalid_argument("Required parameters are missing");

		rapidjson::Document d; // creates json document
		d.SetObject();

		rapidjson::Value user_logs(res.data(), res.size(), d.GetAllocator()); // creates vector value for user one's day log

		d.AddMember("logs", user_logs, d.GetAllocator()); // adds logs array to json document

		rapidjson::StringBuffer buffer; // creates string buffer for serialized json
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer); // creates json writer
		d.Accept(writer); // accepts writer by json document

		auto json = buffer.GetString();

		m_logger->debug("Result json: %s\n", json);

		req->setHeader("Content-Length", boost::lexical_cast<std::string>(buffer.Size()));

		stream << json; // writes result json to fastcgi stream

		req->setStatus(200);
	}
	catch(ioremap::elliptics::error&) {
		req->setHeader("Content-Length", "0");
		req->setStatus(500);
	}
	catch(...) {
		req->setHeader("Content-Length", "0");
		req->setStatus(400);
	}
}

FCGIDAEMON_REGISTER_FACTORIES_BEGIN()
	FCGIDAEMON_ADD_DEFAULT_FACTORY("historydb", handler)
FCGIDAEMON_REGISTER_FACTORIES_END()

} } /* namespace history { namespace fastcgi */
