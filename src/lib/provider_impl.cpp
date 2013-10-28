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

#include <elliptics/cppdef.h>

#include <functional>
#include <deque>

#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/make_shared.hpp>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#define LOG(l, ...) dnet_log_raw(node_.get_native(), l, "HDB: "__VA_ARGS__)

namespace history {

namespace consts {
	const uint32_t TIMEOUT = 60; // timeout for node configuration and session
}

struct waiter
{
	waiter(std::function<void(bool added)> callback,
	       ioremap::elliptics::node &node,
	       uint32_t min_writes,
	       bool log_init = false,
	       bool activity_init = false)
	: log_completed(log_init)
	, activity_completed(activity_init)
	, result_(true)
	, callback_(callback)
	, node_(node)
	, min_writes_(min_writes)
	{}

	void on_log(const ioremap::elliptics::sync_write_result &res,
	            const ioremap::elliptics::error_info &error) {
		boost::mutex::scoped_lock lock(mutex_);
		if (res.size() < min_writes_) {
			LOG(DNET_LOG_ERROR, "Can't write data to the minimum number of groups while appending data to user log error: %s\n", error.message().c_str());
			result_ = false;
		}

		log_completed = true;

		handle();
	}

	void on_activity(const ioremap::elliptics::sync_set_indexes_result &res,
	                 const ioremap::elliptics::error_info &error) {
		boost::mutex::scoped_lock lock(mutex_);
		if (res.size() < min_writes_) {
			LOG(DNET_LOG_ERROR, "Can't write data while adding activity error: %s\n", error.message().c_str());
			result_ = false;
		}

		activity_completed = true;

		handle();
	}


private:

	void handle() {
		if (log_completed && activity_completed)
			callback_(result_);
	}

	bool log_completed;
	bool activity_completed;
	bool result_;
	boost::mutex mutex_;
	std::function<void(bool added)> callback_;
	ioremap::elliptics::node &node_; // elliptics node
	uint32_t min_writes_;
};

class provider::impl : public std::enable_shared_from_this<provider::impl>
{
public:
	impl(const std::vector<server_info>& servers,
	     const std::vector<int>& groups,
	     uint32_t min_writes,
	     const std::string& log_file,
	     const int log_level);
	impl(const std::vector<std::string>& servers,
	     const std::vector<int>& groups,
	     uint32_t min_writes,
	     const std::string& log_file,
	     const int log_level);

	void set_session_parameters(const std::vector<int>& groups, uint32_t min_writes);

	void add_log(const std::string& user,
	             const std::string& subkey,
	             const void *data, size_t size);
	void add_log(const std::string& user,
	             const std::string& subkey,
	             const void *data, size_t size,
	             std::function<void(bool added)> callback);

	void add_activity(const std::string& user, const std::string& subkey);
	void add_activity(const std::string& user,
	                  const std::string& subkey,
	                  std::function<void(bool added)> callback);

	void add_log_with_activity(const std::string& user,
	                           const std::string& subkey,
	                           const void *data, size_t size);
	void add_log_with_activity(const std::string& user,
	                           const std::string& subkey,
	                           const void *data, size_t size,
	                           std::function<void(bool added)> callback);

	std::vector<char> get_user_logs(const std::string& user,
	                                const std::vector<std::string>& subkeys);
	void get_user_logs(const std::string& user,
	                   const std::vector<std::string>& subkeys,
	                   std::function<void(const std::vector<char>& data)> callback);

	std::set<std::string> get_active_users(const std::vector<std::string>& subkeys);
	void get_active_users(const std::vector<std::string>& subkeys,
	                      std::function<void(const std::set<std::string> &active_users)> callback);

	void for_user_logs(const std::string& user,
	                   const std::vector<std::string>& subkeys,
	                   std::function<bool(const std::vector<char>& data)> callback);

	void for_active_users(const std::vector<std::string>& subkeys,
	                      std::function<bool(const std::set<std::string>& active_users)> callback);

private:
	ioremap::elliptics::session create_session(uint32_t io_flags = 0) const;

	ioremap::elliptics::async_write_result
	add_log(ioremap::elliptics::session& s,
	        const std::string& user,
	        const std::string& subkey,
	        const void *data, size_t size);
	ioremap::elliptics::async_set_indexes_result
	add_activity(ioremap::elliptics::session& s,
	             const std::string& user,
	             const std::string& subkey);
	ioremap::elliptics::async_find_indexes_result
	get_active_users(ioremap::elliptics::session& s,
	                 const std::vector<std::string>& subkeys);

	static void on_user_log(std::shared_ptr<std::list<ioremap::elliptics::async_read_result>> results,
							std::shared_ptr<std::deque<char>> data,
							std::function<void(const std::vector<char>& data)> callback,
							const ioremap::elliptics::sync_read_result &entry,
							const ioremap::elliptics::error_info &error);
	static void on_active_users(std::function<void(const std::set<std::string> &active_users)> callback,
	                            const ioremap::elliptics::sync_find_indexes_result &result,
	                            const ioremap::elliptics::error_info &error);

	std::string combine_key(const std::string& user, const std::string& subkey) const;


	std::vector<int>					groups_; // groups of elliptics
	uint32_t							min_writes_; // minimum number of succeeded writes for each write attempt
	dnet_config							config_; //elliptics config
	ioremap::elliptics::file_logger		log_; // logger
	ioremap::elliptics::node			node_; // elliptics node
};

dnet_config create_config()
{
	dnet_config config;
	memset(&config, 0, sizeof(config));

	config.io_thread_num = 100;
	config.nonblocking_io_thread_num = 100;
	config.net_thread_num = 16;
	config.check_timeout = consts::TIMEOUT;
	config.wait_timeout = consts::TIMEOUT;

	return config;
}

provider::impl::impl(const std::vector<server_info>& servers,
                     const std::vector<int>& groups,
                     uint32_t min_writes,
                     const std::string& log_file,
                     const int log_level)
: groups_(groups)
, min_writes_(min_writes)
, config_(create_config())
, log_(log_file.c_str(), log_level)
, node_(log_, config_)
{
	for (auto it = servers.begin(), end = servers.end(); it != end; ++it) {
		try {
			node_.add_remote(it->addr.c_str(), it->port, it->family);	// Adds connection parameters to the node.
			LOG(DNET_LOG_INFO, "Added elliptics server: %s:%d:%d\n", it->addr.c_str(), it->port, it->family);
		}
		catch (ioremap::elliptics::error& e) {
			LOG(DNET_LOG_ERROR, "Coudn't connect to %s:%d:%d: %s\n", it->addr.c_str(), it->port, it->family, e.error_message().c_str());
		}
	}

	if (min_writes_ > groups_.size())
		min_writes_ = groups_.size();

	LOG(DNET_LOG_INFO, "provider::impl has been created\n");
}

provider::impl::impl(const std::vector<std::string>& servers,
                     const std::vector<int>& groups,
                     uint32_t min_writes,
                     const std::string& log_file,
                     const int log_level)
: groups_(groups)
, min_writes_(min_writes)
, config_(create_config())
, log_(log_file.c_str(), log_level)
, node_(log_, config_)
{
	for (auto it = servers.begin(), end = servers.end(); it != end; ++it) {
		try {
			node_.add_remote(it->c_str());	// Adds connection parameters to the node.
			LOG(DNET_LOG_INFO, "Added elliptics server: %s\n", it->c_str());
		}
		catch (ioremap::elliptics::error& e) {
			LOG(DNET_LOG_ERROR, "Coudn't connect to %s: %s\n", it->c_str(), e.error_message().c_str());
		}
	}

	if (min_writes_ > groups_.size())
		min_writes_ = groups_.size();

	LOG(DNET_LOG_INFO, "provider::impl has been created\n");
}

void provider::impl::set_session_parameters(const std::vector<int>& groups, uint32_t min_writes)
{
	groups_ = groups;
	min_writes_ = min_writes;

	if (min_writes_ > groups_.size())
		min_writes_ = groups_.size();
}

void provider::impl::add_log(const std::string& user,
                             const std::string& subkey,
                             const void *data, size_t size)
{
	auto s = create_session(DNET_IO_FLAGS_CACHE | DNET_IO_FLAGS_APPEND);

	auto res = add_log(s, user, subkey, data, size);

	if (res.get().size() < min_writes_) {
		LOG(DNET_LOG_ERROR, "Can't write data to the minimum number of groups while appending data to user log error: %s\n", res.error().message().c_str());
		throw ioremap::elliptics::error(EREMOTEIO, "Data wasn't written to the minimum number of groups");
	}
}

void provider::impl::add_log(const std::string& user,
                             const std::string& subkey,
                             const void *data, size_t size,
                             std::function<void(bool added)> callback)
{
	auto s = create_session(DNET_IO_FLAGS_CACHE | DNET_IO_FLAGS_APPEND);

	auto w = boost::make_shared<waiter>(callback, node_, min_writes_, false, true);

	add_log(s, user, subkey, data, size)
	.connect(boost::bind(&waiter::on_log,
	                     w,
	                     _1,
	                     _2));
}

void provider::impl::add_activity(const std::string& user, const std::string& subkey)
{
	auto s = create_session(DNET_IO_FLAGS_CACHE);

	auto res = add_activity(s, user, subkey);

	if (res.get().size() < min_writes_) {
		LOG(DNET_LOG_ERROR, "Can't write data while adding activity error: %s\n", res.error().message().c_str());
		throw ioremap::elliptics::error(EREMOTEIO, "Data wasn't written to the minimum number of groups");
	}
}

void provider::impl::add_activity(const std::string& user,
                                  const std::string& subkey,
                                  std::function<void(bool added)> callback)
{
	auto s = create_session(DNET_IO_FLAGS_CACHE);

	auto w = boost::make_shared<waiter>(callback, node_, min_writes_, true, false);

	add_activity(s, user, subkey)
	.connect(boost::bind(&waiter::on_activity,
	                     w,
	                     _1,
	                     _2));
}

void provider::impl::add_log_with_activity(const std::string& user,
                                           const std::string& subkey,
                                           const void *data, size_t size)
{
	auto log_s = create_session(DNET_IO_FLAGS_CACHE | DNET_IO_FLAGS_APPEND);
	auto act_s = create_session(DNET_IO_FLAGS_CACHE);

	auto log_res = add_log(log_s, user, subkey, data, size);
	auto act_res = add_activity(act_s, user, subkey);

	bool result = true;

	if (log_res.get().size() < min_writes_) {
		LOG(DNET_LOG_ERROR, "Can't write data while appending data to user log: %s\n", log_res.error().message().c_str());
		result = false;
	}

	if (act_res.get().size() < min_writes_) {
		LOG(DNET_LOG_ERROR, "Can't write data while adding activity: %s\n", act_res.error().message().c_str());
		result = false;
	}

	if (!result)
		throw ioremap::elliptics::error(EREMOTEIO, "Data wasn't written to the minimum number of groups");
}

void provider::impl::add_log_with_activity(const std::string& user,
                                           const std::string& subkey,
                                           const void *data, size_t size,
                                           std::function<void(bool added)> callback)
{
	auto w = boost::make_shared<waiter>(callback, node_, min_writes_);

	auto log_s = create_session(DNET_IO_FLAGS_CACHE | DNET_IO_FLAGS_APPEND);
	auto act_s = create_session(DNET_IO_FLAGS_CACHE);

	add_log(log_s, user, subkey, data, size)
	.connect(boost::bind(&waiter::on_log,
	                     w,
	                     _1,
	                     _2));

	add_activity(act_s, user, subkey)
	.connect(boost::bind(&waiter::on_activity,
	                     w,
	                     _1,
	                     _2));
}

std::vector<char> provider::impl::get_user_logs(const std::string& user, const std::vector<std::string>& subkeys)
{
	LOG(DNET_LOG_DEBUG, "Getting user: %s logs for keys: %d\n", user.c_str(), subkeys.size());

	std::deque<char> data;

	std::list<ioremap::elliptics::async_read_result> results;

	auto s = create_session(0);

	for (auto it = subkeys.begin(), end = subkeys.end(); it != end; ++it) {
		auto cmb_key = combine_key(user, *it);
		LOG(DNET_LOG_DEBUG, "Try to read user: %s log file: %s\n", user.c_str(), cmb_key.c_str());
		results.emplace_back(s.read_latest(cmb_key, 0, 0));
	}

	try {
		for (auto it = results.begin(), end = results.end(); it != end; ++it) {
			try {
				auto file = it->get_one().file(); // reads user log file
				if (file.empty()) // if the file is empty
					continue; // skip it and go to the next

				data.insert(data.end(), file.data<char>(), file.data<char>() + file.size());
			}
			catch (ioremap::elliptics::error& e) {
				LOG(DNET_LOG_ERROR, "Can't read log file: %s\n", e.error_message().c_str());
			}
		}
	}
	catch (ioremap::elliptics::error& e) {
		LOG(DNET_LOG_ERROR, "Error while getting user logs: %s\n", e.error_message().c_str());
	}

	return std::vector<char>(data.begin(), data.end());
}

void provider::impl::on_user_log(std::shared_ptr<std::list<ioremap::elliptics::async_read_result>> results,
                                 std::shared_ptr<std::deque<char>> data,
                                 std::function<void(const std::vector<char>& data)> callback,
                                 const ioremap::elliptics::sync_read_result &entry,
                                 const ioremap::elliptics::error_info &/*error*/)
{
	try {
		results->erase(results->begin());
		if (!entry.empty()) {
			auto file = entry.front().file();
			if (!file.empty())
				data->insert(data->end(), file.data<char>(), file.data<char>() + file.size());
		}
	}
	catch (ioremap::elliptics::error& e) {}

	if (!results->empty()) {
		results->front().connect(boost::bind(&provider::impl::on_user_log, results, data, callback, _1, _2));
	}
	else
		callback(std::vector<char>(data->begin(), data->end()));
}

void provider::impl::get_user_logs(const std::string& user,
                                   const std::vector<std::string>& subkeys,
                                   std::function<void(const std::vector<char>& data)> callback)
{
	LOG(DNET_LOG_DEBUG, "Async getting user: %s logs for keys: %d\n", user.c_str(), subkeys.size());
	auto data = std::make_shared<std::deque<char>>();

	auto results = std::make_shared<std::list<ioremap::elliptics::async_read_result>>();

	auto s = create_session(0);

	for (auto it = subkeys.begin(), end = subkeys.end(); it != end; ++it) {
		auto cmb_key = combine_key(user, *it);
		LOG(DNET_LOG_DEBUG, "Async try to read user: %s log file: %s\n", user.c_str(), cmb_key.c_str());
		results->emplace_back(s.read_latest(cmb_key, 0, 0));
	}

	results->front()
	.connect(boost::bind(&provider::impl::on_user_log,
	                     results,
	                     data,
	                     callback,
	                     _1,
	                     _2));
}

ioremap::elliptics::async_find_indexes_result
provider::impl::get_active_users(ioremap::elliptics::session& s,
                                 const std::vector<std::string>& subkeys)
{
	LOG(DNET_LOG_DEBUG, "Getting active users: %d\n", subkeys.size());
	return s.find_any_indexes(subkeys);
}

std::set<std::string> provider::impl::get_active_users(const std::vector<std::string>& subkeys)
{
	std::set<std::string> ret;

	auto s = create_session();

	auto async_result = get_active_users(s, subkeys);

	for (auto it = async_result.begin(), end = async_result.end(); it != end; ++it) {
		for (auto ind_it = it->indexes.begin(), ind_end = it->indexes.end(); ind_it != ind_end; ++ind_it) {
			ret.insert(ind_it->data.to_string());
			LOG(DNET_LOG_DEBUG, "Found value: %s\n", ret.rbegin()->c_str());
		}
	}

	return ret;
}

void provider::impl::on_active_users(std::function<void(const std::set<std::string> &active_users)> callback,
                                     const ioremap::elliptics::sync_find_indexes_result &result,
                                     const ioremap::elliptics::error_info &/*error*/)
{
	std::set<std::string> active_users;

	for (auto it = result.begin(), end = result.end(); it != end; ++it) {
		for (auto ind_it = it->indexes.begin(), ind_end = it->indexes.end(); ind_it != ind_end; ++ind_it) {
			active_users.insert(ind_it->data.to_string());
		}
	}

	callback(active_users);
}

void provider::impl::get_active_users(const std::vector<std::string>& subkeys,
                                      std::function<void(const std::set<std::string> &active_users)> callback)
{
	auto s = create_session();

	get_active_users(s, subkeys)
	.connect(boost::bind(&provider::impl::on_active_users,
	                     callback,
	                     _1,
	                     _2));
}

void provider::impl::for_user_logs(const std::string& user,
                                   const std::vector<std::string>& subkeys,
                                   std::function<bool(const std::vector<char>& data)> callback)
{
	LOG(DNET_LOG_DEBUG, "Iterate user: %s logs: %d\n", user.c_str(), subkeys.size());
	std::list<ioremap::elliptics::async_read_result> results;

	auto s = create_session(0);

	for (auto it = subkeys.begin(), end = subkeys.end(); it != end; ++it) {
		results.emplace_back(s.read_latest(combine_key(user, *it), 0, 0));
	}

	try {
		for (auto it = results.begin(), end = results.end(); it != end; ++it) {
			try {
				auto file = it->get_one().file(); // reads user log file
				if (file.empty()) // if the file is empty
					continue; // skip it and go to the next

				if (!callback(std::vector<char>(file.data<char>(), file.data<char>())))
					return;
			}
			catch (ioremap::elliptics::error& e) {
				LOG(DNET_LOG_ERROR, "Can't read log file: %s\n", e.error_message().c_str());
			}
		}
	}
	catch (ioremap::elliptics::error& e) {
		LOG(DNET_LOG_ERROR, "Error while iterating log files: %s\n", e.error_message().c_str());
	}
}

void provider::impl::for_active_users(const std::vector<std::string>& subkeys,
                                      std::function<bool(const std::set<std::string>& active_users)> callback)
{
	LOG(DNET_LOG_DEBUG, "Iterate active users: %d\n", subkeys.size());
	for (auto it = subkeys.begin(), end = subkeys.end(); it != end; ++it) {
		std::vector<std::string> one_subkey;
		one_subkey.push_back(*it);
		if (!callback(get_active_users(one_subkey)))
			return;
	}
}

ioremap::elliptics::session provider::impl::create_session(uint32_t io_flags) const
{
	auto ret = ioremap::elliptics::session(node_);

	ret.set_ioflags(io_flags);
	ret.set_cflags(0);
	ret.set_groups(groups_); // sets groups
	ret.set_exceptions_policy(ioremap::elliptics::session::exceptions_policy::no_exceptions);
	ret.set_timeout(consts::TIMEOUT);

	return ret;
}

ioremap::elliptics::async_write_result
provider::impl::add_log(ioremap::elliptics::session& s,
                        const std::string& user,
                        const std::string& subkey,
                        const void *data, size_t size)
{
	auto write_key = combine_key(user, subkey);

	LOG(DNET_LOG_DEBUG, "Try to append data to user log key: %s\n", write_key.c_str());

	auto dp = ioremap::elliptics::data_pointer::copy(data, size);

	return s.write_data(write_key, dp, 0); // write data into elliptics
}

ioremap::elliptics::async_set_indexes_result
provider::impl::add_activity(ioremap::elliptics::session& s,
                             const std::string& user,
                             const std::string& subkey)
{
	LOG(DNET_LOG_DEBUG, "Try to add user to activity statistics: %s\n", subkey.c_str());

	std::vector<std::string> indexes;
	std::vector<ioremap::elliptics::data_pointer> datas;
	indexes.push_back(subkey);
	datas.push_back(user);

	LOG(DNET_LOG_DEBUG, "Update indexes with key: %s and index: %s\n", subkey.c_str(), indexes.front().c_str());
	return s.update_indexes_internal(user, indexes, datas);
}

std::string provider::impl::combine_key(const std::string& basekey, const std::string& subkey) const
{
	return basekey + "." + subkey;
}

} /* namespace history */
