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

#include "historydb/provider.h"
#include "provider_impl.cpp"

#include <boost/algorithm/string.hpp>

namespace history {

namespace consts {
	const uint32_t SECONDS_IN_DAY = 24 * 60 * 60; // number of seconds in one day. used for calculation days
}

std::string time_to_subkey(uint64_t time)
{
	return boost::lexical_cast<std::string>(time / consts::SECONDS_IN_DAY);
}

std::vector<std::string> time_period_to_subkeys(uint64_t begin_time, uint64_t end_time)
{
	uint64_t begin = begin_time / consts::SECONDS_IN_DAY;
	uint64_t end = (end_time / consts::SECONDS_IN_DAY) + 1;

	std::vector<std::string> ret;

	if(end > begin)
		ret.reserve(end - begin);

	for(; begin < end; ++begin) {
		ret.emplace_back(boost::lexical_cast<std::string>(begin));
	}

	return ret;
}

provider::provider(const std::vector<server_info> &servers,
                   const std::vector<int> &groups,
                   uint32_t min_writes,
                   const std::string &log_file,
                   const int log_level)
: m_impl(std::make_shared<impl>(servers, groups, min_writes, log_file, log_level))
{}

provider::provider(const std::vector<std::string> &servers,
                   const std::vector<int> &groups,
                   uint32_t min_writes,
                   const std::string &log_file,
                   const int log_level)
: m_impl(std::make_shared<impl>(servers, groups, min_writes, log_file, log_level))
{}

void provider::set_session_parameters(const std::vector<int> &groups,
                                      uint32_t min_writes)
{
	m_impl->set_session_parameters(groups, min_writes);
}

void provider::add_log(const std::string &user,
                       uint64_t time,
                       const void *data, size_t size)
{
	m_impl->add_log(user, time_to_subkey(time), data, size);
}

void provider::add_log(const std::string &user,
                       uint64_t time,
                       const std::string &data)
{
	m_impl->add_log(user, time_to_subkey(time), data.data(), data.size());
}

void provider::add_log(const std::string &user,
                       uint64_t time,
                       const std::vector<char> &data)
{
	m_impl->add_log(user, time_to_subkey(time), data.data(), data.size());
}

void provider::add_log(const std::string &user,
                       const std::string &subkey,
                       const void *data, size_t size)
{
	m_impl->add_log(user, subkey, data, size);
}

void provider::add_log(const std::string &user,
                       const std::string &subkey,
                       const std::string &data)
{
	m_impl->add_log(user, subkey, data.data(), data.size());
}

void provider::add_log(const std::string &user,
                       const std::string &subkey,
                       const std::vector<char> &data)
{
	m_impl->add_log(user, subkey, data.data(), data.size());
}

void provider::add_log(const std::string &user,
                       uint64_t time,
                       const void *data, size_t size,
                       std::function<void(bool added)> callback)
{
	m_impl->add_log(user, time_to_subkey(time), data, size, callback);
}

void provider::add_log(const std::string &user,
                       uint64_t time,
                       const std::string &data,
                       std::function<void(bool added)> callback)
{
	m_impl->add_log(user, time_to_subkey(time), data.data(), data.size(), callback);
}

void provider::add_log(const std::string &user,
                       uint64_t time,
                       const std::vector<char> &data,
                       std::function<void(bool added)> callback)
{
	m_impl->add_log(user, time_to_subkey(time), data.data(), data.size(), callback);
}

void provider::add_log(const std::string &user,
                       const std::string &subkey,
                       const void *data, size_t size,
                       std::function<void(bool added)> callback)
{
	m_impl->add_log(user, subkey, data, size, callback);
}

void provider::add_log(const std::string &user,
                       const std::string &subkey,
                       const std::string &data,
                       std::function<void(bool added)> callback)
{
	m_impl->add_log(user, subkey, data.data(), data.size(), callback);
}

void provider::add_log(const std::string &user,
                       const std::string &subkey,
                       const std::vector<char> &data,
                       std::function<void(bool added)> callback)
{
	m_impl->add_log(user, subkey, data.data(), data.size(), callback);
}

void provider::add_activity(const std::string &user, uint64_t time)
{
	m_impl->add_activity(user, time_to_subkey(time));
}

void provider::add_activity(const std::string &user, const std::string &subkey)
{
	m_impl->add_activity(user, subkey);
}

void provider::add_activity(const std::string &user,
                            uint64_t time,
                            std::function<void(bool added)> callback)
{
	m_impl->add_activity(user, time_to_subkey(time), callback);
}

void provider::add_activity(const std::string &user,
                            const std::string &subkey,
                            std::function<void(bool added)> callback)
{
	m_impl->add_activity(user, subkey, callback);
}

void provider::add_log_with_activity(const std::string &user,
                                     uint64_t time,
                                     const void *data, size_t size)
{
	m_impl->add_log_with_activity(user, time_to_subkey(time), data, size);
}

void provider::add_log_with_activity(const std::string &user,
                                     uint64_t time,
                                     const std::string &data)
{
	m_impl->add_log_with_activity(user, time_to_subkey(time), data.data(), data.size());
}

void provider::add_log_with_activity(const std::string &user,
                                     uint64_t time,
                                     const std::vector<char> &data)
{
	m_impl->add_log_with_activity(user, time_to_subkey(time), data.data(), data.size());
}

void provider::add_log_with_activity(const std::string &user,
                                     const std::string &subkey,
                                     const void *data, size_t size)
{
	m_impl->add_log_with_activity(user, subkey, data, size);
}

void provider::add_log_with_activity(const std::string &user,
                                     const std::string &subkey,
                                     const std::string &data)
{
	m_impl->add_log_with_activity(user, subkey, data.data(), data.size());
}

void provider::add_log_with_activity(const std::string &user,
                                     const std::string &subkey,
                                     const std::vector<char> &data)
{
	m_impl->add_log_with_activity(user, subkey, data.data(), data.size());
}

void provider::add_log_with_activity(const std::string &user,
                                     uint64_t time,
                                     const void *data, size_t size,
                                     std::function<void(bool added)> callback)
{
	m_impl->add_log_with_activity(user, time_to_subkey(time), data, size, callback);
}

void provider::add_log_with_activity(const std::string &user,
                                     uint64_t time,
                                     const std::string &data,
                                     std::function<void(bool added)> callback)
{
	m_impl->add_log_with_activity(user, time_to_subkey(time), data.data(), data.size(), callback);
}

void provider::add_log_with_activity(const std::string &user,
                                     uint64_t time,
                                     const std::vector<char> &data,
                                     std::function<void(bool added)> callback)
{
	m_impl->add_log_with_activity(user, time_to_subkey(time), data.data(), data.size(), callback);
}

void provider::add_log_with_activity(const std::string &user,
                                     const std::string &subkey,
                                     const void *data, size_t size,
                                     std::function<void(bool added)> callback)
{
	m_impl->add_log_with_activity(user, subkey, data, size, callback);
}

void provider::add_log_with_activity(const std::string &user,
                                     const std::string &subkey,
                                     const std::string &data,
                                     std::function<void(bool added)> callback)
{
	m_impl->add_log_with_activity(user, subkey, data.data(), data.size(), callback);
}

void provider::add_log_with_activity(const std::string &user,
                                     const std::string &subkey,
                                     const std::vector<char> &data,
                                     std::function<void(bool added)> callback)
{
	m_impl->add_log_with_activity(user, subkey, data.data(), data.size(), callback);
}

std::vector<char> provider::get_user_logs(const std::string &user,
                                          uint64_t begin_time,
                                          uint64_t end_time)
{
	return m_impl->get_user_logs(user, time_period_to_subkeys(begin_time, end_time));
}

std::vector<char> provider::get_user_logs(const std::string &user,
                                          const std::vector<std::string> &subkeys)
{
	return m_impl->get_user_logs(user, subkeys);
}

void provider::get_user_logs(const std::string &user,
                             uint64_t begin_time,
                             uint64_t end_time,
                             std::function<void(const std::vector<char> &data)> callback)
{
	m_impl->get_user_logs(user,
	                     time_period_to_subkeys(begin_time, end_time),
	                     callback);
}

void provider::get_user_logs(const std::string &user,
                             const std::vector<std::string> &subkeys,
                             std::function<void(const std::vector<char> &data)> callback)
{
	m_impl->get_user_logs(user, subkeys, callback);
}

std::set<std::string> provider::get_active_users(uint64_t begin_time, uint64_t end_time)
{
	return m_impl->get_active_users(time_period_to_subkeys(begin_time, end_time));
}

std::set<std::string> provider::get_active_users(const std::vector<std::string> &subkeys)
{
	return m_impl->get_active_users(subkeys);
}

void provider::get_active_users(uint64_t begin_time,
                                uint64_t end_time,
                                std::function<void(const std::set<std::string>  &ctive_users)> callback)
{
	m_impl->get_active_users(time_period_to_subkeys(begin_time, end_time), callback);
}

void provider::get_active_users(const std::vector<std::string> &subkeys,
                                std::function<void(const std::set<std::string>  &ctive_users)> callback)
{
	m_impl->get_active_users(subkeys, callback);
}


void provider::for_user_logs(const std::string &user,
                             uint64_t begin_time,
                             uint64_t end_time,
                             std::function<bool(const std::vector<char> &data)> callback)
{
	m_impl->for_user_logs(user, time_period_to_subkeys(begin_time, end_time), callback);
}

void provider::for_user_logs(const std::string &user,
                             const std::vector<std::string> &subkeys,
                             std::function<bool(const std::vector<char> &data)> callback)
{
	m_impl->for_user_logs(user, subkeys, callback);
}

void provider::for_active_users(uint64_t begin_time,
                                uint64_t end_time,
                                std::function<bool(const std::set<std::string> &active_users)> callback)
{
	m_impl->for_active_users(time_period_to_subkeys(begin_time, end_time), callback);
}

void provider::for_active_users(const std::vector<std::string> &subkeys,
                                std::function<bool(const std::set<std::string> &active_users)> callback)
{
	m_impl->for_active_users(subkeys, callback);
}


int get_log_level(const std::string &log_level)
{
			if (boost::iequals(log_level,	"DATA"))	return DNET_LOG_DATA;
	else	if (boost::iequals(log_level,	"ERROR"))	return DNET_LOG_ERROR;
	else	if (boost::iequals(log_level,	"INFO"))	return DNET_LOG_INFO;
	else	if (boost::iequals(log_level,	"NOTICE"))	return DNET_LOG_NOTICE;
	else	if (boost::iequals(log_level,	"DEBUG"))	return DNET_LOG_DEBUG;
	return DNET_LOG_ERROR;
}

} /* namespace history */
