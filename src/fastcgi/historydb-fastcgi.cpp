#include <iostream>
#include <stdexcept>

#include <fastcgi2/logger.h>
#include <fastcgi2/config.h>
#include <fastcgi2/stream.h>
#include <fastcgi2/handler.h>
#include <fastcgi2/request.h>
#include <fastcgi2/component.h>
#include <fastcgi2/component_factory.h>

namespace historydb
{
	class handler : virtual public fastcgi::Component, virtual public fastcgi::Handler
	{
	public:
		handler(fastcgi::ComponentContext* context);
		virtual ~handler();

		virtual void onLoad();
		virtual void onUnload();

		virtual void handleRequest(fastcgi::Request* req, fastcgi::HandlerContext* handler_context);

	private:
		fastcgi::Logger* m_logger;
	};

	handler::handler(fastcgi::ComponentContext* context)
	: fastcgi::Component(context)
	, m_logger(NULL) {}

	handler::~handler() {}

	void handler::onLoad() {
		std::cout << "onLoad handler executed" << std::endl;
		const std::string logger_component_name = context()->getConfig()->asString(context()->getComponentXPath() + "/logger");
		m_logger = context()->findComponent<fastcgi::Logger>(logger_component_name);
		if(!m_logger) {
			throw std::runtime_error("cannot get component " + logger_component_name);
		}
	}

	void handler::onUnload() {
		std::cout << "onUnload handler executed" << std::endl;
	}

	void handler::handleRequest(fastcgi::Request* req, fastcgi::HandlerContext* handler_context) {
		fastcgi::RequestStream stream(req);
		std::cout << "User " << req->getUrl() << std::endl;
		stream << "Hello, World!!!\n";
		req->setStatus(200);
	}

	FCGIDAEMON_REGISTER_FACTORIES_BEGIN()
		FCGIDAEMON_ADD_DEFAULT_FACTORY("historydb", handler)
	FCGIDAEMON_REGISTER_FACTORIES_END()
}