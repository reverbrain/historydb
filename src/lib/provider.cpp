#include "provider.h"

#include <boost/algorithm/string.hpp>

#include <vector>
#include <time.h>
#include <msgpack.hpp>
#include <iomanip>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "features.h"

//log macro adds HDB: prefix to each log record
#define LOG(l, ...) dnet_log_raw(m_context.node.get_native(), l, "HDB: "__VA_ARGS__)

namespace history {

std::shared_ptr<iprovider> create_provider(const std::vector<server_info>& servers, const std::string& log_file, const int log_level)
{
	return std::make_shared<provider>(servers, log_file, log_level);
}

std::shared_ptr<iprovider> create_provider(const std::vector<std::string>& servers, const std::string& log_file, const int log_level)
{
	return std::make_shared<provider>(servers, log_file, log_level);
}

int get_log_level(const std::string& log_level)
{
			if (boost::iequals(log_level,	"DATA"))	return DNET_LOG_DATA;
	else	if (boost::iequals(log_level,	"ERROR"))	return DNET_LOG_ERROR;
	else	if (boost::iequals(log_level,	"INFO"))	return DNET_LOG_INFO;
	else	if (boost::iequals(log_level,	"NOTICE"))	return DNET_LOG_NOTICE;
	else	if (boost::iequals(log_level,	"DEBUG"))	return DNET_LOG_DEBUG;
	return DNET_LOG_ERROR;
}

dnet_config create_config()
{
	dnet_config config;
	memset(&config, 0, sizeof(config));

	config.io_thread_num = 100;
	config.nonblocking_io_thread_num = 100;
	config.net_thread_num = 16;

	return config;
}

provider::context::context(const std::string& log_file, const int log_level)
: config(create_config())
, log(log_file.c_str(), log_level)
, node(log, config)
{
	generator.seed(time(NULL));
}

provider::provider(const std::vector<server_info>& servers, const std::string& log_file, const int log_level)
: m_context(log_file, log_level)
{
	for(auto it = servers.begin(), itEnd = servers.end(); it != itEnd; ++it) {
		m_context.node.add_remote(it->addr.c_str(), it->port, it->family);	// Adds connection parameters to the node.
		LOG(DNET_LOG_INFO, "Added remote for %s:%d:%d\n", it->addr.c_str(), it->port, it->family);
	}

	LOG(DNET_LOG_INFO, "Provider has been created\n");
}

provider::provider(const std::vector<std::string>& servers, const std::string& log_file, const int log_level)
: m_context(log_file, log_level)
{
	for(auto it = servers.begin(), itEnd = servers.end(); it != itEnd; ++it) {
		m_context.node.add_remote(it->c_str());	// Adds connection parameters to the node.
		LOG(DNET_LOG_INFO, "Added remote for %s\n", it->c_str());
	}

	LOG(DNET_LOG_INFO, "Provider has been created\n");
}

provider::~provider()
{
	LOG(DNET_LOG_INFO, "Destroyed provder\n");
}

void provider::set_session_parameters(const std::vector<int>& groups, uint32_t min_writes)
{
	m_context.groups = groups;
	m_context.min_writes = min_writes > m_context.groups.size() ? m_context.groups.size() : min_writes;

	LOG(DNET_LOG_INFO, "Session parameters:\n");
	for (auto it = m_context.groups.begin(), it_end = m_context.groups.end(); it != it_end; ++it)
		LOG(DNET_LOG_INFO, "group:  %d\n", *it);
	LOG(DNET_LOG_INFO, "minimum number of successfull writes: %d\n", min_writes);
}

void provider::add_log(const std::string& user, uint64_t timestamp, const void* data, uint32_t size)
{
	features f(m_context);
	f.add_log(user, timestamp, data, size);
}

void provider::add_log(const std::string& user, uint64_t timestamp, const void* data, uint32_t size, std::function<void(bool added)> callback)
{
	features f(m_context);
	f.add_log(user, timestamp, data, size, callback);
}

void provider::add_activity(const std::string& user, uint64_t timestamp, const std::string& key)
{
	features f(m_context);
	f.add_activity(user, timestamp, key);
}

void provider::add_activity(const std::string& user, uint64_t timestamp, std::function<void(bool added)> callback, const std::string& key)
{
	features f(m_context);
	f.add_activity(user, timestamp, callback, key);
}

void provider::add_user_activity(const std::string& user, uint64_t time, const void* data, uint32_t size, const std::string& key)
{
	features f(m_context);
	f.add_user_activity(user, time, data, size, key);
}

void provider::add_user_activity(const std::string& user, uint64_t time, const void* data, uint32_t size, std::function<void(bool log_writed, bool statistics_updated)> func, const std::string& key)
{
	make_features()->add_user_activity(user, time, data, size, func, key);
}

/*void provider::repartition_activity(const std::string& key, uint32_t chunks)
{
	features f(m_context);
	f.repartition_activity(key, chunks);
}

void provider::repartition_activity(const std::string& old_key, const std::string& new_key, uint32_t chunks)
{
	features f(m_context);
	f.repartition_activity(old_key, new_key, chunks);
}

void provider::repartition_activity(uint64_t time, uint32_t chunks)
{
	features f(m_context);
	f.repartition_activity(time, chunks);
}

void provider::repartition_activity(uint64_t time, const std::string& new_key, uint32_t chunks)
{
	features f(m_context);
	f.repartition_activity(time, new_key, chunks);
}*/

std::list<std::vector<char>> provider::get_user_logs(const std::string& user, uint64_t begin_time, uint64_t end_time)
{
	features f(m_context);
	return f.get_user_logs(user, begin_time, end_time);
}

std::set<std::string> provider::get_active_users(uint64_t time)
{
	features f(m_context);
	return f.get_active_users(time);
}

std::set<std::string> provider::get_active_users(const std::string& key)
{
	features f(m_context);
	return f.get_active_users(key);
}

void provider::for_user_logs(const std::string& user, uint64_t begin_time, uint64_t end_time,
		std::function<bool(uint64_t time, void* data, uint32_t size)> func)
{
	features f(m_context);
	f.for_user_logs(user, begin_time, end_time, func);
}

void provider::for_active_users(uint64_t time, std::function<bool(const std::string& user)> func)
{
	features f(m_context);
	f.for_active_users(time, func);
}

void provider::for_active_users(const std::string& key, std::function<bool(const std::string& user)> func)
{
	features f(m_context);
	f.for_active_users(key, func);
}

std::shared_ptr<features> provider::make_features()
{
	auto f = std::make_shared<features>(m_context);
	f->set_self(f);

	return f;
}

} /* namespace history */
