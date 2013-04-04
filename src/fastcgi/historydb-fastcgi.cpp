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
		std::cout << "~handler" << std::endl;
	}

	void handler::onLoad() {
		std::cout << ">> onLoad handler executed" << std::endl;
		const std::string logger_component_name = context()->getConfig()->asString(context()->getComponentXPath() + "/logger");
		m_logger = context()->findComponent<fastcgi::Logger>(logger_component_name);
		if(!m_logger) {
			throw std::runtime_error("cannot get component " + logger_component_name);
		}

		m_provider = history::create_provider(consts::REMOTE_ADDR, consts::REMOTE_PORT, consts::REMOTE_FAMILY);

		std::vector<int> groups;
		groups.push_back(consts::REMOTE_GROUPS);

		m_provider->set_session_parameters(groups, groups.size());

		std::cout << "<< onLoad" << std::endl;
	}

	void handler::onUnload() {
		std::cout << "onUnload handler executed" << std::endl;

		m_provider.reset();
	}

	void handler::handleRequest(fastcgi::Request* req, fastcgi::HandlerContext* context) {
		fastcgi::RequestStream stream(req);
		if(req->getURI() == std::string("/")) {
			handle_root(req, context);
		}
		else if(req->getURI() == std::string("/test")) {
			handle_test(req, context);
		}
		else if(req->getURI() == std::string("/add_activity")) {
			handle_add_activity(req, context);
		}
		else if(req->getURI() == std::string("/get_active_users")) {
			handle_get_active_users(req, context);
		}
		else if(req->getURI() == std::string("/get_user_logs")) {
			handle_get_user_logs(req, context);
		}
		else {
			handle_wrong_uri(req, context);
		}

		stream << "Request info:<br>" << std::endl;
		stream << "getUrl: " << req->getUrl() << "<br>" << std::endl;
		stream << "getQueryString " << req->getQueryString() << "<br>" << std::endl;
		stream << "getRequestMethod " << req->getRequestMethod() << "<br>" << std::endl;
		stream << "getURI " << req->getURI() << "<br>" << std::endl;
		stream << "getPathInfo " << req->getPathInfo() << "<br>" << std::endl;
		stream << "countHeaders " << req->countHeaders() << "<br>" << std::endl;
		stream << "countCookie " << req->countCookie() << "<br>" << std::endl;
		stream << "countArgs: " << req->countArgs() << "<br>" << std::endl;
		std::vector<std::string> args;
		req->argNames(args);

		for(auto it = args.begin(); it != args.end(); ++it) {
			stream << *it << " = " << req->getArg(*it) << "<br>" << std::endl;
		}

		stream << "<br>" << std::endl;
		args.clear();
		req->remoteFiles(args);
		for(auto it = args.begin(); it != args.end(); ++it) {
			stream << *it << "<br>" << std::endl;
		}
	}
	void handler::handle_root(fastcgi::Request* req, fastcgi::HandlerContext*)
	{
		fastcgi::RequestStream stream(req);
		stream << "<body>" << std::endl;
		stream << "<form name=\"add\" action=\"add_activity\" method=\"post\"><br>" << std::endl;
		stream << "User name: <input type=\"text\" name=\"user\" value=\"WebUser1\"><br>" << std::endl;
		stream << "Timestamp: <input type=\"text\" name=\"timestamp\" value=\"\"><br>" << std::endl;
		stream << "Data: <input type=\"file\" name=\"data\" value=\"\"><br>" << std::endl;
		stream << "Key: <input type=\"text\" name=\"key\" valude=\"\"><br>" << std::endl;
		stream << "<input type=\"submit\" value=\"Send\">" << std::endl;
		stream << "</form>" << std::endl;

		stream << "<form name=\"get_user\" action=\"get_active_users\" method=\"post\"><br>" << std::endl;
		stream << "Timestamp: <input type=\"text\" name=\"timestamp\" value=\"\"><br>" << std::endl;
		stream << "Key: <input type=\"text\" name=\"key\" valude=\"\"><br>" << std::endl;
		stream << "<input type=\"submit\" value=\"Send\">" << std::endl;
		stream << "</form>" << std::endl;

		stream << "<form name=\"get_logs\" action=\"get_user_logs\" method=\"post\"><br>" << std::endl;
		stream << "User name: <input type=\"text\" name=\"user\" value=\"WebUser1\"><br>" << std::endl;
		stream << "Begin timestamp: <input type=\"text\" name=\"begin_time\" value=\"\"><br>" << std::endl;
		stream << "End timestamp: <input type=\"text\" name=\"end_time\" value=\"\"><br>" << std::endl;
		stream << "<input type=\"submit\" value=\"Send\">" << std::endl;
		stream << "</form>" << std::endl;

		stream << "</body>" << std::endl;

		req->setStatus(200);
	}

	void handler::handle_wrong_uri(fastcgi::Request* req, fastcgi::HandlerContext* context)
	{
		std::cout << "getURI " << req->getURI() << std::endl;
		req->setStatus(404);
	}

	void handler::handle_add_activity(fastcgi::Request* req, fastcgi::HandlerContext* context)
	{
		fastcgi::RequestStream stream(req);

		auto file = req->remoteFile("data");
		std::string data("Beeer");
		//file.toString(data);

		/*if(!req->hasFile(req->getArg("data"))) {
			req->setStatus(404);
			stream << "no data" << std::endl;
			return;
		}*/

		if(!req->hasArg("user")) {
			req->setStatus(404);
			stream << "no user" << std::endl;
			return;
		}

		auto user = req->getArg("user");

		std::string key = std::string();
		if(req->hasArg("key"))
			key = req->getArg("key");

		uint64_t tm = time(NULL);
		if(req->hasArg("timestamp"))
			tm = boost::lexical_cast<uint64_t>(req->getArg("timestamp"));

		std::cout << "Add user activity " << user << " " << tm << " " << data.size() << " " << key << std::endl;

		m_provider->add_user_activity(user, tm, &data.front(), data.size(), key);

		req->setStatus(200);
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
			req->setStatus(404);
			stream << "no key or timestamp" << std::endl;
			return;
		}

		std::map<std::string, uint32_t> res;
		if(!key.empty())
			res = m_provider->get_active_users(key);
		else
			res = m_provider->get_active_users(boost::lexical_cast<uint64_t>(timestamp));

		for(auto it = res.begin(); it != res.end(); ++it) {
			stream << it->first << " " << it->second << " <br>" << std::endl;
		}

		req->setStatus(200);
	}

	void handler::handle_get_user_logs(fastcgi::Request* req, fastcgi::HandlerContext* context)
	{
		req->setStatus(200);
	}

	void handler::handle_test(fastcgi::Request* req, fastcgi::HandlerContext* context)
	{
		req->setStatus(200);
	}


	FCGIDAEMON_REGISTER_FACTORIES_BEGIN()
		FCGIDAEMON_ADD_DEFAULT_FACTORY("historydb", handler)
	FCGIDAEMON_REGISTER_FACTORIES_END()

} } /* namespace history { namespace fastcgi */
