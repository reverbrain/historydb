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

#ifndef HISTORY_FCGI_HANDLER_H
#define HISTORY_FCGI_HANDLER_H

#include <fastcgi2/handler.h>
#include <fastcgi2/component.h>

#include <memory>
#include <map>

namespace fastcgi {
	class ComponentContext;
	class Request;
	class HandlerContext;
	class Logger;
}

namespace history {
	class provider;
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
		void init_handlers(); // inits handlers map match handle function to script namespace

		void handle_root(fastcgi::Request* req, fastcgi::HandlerContext* context); // handle request to root path
		void handle_wrong_uri(fastcgi::Request* req, fastcgi::HandlerContext* context); // handle request to unknown path

		void handle_add_log(fastcgi::Request* req, fastcgi::HandlerContext* context);
		void handle_add_activity(fastcgi::Request* req, fastcgi::HandlerContext* context);
		void handle_add_log_with_activity(fastcgi::Request* req, fastcgi::HandlerContext* context);
		void handle_get_active_users(fastcgi::Request* req, fastcgi::HandlerContext* context); // handle get active user request
		void handle_get_user_logs(fastcgi::Request* req, fastcgi::HandlerContext* context); // handle get user logs request

		fastcgi::Logger*	m_logger;
		std::shared_ptr<history::provider>	m_provider;

		std::map<std::string,
		         std::function<void(fastcgi::Request* req, fastcgi::HandlerContext* context)>
		        >	m_handlers;
	};

} } /* namespace history { namespace fastcgi */

#endif //HISTORY_FCGI_HANDLER_H
