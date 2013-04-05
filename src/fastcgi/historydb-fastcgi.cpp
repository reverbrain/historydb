#include "historydb-fastcgi.h"
#include <iostream>
#include <stdexcept>

#include <boost/lexical_cast.hpp>

#include <fastcgi2/logger.h>
#include <fastcgi2/config.h>
#include <fastcgi2/stream.h>
#include <fastcgi2/request.h>
#include <fastcgi2/component_factory.h>

#include <historydb/iprovider.h>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

namespace history { namespace fcgi {

	handler::handler(fastcgi::ComponentContext* context)
	: fastcgi::Component(context)
	, m_logger(NULL)
	{}

	handler::~handler()
	{
	}

	void handler::onLoad()
	{
		const auto xpath = context()->getComponentXPath();
		auto config = context()->getConfig();
		const std::string logger_component_name = config->asString(xpath + "/logger");
		m_logger = context()->findComponent<fastcgi::Logger>(logger_component_name);
		if(!m_logger) {
			throw std::runtime_error("cannot get component " + logger_component_name);
		}

		m_provider = history::create_provider(config->asString(xpath + "/elliptics_addr").c_str(), config->asInt(xpath + "/elliptics_port"), config->asInt(xpath + "/elliptics_family"));

		std::vector<std::string> subs;
		std::vector<int> groups;

		config->subKeys(xpath + "/elliptics_group_", subs);

		for(auto it = subs.begin(), itEnd = subs.end(); it != itEnd; ++it) {
			groups.push_back(config->asInt(*it));
		}

		m_provider->set_session_parameters(groups, groups.size());
	}

	void handler::onUnload()
	{
		m_provider.reset();
	}

	void handler::handleRequest(fastcgi::Request* req, fastcgi::HandlerContext* context)
	{

		if(req->getURI() == std::string("/")) {
			write_header(req);
			handle_root(req, context);
			close_html(req);
		}
		else if(req->getURI() == std::string("/test")) {
			handle_test(req, context);
		}
		else if(req->getURI() == std::string("/add_activity")) {
			handle_add_activity(req, context);
		}
		else if(req->getURI().find("/get_active_users?") == 0) {
			handle_get_active_users(req, context);
		}
		else if(req->getURI().find("/get_user_logs?") == 0) {
			handle_get_user_logs(req, context);
		}
		else {
			handle_wrong_uri(req, context);
		}

	}

	void handler::handle_root(fastcgi::Request* req, fastcgi::HandlerContext*)
	{
		fastcgi::RequestStream stream(req);
		stream <<	"<div class=\"title\" onclick=\"$(this).next('.content').toggle();\" style=\"cursor: pointer;\">Add activity</div>\n\
						<div class=\"content\" style=\"display:none;background-color:gainsboro;\">\n\
							<form name=\"add\" action=\"add_activity\" method=\"post\">\n\
								User name: <input type=\"text\" name=\"user\" value=\"WebUser1\"><br>\n\
								Timestamp: <input type=\"text\" name=\"timestamp\" value=\"\"><br>\n\
								Data: <input type=\"text\" name=\"data\" value=\"Some data\"><br>\n\
								Key: <input type=\"text\" name=\"key\" valude=\"\"><br>\n\
								<input type=\"submit\" value=\"Send\">\n\
							</form>\n\
						</div>\n\
					</div>\n\
					<div class=\"title\" onclick=\"$(this).next('.content').toggle();\" style=\"cursor: pointer;\">Get active users</div>\n\
						<div class=\"content\" style=\"display:none;background-color:gainsboro;\">\n\
							<form name=\"get_user\" action=\"get_active_users\" method=\"get\">\n\
								Timestamp: <input type=\"text\" name=\"timestamp\" value=\"\"><br>\n\
								Key: <input type=\"text\" name=\"key\" valude=\"\"><br>\n\
								<input type=\"submit\" value=\"Send\">\n\
							</form>\n\
						</div>\n\
					</div>\n\
					<div class=\"title\" onclick=\"$(this).next('.content').toggle();\" style=\"cursor: pointer;\">Get user logs</div>\n\
						<div class=\"content\" style=\"display:none;background-color:gainsboro;\">\n\
							<form name=\"get_logs\" action=\"get_user_logs\" method=\"get\">\n\
								User name: <input type=\"text\" name=\"user\" value=\"WebUser1\"><br>\n\
								Begin timestamp: <input type=\"text\" name=\"begin_time\" value=\"\"><br>\n\
								End timestamp: <input type=\"text\" name=\"end_time\" value=\"\"><br>\n\
								<input type=\"submit\" value=\"Send\">\n\
							</form>\n\
						</div>\n\
					</div>\n";
	}

	void handler::handle_wrong_uri(fastcgi::Request* req, fastcgi::HandlerContext*)
	{
		std::cout << "getURI " << req->getURI() << std::endl;
		req->setStatus(404);
	}

	void handler::handle_add_activity(fastcgi::Request* req, fastcgi::HandlerContext*)
	{
		fastcgi::RequestStream stream(req);

		if(!req->hasArg("data")) {
			stream << "no data" << std::endl;
			return;
		}

		if(!req->hasArg("user")) {
			req->setStatus(404);
			stream << "no user" << std::endl;
			return;
		}

		auto user = req->getArg("user");
		auto data = req->getArg("data");

		std::string key = std::string();
		if(req->hasArg("key"))
			key = req->getArg("key");

		uint64_t tm = time(NULL);
		if(req->hasArg("timestamp"))
			tm = boost::lexical_cast<uint64_t>(req->getArg("timestamp"));

		m_provider->add_user_activity(user, tm, &data.front(), data.size(), key);
	}

	void handler::handle_get_active_users(fastcgi::Request* req, fastcgi::HandlerContext*)
	{
		fastcgi::RequestStream stream(req);
		std::string key, timestamp;

		if(req->hasArg("key"))
			key = req->getArg("key");

		if(req->hasArg("timestamp"))
			timestamp = req->getArg("timestamp");

		if(key.empty() && timestamp.empty()) {
			req->setStatus(404);
			return;
		}

		std::map<std::string, uint32_t> res;
		if(!key.empty())
			res = m_provider->get_active_users(key);
		else
			res = m_provider->get_active_users(boost::lexical_cast<uint64_t>(timestamp));

		rapidjson::Document d;
		d.SetObject();

		for(auto it = res.begin(), itEnd = res.end(); it != itEnd; ++it) {
			d.AddMember(it->first.c_str(), it->second, d.GetAllocator());
		}

		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		d.Accept(writer);

		stream << buffer.GetString();
	}

	void handler::handle_get_user_logs(fastcgi::Request* req, fastcgi::HandlerContext*)
	{
		fastcgi::RequestStream stream(req);
		if(!req->hasArg("user")) {
			req->setStatus(404);
			return;
		}

		if(!req->hasArg("begin_time")) {
			req->setStatus(404);
			return;
		}

		if(!req->hasArg("end_time")) {
			req->setStatus(404);
			return;
		}

		auto user = req->getArg("user");
		auto begin_time = boost::lexical_cast<uint64_t>(req->getArg("begin_time"));
		auto end_time = boost::lexical_cast<uint64_t>(req->getArg("end_time"));

		auto res = m_provider->get_user_logs(user, begin_time, end_time);

		rapidjson::Document d;
		d.SetObject();

		rapidjson::Value value(rapidjson::kArrayType);

		for(auto it = res.begin(), itEnd = res.end(); it != itEnd; ++it) {

			rapidjson::Value vec(&it->front(), it->size(), d.GetAllocator());
			value.PushBack(vec, d.GetAllocator());
		}

		d.AddMember("logs", value, d.GetAllocator());

		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		d.Accept(writer);

		stream << buffer.GetString();
	}

	void handler::handle_test(fastcgi::Request* req, fastcgi::HandlerContext*)
	{
		req->setStatus(200);
	}

	void handler::write_header(fastcgi::Request* req)
	{
		fastcgi::RequestStream stream(req);

		stream <<	"<html> \n\
						<head> \n\
						<script src=\"//ajax.googleapis.com/ajax/libs/jquery/1.9.1/jquery.min.js\"></script> \n\
						</head> \n\
						<body>\n";
	}

	void handler::close_html(fastcgi::Request* req)
	{
		fastcgi::RequestStream stream(req);

		stream << "		</body> \n\
					</html>\n";
	}

	FCGIDAEMON_REGISTER_FACTORIES_BEGIN()
		FCGIDAEMON_ADD_DEFAULT_FACTORY("historydb", handler)
	FCGIDAEMON_REGISTER_FACTORIES_END()

} } /* namespace history { namespace fastcgi */
