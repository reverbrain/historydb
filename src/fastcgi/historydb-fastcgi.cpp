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
	namespace consts {
		auto REMOTE_ADDR	= "5.255.215.168";
		auto REMOTE_PORT	= 1025;
		auto REMOTE_FAMILY	= 2;
		auto REMOTE_GROUPS	= 2;
	}

	handler::handler(fastcgi::ComponentContext* context)
	: fastcgi::Component(context)
	, m_logger(NULL) {}

	handler::~handler() {
	}

	void handler::onLoad() {
		const std::string logger_component_name = context()->getConfig()->asString(context()->getComponentXPath() + "/logger");
		m_logger = context()->findComponent<fastcgi::Logger>(logger_component_name);
		if(!m_logger) {
			throw std::runtime_error("cannot get component " + logger_component_name);
		}

		m_provider = history::create_provider(consts::REMOTE_ADDR, consts::REMOTE_PORT, consts::REMOTE_FAMILY);

		std::vector<int> groups;
		groups.push_back(consts::REMOTE_GROUPS);

		m_provider->set_session_parameters(groups, groups.size());
	}

	void handler::onUnload() {
		m_provider.reset();
	}

	void handler::handleRequest(fastcgi::Request* req, fastcgi::HandlerContext* context) {
		write_header(req);

		if(req->getURI() == std::string("/")) {
			handle_root(req, context);
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
		{
		fastcgi::RequestStream stream(req);
		stream <<	"<div class=\"title\" onclick=\"$(this).next('.content').toggle();\" style=\"cursor: pointer;\">Request info</div>\n\
						<div class=\"content\" style=\"display:none;background-color:gainsboro;\">\n";
		stream << "getUrl: <b>"				<< req->getUrl()			<< "</b><br>\n";
		stream << "getQueryString: <b>"		<< req->getQueryString()	<< "</b><br>\n";
		stream << "getRequestMethod: <b>"	<< req->getRequestMethod()	<< "</b><br>\n";
		stream << "getURI: <b>"				<< req->getURI()			<< "</b><br>\n";
		stream << "getPathInfo: <b>"		<< req->getPathInfo()		<< "</b><br>\n";
		stream << "getContentType: <b>"		<< req->getContentType()	<< "</b><br>\n";
		stream << "countHeaders: <b>"		<< req->countHeaders()		<< "</b><br>\n";
		stream << "countCookie: <b>"		<< req->countCookie()		<< "</b><br>\n";
		stream << "countArgs: <b>"			<< req->countArgs()			<< "</b><br>\n";
		std::vector<std::string> args;
		req->argNames(args);

		for(auto it = args.begin(); it != args.end(); ++it) {
			stream << *it << " = <b>" << req->getArg(*it) << "</b><br>" << std::endl;
		}

		stream << "<br> Files:" << std::endl;
		args.clear();
		req->remoteFiles(args);
		for(auto it = args.begin(); it != args.end(); ++it) {
			stream << *it << "<br>" << std::endl;
		}

		stream << "</div>\n\
					</div>\n";
		}

		close_html(req);
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
		//req->setStatus(200);
	}

	void handler::handle_wrong_uri(fastcgi::Request* req, fastcgi::HandlerContext* context)
	{
		std::cout << "getURI " << req->getURI() << std::endl;
		//req->setStatus(404);
	}

	void handler::handle_add_activity(fastcgi::Request* req, fastcgi::HandlerContext* context)
	{
		fastcgi::RequestStream stream(req);

		if(!req->hasArg("data")) {
			stream << "no data" << std::endl;
			return;
		}

		if(!req->hasArg("user")) {
			//req->setStatus(404);
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

		//req->setStatus(200);
	}

	void handler::handle_get_active_users(fastcgi::Request* req, fastcgi::HandlerContext* context)
	{
		fastcgi::RequestStream stream(req);
		std::string key, timestamp;
		if(req->hasArg("key"))
			key = req->getArg("key");
		if(req->hasArg("timestamp"))
			timestamp = req->getArg("timestamp");
		if(key.empty() && timestamp.empty()) {
			//req->setStatus(404);
			stream << "no key or timestamp" << std::endl;
			return;
		}

		std::map<std::string, uint32_t> res;
		if(!key.empty())
			res = m_provider->get_active_users(key);
		else
			res = m_provider->get_active_users(boost::lexical_cast<uint64_t>(timestamp));

		rapidjson::Document d;
		d.SetObject();

		for(auto it = res.begin(); it != res.end(); ++it) {
			d.AddMember(it->first.c_str(), it->second, d.GetAllocator());
		}

		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		d.Accept(writer);

		stream << buffer.GetString();

		//req->setStatus(200);
	}

	void handler::handle_get_user_logs(fastcgi::Request* req, fastcgi::HandlerContext* context)
	{
		fastcgi::RequestStream stream(req);
		if(!req->hasArg("user")) {
			//req->setStatus(404);
			stream << "no user" << std::endl;
			return;
		}

		if(!req->hasArg("begin_time")) {
			//req->setStatus(404);
			stream << "no begin time" << std::endl;
			return;
		}

		if(!req->hasArg("end_time")) {
			//req->setStatus(404);
			stream << "no end time" << std::endl;
			return;
		}

		auto user = req->getArg("user");
		auto begin_time = boost::lexical_cast<uint64_t>(req->getArg("begin_time"));
		auto end_time = boost::lexical_cast<uint64_t>(req->getArg("end_time"));

		auto res = m_provider->get_user_logs(user, begin_time, end_time);

		rapidjson::Document d;
		d.SetObject();

		rapidjson::Value value(rapidjson::kArrayType);

		for(auto it = res.begin(); it != res.end(); ++it) {

			rapidjson::Value vec(&it->front(), it->size(), d.GetAllocator());
			value.PushBack(vec, d.GetAllocator());
		}

		d.AddMember("logs", value, d.GetAllocator());

		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		d.Accept(writer);

		stream << buffer.GetString();
		//req->setStatus(200);
	}

	void handler::handle_test(fastcgi::Request* req, fastcgi::HandlerContext* context)
	{
		//req->setStatus(200);
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
