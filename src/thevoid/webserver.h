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

#ifndef HISTORY_SRC_THEVOID_WEBSERVER_H
#define HISTORY_SRC_THEVOID_WEBSERVER_H

#include <thevoid/server.hpp>

namespace history {
class provider;

class webserver : public ioremap::thevoid::server<webserver>
{
public:
	webserver();
	virtual bool initialize(const rapidjson::Value &config);

	struct on_root : public ioremap::thevoid::simple_request_stream<webserver>
	{
		virtual void on_request(const ioremap::swarm::network_request &req,
                                const boost::asio::const_buffer &buffer);
		virtual void on_close(const boost::system::error_code &) {}
	};

	std::shared_ptr<provider> get_provider() { return provider_; }

private:
	std::shared_ptr<provider> provider_;
};

} /* namespace history */

#endif //HISTORY_SRC_THEVOID_WEBSERVER_H
