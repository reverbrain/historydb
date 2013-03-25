#include "historydb-fastcgi.h"
#include <iostream>
#include <stdexcept>

#include <fastcgi2/logger.h>
#include <fastcgi2/config.h>
#include <fastcgi2/stream.h>
#include <fastcgi2/request.h>
#include <fastcgi2/component_factory.h>

#include <iprovider.h>

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
		}
		else {
			handle_wrong_uri(req, context);
		}

		stream << "Hello, World!!!" << std::endl;
		stream << "getUrl: " << req->getUrl() << std::endl;
		stream << "countArgs: " << req->countArgs() << std::endl;
		stream << "getQueryString " << req->getQueryString() << std::endl;
		stream << "getRequestMethod " << req->getRequestMethod() << std::endl;
		stream << "getURI " << req->getURI() << std::endl;
		stream << "getPathInfo " << req->getPathInfo() << std::endl;
		stream << "countHeaders " << req->countHeaders() << std::endl;
		stream << "countCookie " << req->countCookie() << std::endl;
	}
	void handler::handle_root(fastcgi::Request* req, fastcgi::HandlerContext* context)
	{

	}

	void handler::handle_wrong_uri(fastcgi::Request* req, fastcgi::HandlerContext* context)
	{
		std::cout << "getURI " << req->getURI() << std::endl;
		req->setStatus(404);
	}


	FCGIDAEMON_REGISTER_FACTORIES_BEGIN()
		FCGIDAEMON_ADD_DEFAULT_FACTORY("historydb", handler)
	FCGIDAEMON_REGISTER_FACTORIES_END()

} } /* namespace history { namespace fastcgi */