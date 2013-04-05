#ifndef HISTORY_FCGI_HANDLER_H
#define HISTORY_FCGI_HANDLER_H

#include <fastcgi2/handler.h>
#include <fastcgi2/component.h>

#include <memory>

namespace fastcgi {
	class ComponentContext;
	class Request;
	class HandlerContext;
	class Logger;
}

namespace history {
	class iprovider;
namespace fcgi {

	class handler : virtual public fastcgi::Component, virtual public fastcgi::Handler
	{
	public:
		handler(fastcgi::ComponentContext* context);
		virtual ~handler();

		virtual void onLoad();
		virtual void onUnload();

		virtual void handleRequest(fastcgi::Request* req, fastcgi::HandlerContext* context);

	private:
		void handle_root(fastcgi::Request* req, fastcgi::HandlerContext* context);
		void handle_wrong_uri(fastcgi::Request* req, fastcgi::HandlerContext* context);
		void handle_test(fastcgi::Request* req, fastcgi::HandlerContext* context);

		void handle_add_activity(fastcgi::Request* req, fastcgi::HandlerContext* context);
		void handle_get_active_users(fastcgi::Request* req, fastcgi::HandlerContext* context);
		void handle_get_user_logs(fastcgi::Request* req, fastcgi::HandlerContext* context);

		void write_header(fastcgi::Request* req);
		void close_html(fastcgi::Request* req);

		fastcgi::Logger*					m_logger;
		std::shared_ptr<history::iprovider>	m_provider;
	};

} } /* namespace history { namespace fastcgi */

#endif //HISTORY_FCGI_HANDLER_H
