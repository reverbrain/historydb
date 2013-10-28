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

#ifndef HISTORY_SRC_THEVOID_ON_ADD_LOG_H
#define HISTORY_SRC_THEVOID_ON_ADD_LOG_H

#include "webserver.h"

namespace history {

	struct on_add_log :
		public ioremap::thevoid::simple_request_stream<webserver>,
		public std::enable_shared_from_this<on_add_log>
	{
		virtual void on_request(const ioremap::swarm::network_request &req,
		                        const boost::asio::const_buffer &buffer);
		void on_finish(bool added);
		virtual void on_close(const boost::system::error_code &) {}
	};

} /* namespace history */

#endif //HISTORY_SRC_THEVOID_ON_ADD_LOG_H
