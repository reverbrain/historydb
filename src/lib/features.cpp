#include "features.h"

#include <msgpack.hpp>
#include <functional>

#include <boost/thread/locks.hpp>
#include <boost/thread/thread.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>
#include <boost/lambda/lambda.hpp>

#include "keys_size_cache.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "provider.h"

//log macro adds HDB: prefix to each log record
#define LOG(l, ...) dnet_log_raw(m_context.node.get_native(), l, "HDB: "__VA_ARGS__)

namespace history {
namespace consts {
	const uint32_t	SEC_PER_DAY			= 24 * 60 * 60; // number of seconds in one day. used for calculation days
	const uint32_t	CHUNKS_COUNT		= 100; // number of chunks in user's shard
	const uint32_t	WRITES_BEFORE_FAIL	= 3; // Number of attempts of write_cas before returning fail result
} /* namespace consts */

features::features(provider::context& context)
: m_context(context)
, m_session(m_context.node)
{
	LOG(DNET_LOG_DEBUG, "Constructing features object for some action\n");
	m_session.set_cflags(0/*DNET_FLAGS_NOLOCK*/); // sets cflags to 0
	m_session.set_ioflags(0);
	m_session.set_groups(m_context.groups); // sets groups
	m_session.set_exceptions_policy(ioremap::elliptics::session::exceptions_policy::no_exceptions);
}

void features::add_log(const std::string& user, uint64_t timestamp, const void* data, uint32_t size)
{
	m_user = user;
	LOG(DNET_LOG_INFO, "Add user log: %s timestamp:%" PRIu64 " data size: %d\n", m_user.c_str(), timestamp, size);
	m_user_key = make_user_key(timestamp);

	auto res = add_user_data(data, size); // Adds data to the user log

	if(res.get().size() < m_context.min_writes) { // Checks number of successfull results and if it is less minimum then throw exception
		LOG(DNET_LOG_ERROR, "Can't write data while adding data to user log key: %s error: %s\n", m_user_key.c_str(), res.error().message().c_str());
		throw ioremap::elliptics::error(EREMOTEIO, "Data wasn't written to the minimum number of groups");
	}

	m_self.reset();
}

void features::add_log(const std::string& user, uint64_t timestamp, const void* data, uint32_t size, std::function<void(bool added)> callback)
{
	m_user = user;
	LOG(DNET_LOG_INFO, "Add user log: %s timestamp:%" PRIu64 " data size: %d\n", m_user.c_str(), timestamp, size);

	m_user_key = make_user_key(timestamp);
	m_add_log_callback = callback;

	auto res = add_user_data(data, size); // Adds data to the user log

	res.connect(boost::bind(callback,
							(boost::bind(&ioremap::elliptics::sync_write_result::size, _1) >= m_context.min_writes)));
}

void features::add_activity(const std::string& user, uint64_t timestamp, const std::string& key)
{
	m_user = user;
	LOG(DNET_LOG_INFO, "Add user activity: %s timestamp:%" PRIu64 " custom key: %s\n", m_user.c_str(), timestamp, key.c_str());

	m_user_key = make_user_key(timestamp);

	m_activity_key = key.empty() ? make_key(timestamp) : key;

	auto res = update_activity(); // updatea user activity

	if (res.get().size() < m_context.min_writes) { // Checks number of successfull results and if it is less minimum then throw exception
		LOG(DNET_LOG_ERROR, "Can't write data while updating activity for key: %s error: %s\n", m_chunk_key.c_str(), res.error().message().c_str());
		throw ioremap::elliptics::error(EREMOTEIO, "Data wasn't written to the minimum number of groups");
	}
	m_self.reset();
}

void features::add_activity(const std::string& user, uint64_t timestamp, std::function<void(bool added)> callback, const std::string& key)
{
	m_user = user;
	LOG(DNET_LOG_INFO, "Add user activity: %s timestamp:%" PRIu64 " custom key: %s\n", m_user.c_str(), timestamp, key.c_str());

	m_user_key = make_user_key(timestamp);
	m_add_activity_callback = callback;

	m_activity_key = key.empty() ? make_key(timestamp) : key;

	auto res = update_activity(); // updates user activity

	res.connect(boost::bind(callback,
							(boost::bind(&ioremap::elliptics::sync_update_indexes_result::size, _1) >= m_context.min_writes)));
}

void features::add_user_activity(const std::string& user, uint64_t time, const void* data, uint32_t size, const std::string& key)
{
	m_user = user;
	LOG(DNET_LOG_INFO, "Add activity user: %s time: %" PRIu64 " data size: %d custom key: %s\n", m_user.c_str(), time, size, key.c_str());

	m_user_key = make_user_key(time);

	m_activity_key = key.empty() ? make_key(time) : key;

	auto add_res = add_user_data(data, size); // Adds data to the user log
	auto update_res = update_activity();

	if (add_res.get().size() < m_context.min_writes) { // Checks number of successfull results and if it is less minimum then throw exception
		LOG(DNET_LOG_ERROR, "Can't write data while adding data to user log key: %s error: %s\n", m_user_key.c_str(), add_res.error().message().c_str());
		throw ioremap::elliptics::error(EREMOTEIO, "Data wasn't written to the minimum number of groups");
	}

	if (update_res.get().size() < m_context.min_writes) { // Checks number of successfull results and if it is less minimum then throw exception
		LOG(DNET_LOG_ERROR, "Can't write data while updating activity for key: %s error: %s\n", m_chunk_key.c_str(), update_res.error().message().c_str());
		throw ioremap::elliptics::error(EREMOTEIO, "Data wasn't written to the minimum number of groups");
	}
	m_self.reset();
}

void features::add_user_activity(const std::string& user, uint64_t time, const void* data, uint32_t size, std::function<void(bool log_written, bool statistics_updated)> func, const std::string& key)
{
	m_user = user;
	LOG(DNET_LOG_INFO, "Async: Add activity user: %s time: %" PRIu64 " data size: %d custom key: %s\n", m_user.c_str(), time, size, key.c_str());
	m_user_key = make_user_key(time);

	m_add_user_activity_callback = func;

	m_activity_key = key.empty() ? make_key(time) : key;

	add_user_data(data, size).connect(boost::bind(&features::add_user_data_callback, this, _1, _2));
}

/*void features::repartition_activity(const std::string& key, uint32_t chunks)
{
	repartition_activity(key, key, chunks);
	m_self.reset();
}

void features::repartition_activity(const std::string& old_key, const std::string& new_key, uint32_t chunks)
{
	LOG(DNET_LOG_DEBUG, "Repartition activity: (%s, %s, %d)\n", old_key.c_str(), new_key.c_str(), chunks);
	m_session.set_ioflags(DNET_IO_FLAGS_CACHE);
	activity tmp

	auto res = get_activity(old_key);
	if (res.size == 0) {
		LOG(DNET_LOG_DEBUG, "Nothing to repartition\n");
		m_self.reset();
		return;
	}

	LOG(DNET_LOG_DEBUG, "Previous count of chunk: %lu\n", res.map.size());
	auto per_chunk = res.map.size() / chunks; // calculates number of elements in new chunk
	per_chunk = per_chunk == 0 ? 1 : per_chunk; // if number of chunk more then elements in result map then keep only 1 element in first chunks
	std::vector<char> data;

	uint32_t chunk_no = 0; // chunk number in which data will be written
	bool written = true;

	tmp.size = chunks;

	for (auto it = res.map.begin(), it_next = res.map.begin(), itEnd = res.map.end(); it != itEnd; it = it_next) { // iterates throw result map
		it_next = std::next(it, per_chunk); // sets it_next by it plus number of elements in one chunk
		tmp.map = std::map<std::string, uint32_t>(it, it_next); // creates sub-map from it to it_next

		msgpack::sbuffer sbuf;
		msgpack::pack(sbuf, tmp); // packs the activity statistics chunk

		auto skey = make_chunk_key(new_key, chunk_no);

		auto write_res = write_data(skey, sbuf.data(), sbuf.size()); // writes data to the elliptics

		if (!write_res) { // checks number of successfull results and if it is less then minimum then mark that some write was failed
			LOG(DNET_LOG_ERROR, "Can't write data while repartition activity key: %s\n", skey.c_str());
			written = false;
		}

		++chunk_no; // increments chunk number
	}

	m_context.keys_cache.remove(old_key);
	m_context.keys_cache.set(new_key, chunks);

	if (!written) { // checks if some writes was failed if so throw exception
		LOG(DNET_LOG_ERROR, "Cannot write one part. Repartition failed\n");
		throw ioremap::elliptics::error(EREMOTEIO, "Some activity wasn't written to the minimum number of groups");
	}
	m_self.reset();
}

void features::repartition_activity(uint64_t time, uint32_t chunks)
{
	repartition_activity(make_key(time), chunks);
	m_self.reset();
}

void features::repartition_activity(uint64_t time, const std::string& new_key, uint32_t chunks)
{
	repartition_activity(make_key(time), new_key, chunks);
	m_self.reset();
}*/

std::list<std::vector<char>> features::get_user_logs(const std::string& user, uint64_t begin_time, uint64_t end_time)
{
	m_user = user;
	LOG(DNET_LOG_INFO, "Getting logs for user: %s begin_time: %" PRIu64 " end_time: %" PRIu64 "\n", m_user.c_str(), begin_time, end_time);

	std::list<std::vector<char>> ret;

	for_user_logs(m_user, begin_time, end_time, boost::bind(&features::get_user_logs_callback, this, boost::ref(ret), _1, _2, _3));

	LOG(DNET_LOG_DEBUG, "User logs size: %d", (uint32_t)ret.size());

	m_self.reset();
	return ret; //returns combined user logs.
}

std::set<std::string> features::get_active_users(uint64_t time)
{
	return get_active_users(make_key(time));
}

std::set<std::string> features::get_active_users(const std::string& key)
{
	LOG(DNET_LOG_INFO, "Getting active users with key: %s\n", key.c_str());

	return get_activity(key);
}

void features::for_user_logs(const std::string& user, uint64_t begin_time, uint64_t end_time,
		std::function<bool(uint64_t time, void* data, uint32_t size)> func)
{
	m_user = user;
	LOG(DNET_LOG_INFO, "Iterating by user logs for user: %s, begin time: %" PRIu64 " end time: %" PRIu64 " \n", m_user.c_str(), begin_time, end_time);

	std::list<std::pair<uint64_t,ioremap::elliptics::async_read_result>> async_results;

	for (auto time = begin_time; time <= end_time; time += consts::SEC_PER_DAY) { // iterates by user logs
		m_user_key = make_user_key(time);
		LOG(DNET_LOG_INFO, "Try to read user logs for user: %s time: %" PRIu64 " key: %s\n", m_user.c_str(), time, m_user_key.c_str());
		async_results.emplace_back(time, m_session.read_latest(m_user_key, 0, 0));
	}

	for(auto it = async_results.begin(), itEnd = async_results.end(); it != itEnd; ++it) {
		try {
			auto file = it->second.get_one().file(); // reads user log file
			if (file.empty()) // if the file is empty
				continue; // skip it and go to the next

			if (!func(it->first, file.data(), file.size())) // calls user's callback with user's log file data
				return;
		}
		catch(std::exception& e) {} // skips standard exception while iterating
	}

	m_self.reset();
}

void features::for_active_users(uint64_t time, std::function<bool(const std::string&)> func)
{
	for_active_users(make_key(time), func);

	m_self.reset();
}

void features::for_active_users(const std::string& key, std::function<bool(const std::string&)> func)
{
	LOG(DNET_LOG_INFO, "Iterating by active users for key:%s\n", key.c_str());
	auto res = get_active_users(key); // gets activity map for the key

	for (auto it = res.begin(), itEnd = res.end(); it != itEnd; ++it) { // iterates by users from activity map
		if (!func(*it)) //calls callback for each user activity from activity map
			break;
	}

	m_self.reset();
}

ioremap::elliptics::async_write_result features::add_user_data(const void* data, uint32_t size)
{
	LOG(DNET_LOG_DEBUG, "Adding DNET_IO_FLAGS_APPEND to session ioflags\n");
	m_session.set_ioflags(DNET_IO_FLAGS_APPEND);

	auto dp = ioremap::elliptics::data_pointer::copy(data, size);

	LOG(DNET_LOG_DEBUG, "Try to append user log to key: %s\n", m_user_key.c_str());

	return m_session.write_data(m_user_key, dp, 0); // write data into elliptics
}

ioremap::elliptics::async_update_indexes_result features::update_activity()
{
	m_session.set_ioflags(DNET_IO_FLAGS_CACHE/* | DNET_IO_FLAGS_CACHE_ONLY*/);
	uint32_t chunk = 0;
	auto size = m_context.keys_cache.get(m_activity_key);
	if(size != 0) {
		chunk = rand(size);
	}

	std::vector<std::string> indexes;
	std::vector<ioremap::elliptics::data_pointer> datas;
	indexes.emplace_back(make_chunk_key(m_activity_key, chunk));
	datas.emplace_back(m_user);

	LOG(DNET_LOG_DEBUG, "Update indexes with key: %s and index: %s\n", m_user.c_str(), indexes.front().c_str());
	return m_session.update_indexes(m_user, indexes, datas);
}

inline std::string features::make_user_key(uint64_t time) const
{
	return m_user + '.' + boost::lexical_cast<std::string>(time / consts::SEC_PER_DAY);
}

inline std::string features::make_key(uint64_t time) const
{
	return boost::lexical_cast<std::string>(time / consts::SEC_PER_DAY);
}

inline std::string features::make_chunk_key(const std::string& key, uint32_t chunk) const
{
	return key + '.' + boost::lexical_cast<std::string>(chunk);
}

bool features::write_data(const std::string& key, void* data, uint32_t size)
{
	auto dp = ioremap::elliptics::data_pointer::copy(data, size);
	LOG(DNET_LOG_DEBUG, "Writing sync data to key: %s\n", key.c_str());
	auto write_res = m_session.write_data(key, dp, 0).get(); // write data into elliptics

	return write_res.size() >= m_context.min_writes; // checks number of successfull results and if it is less then minimum then throw exception
}

uint32_t features::rand(uint32_t max)
{
	boost::lock_guard<boost::mutex> lock(m_context.gen_mutex);
	boost::uniform_int<> dist(0, max - 1);
	boost::variate_generator<boost::mt19937&, boost::uniform_int<>> limited(m_context.generator, dist);
	return limited();
}

std::set<std::string> features::get_activity(const std::string& key)
{
	std::set<std::string> ret;
	m_session.set_ioflags(DNET_IO_FLAGS_CACHE/* | DNET_IO_FLAGS_CACHE_ONLY*/);
	std::vector<std::string> indexes;
	for(int i = 0; i < 1; ++i) {
		indexes.emplace_back(make_chunk_key(key, i));
	}

	LOG(DNET_LOG_DEBUG, "Find indexes %s\n", indexes.front().c_str());
	std::vector<ioremap::elliptics::find_indexes_result_entry> res = m_session.find_indexes(indexes);
	LOG(DNET_LOG_DEBUG, "Found %zd results\n", res.size());

	for(auto it = res.begin(), itEnd = res.end(); it != itEnd; ++it) {
		for(auto ind_it = it->indexes.begin(), ind_end = it->indexes.end(); ind_it != ind_end; ++ind_it) {
			LOG(DNET_LOG_DEBUG, "Found value: %s\n", ind_it->second.to_string().c_str());
			ret.insert(ind_it->second.to_string());
		}
	}

	return ret;
}

void features::update_activity_callback(const ioremap::elliptics::sync_update_indexes_result& res, const ioremap::elliptics::error_info&)
{
	bool written = res.size() >= m_context.min_writes;

	LOG(DNET_LOG_DEBUG, "Write callback result: %s\n", (written ? "written" : "failed"));

	m_stat_updated = written;
	m_add_user_activity_callback(m_log_written, m_stat_updated);
	m_self.reset();
}

bool features::get_user_logs_callback(std::list<std::vector<char>>& ret, uint64_t time, void* data, uint32_t size)
{
	ret.push_back(std::vector<char>((char*)data, (char*)data + size)); // combine all logs in result list
	LOG(DNET_LOG_INFO, "Got log for user: %s time: %" PRIu64 " size: %d\n", m_user.c_str(), time, size);
	return true;
}

void features::add_user_data_callback(const ioremap::elliptics::sync_write_result& res, const ioremap::elliptics::error_info&)
{
	m_log_written = res.size() >= m_context.min_writes;
	LOG(DNET_LOG_DEBUG, "Add user data callback result: %s\n", (m_log_written ? "written" : "failed"));

	update_activity().connect(boost::bind(&features::update_activity_callback, this, _1, _2));
}

}
