#include "features.h"

#include <msgpack.hpp>
#include <functional>

#include <boost/thread/locks.hpp>
#include <boost/thread/thread.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>

#include "activity.h"
#include "keys_size_cache.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "provider.h"

//log macro adds HDB: prefix to each log record
#define LOG(l, ...) dnet_log_raw(m_context.node.get_native(), l, "HDB: "__VA_ARGS__)

namespace history {
namespace consts {
	const uint32_t	SEC_PER_DAY			= 24 * 60 * 60; // number of seconds in one day. used for calculation days
	const uint32_t	CHUNKS_COUNT		= 10; // number of chunks in user's shard
	const uint32_t	USERS_CHUNKS_COUNT	= 1000; // number of user's shard.
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

void features::add_user_activity(const std::string& user, uint64_t time, void* data, uint32_t size, const std::string& key)
{
	m_user = user;
	LOG(DNET_LOG_INFO, "Add activity user: %s time: %" PRIu64 " data size: %d custom key: %s\n", m_user.c_str(), time, size, key.c_str());

	m_user_key = make_user_key(time);

	m_activity_key = key.empty() ? make_key(time) : key;

	auto add_res = add_user_data(data, size); // Adds data to the user log
	auto increment_res = increment_activity();

	auto res = add_res.get();
	if (res.size() < m_context.min_writes) { // Checks number of successfull results and if it is less minimum then throw exception
		LOG(DNET_LOG_ERROR, "Can't write data while adding data to user log key: %s error: %s\n", m_user_key.c_str(), add_res.error().message().c_str());
		throw ioremap::elliptics::error(EREMOTEIO, "Data wasn't written to the minimum number of groups");
	}

	res = increment_res.get();
	if (res.size() < m_context.min_writes) { // Checks number of successfull results and if it is less minimum then throw exception
		LOG(DNET_LOG_ERROR, "Can't write data while incrementing activity for key: %s error: %s\n", m_chunk_key.c_str(), increment_res.error().message().c_str());
		throw ioremap::elliptics::error(EREMOTEIO, "Data wasn't written to the minimum number of groups");
	}
	m_self.reset();
}

void features::add_user_activity(const std::string& user, uint64_t time, void* data, uint32_t size, std::function<void(bool log_written, bool statistics_updated)> func, const std::string& key)
{
	m_user = user;
	LOG(DNET_LOG_INFO, "Async: Add activity user: %s time: %" PRIu64 " data size: %d custom key: %s\n", m_user.c_str(), time, size, key.c_str());
	m_user_key = make_user_key(time);

	m_add_user_activity_callback = func;

	m_activity_key = key.empty() ? make_key(time) : key;

	add_user_data(data, size).connect(boost::bind(&features::add_user_data_callback, this, _1, _2));
}

void features::repartition_activity(const std::string& key, uint32_t chunks)
{
	repartition_activity(key, key, chunks);
	m_self.reset();
}

void features::repartition_activity(const std::string& old_key, const std::string& new_key, uint32_t chunks)
{
	LOG(DNET_LOG_DEBUG, "Repartition activity: (%s, %s, %d)\n", old_key.c_str(), new_key.c_str(), chunks);
	m_session.set_ioflags(DNET_IO_FLAGS_CACHE/* | DNET_IO_FLAGS_CACHE_ONLY*/);
	activity tmp;

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

		auto skey = make_chunk_key(new_key, 0, chunk_no);

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
}

std::list<std::vector<char>> features::get_user_logs(const std::string& user, uint64_t begin_time, uint64_t end_time)
{
	m_user = user;
	LOG(DNET_LOG_INFO, "Getting logs for user: %s begin_time: %" PRIu64 " end_time: %" PRIu64 "\n", m_user.c_str(), begin_time, end_time);

	std::list<std::vector<char>> ret;

	for_user_logs(m_user, begin_time, end_time, boost::bind(&features::get_user_logs_callback, this, boost::ref(ret), _1, _2, _3));

	LOG(DNET_LOG_DEBUG, "User logs cout: %d", (uint32_t)ret.size());

	m_self.reset();
	return ret; //returns combined user logs.
}

std::map<std::string, uint32_t> features::get_active_users(uint64_t time)
{
	return get_active_users(make_key(time));
}

std::map<std::string, uint32_t> features::get_active_users(const std::string& key)
{
	LOG(DNET_LOG_INFO, "Getting active users with key: %s\n", key.c_str());

	auto ret = get_activity(key).map; // returns result map

	m_self.reset();

	return ret;
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

void features::for_active_users(uint64_t time, std::function<bool(const std::string& user, uint32_t number)> func)
{
	for_active_users(make_key(time), func);

	m_self.reset();
}

void features::for_active_users(const std::string& key, std::function<bool(const std::string& user, uint32_t number)> func)
{
	LOG(DNET_LOG_INFO, "Iterating by active users for key:%s\n", key.c_str());
	auto res = get_active_users(key); // gets activity map for the key

	for (auto it = res.begin(), itEnd = res.end(); it != itEnd; ++it) { // iterates by users from activity map
		if (!func(it->first, it->second)) //calls callback for each user activity from activity map
			break;
	}

	m_self.reset();
}

ioremap::elliptics::async_write_result features::add_user_data(void* data, uint32_t size)
{
	LOG(DNET_LOG_DEBUG, "Adding DNET_IO_FLAGS_APPEND to session ioflags\n");
	m_session.set_ioflags(DNET_IO_FLAGS_APPEND);

	auto dp = ioremap::elliptics::data_pointer::copy(data, size);

	LOG(DNET_LOG_DEBUG, "Try to append user log to key: %s\n", m_user_key.c_str());

	return m_session.write_data(m_user_key, dp, 0); // write data into elliptics
}

ioremap::elliptics::async_write_result features::increment_activity()
{
	m_session.set_ioflags(DNET_IO_FLAGS_CACHE/* | DNET_IO_FLAGS_CACHE_ONLY*/);

	uint32_t chunk = 0;
	auto size = m_context.keys_cache.get(m_activity_key);
	if(size != (uint32_t)-1) {
		chunk = rand(size);
	}

	auto shard = std::hash<std::string>()(m_user) % consts::USERS_CHUNKS_COUNT;

	m_chunk_key = make_chunk_key(m_activity_key, shard, chunk);
	LOG(DNET_LOG_DEBUG, "Try to increment user activity in key %s\n", m_chunk_key.c_str());
	return m_session.write_cas(m_chunk_key, boost::bind(&features::write_cas_callback, this, _1), 0, consts::WRITES_BEFORE_FAIL);
}

std::string features::make_user_key(uint64_t time)
{
	return m_user + '.' + boost::lexical_cast<std::string>(time / consts::SEC_PER_DAY);
}

std::string features::make_key(uint64_t time)
{
	return boost::lexical_cast<std::string>(time / consts::SEC_PER_DAY);
}

std::string features::make_chunk_key(const std::string& key, uint32_t user_chunk, uint32_t chunk)
{
	return key + '.' + boost::lexical_cast<std::string>(user_chunk) + '.' + boost::lexical_cast<std::string>(chunk);
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
	boost::uniform_int<> dist(0, max - 1);
	boost::variate_generator<boost::mt19937&, boost::uniform_int<>> limited(m_context.generator, dist);
	return limited();
}

void features::merge(activity& res_chunk, const activity& merge_chunk) const
{
	res_chunk.size = merge_chunk.size;
	auto res_it = res_chunk.map.begin(); // sets res_it to begin of res_map
	size_t size;
	for (auto it = merge_chunk.map.begin(), itEnd = merge_chunk.map.end(); it != itEnd; ++it) { // iterates by merge_chunk map pairs
		size = res_chunk.map.size(); // saves old res_map size
		res_it = res_chunk.map.insert(res_it, *it); // trys insert pair from merge_chunk map into res_map at res_it and sets to res_it iterator to the inserted element. If pair would be inserted then size of res_map will be increased.
		if (size == res_chunk.map.size()) // checks saved size with current size of res_map if they are equal - item wasn't inserted and we should add data from it with res_it.
			res_it->second += it->second;
	}
}

activity features::get_activity(const std::string& key)
{
	m_session.set_ioflags(DNET_IO_FLAGS_CACHE/* | DNET_IO_FLAGS_CACHE_ONLY*/);

	activity ret, tmp;

	auto size = m_context.keys_cache.get(key);
	std::list<ioremap::elliptics::async_read_result> async_results;
	std::string chunk_key;

	for(uint32_t shard = 0; shard < consts::USERS_CHUNKS_COUNT; ++shard) {
		for(uint32_t chunk = 0; chunk < consts::CHUNKS_COUNT; ++chunk) {
			chunk_key = make_chunk_key(key ,shard, chunk);
			async_results.emplace_back(m_session.read_latest(chunk_key, 0, 0));
		}
	}

	for(auto it = async_results.begin(), itEnd = async_results.end(); it != itEnd; ++it) {
		try {
			auto file = it->get_one().file(); // trys to read chunk data
			if (!file.empty()) { // if it isn't empty
				msgpack::unpacked msg;
				msgpack::unpack(&msg, file.data<const char>(), file.size()); // unpack chunk
				msg.get().convert(&tmp);

				merge(ret, tmp);
			}
		}
		catch(std::exception& e) {}
	}

	return ret;
}

void features::increment_activity_callback(const ioremap::elliptics::sync_write_result& res, const ioremap::elliptics::error_info&)
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

	increment_activity().connect(boost::bind(&features::increment_activity_callback, this, _1, _2));
}

ioremap::elliptics::data_pointer features::write_cas_callback(const ioremap::elliptics::data_pointer& data)
{
	activity act;
	act.size = consts::CHUNKS_COUNT;

	if (!data.empty()) {
		try {
			msgpack::unpacked msg;
			msgpack::unpack(&msg, data.data<const char>(), data.size());
			msg.get().convert(&act);
		}
		catch(...) {}
	}

	if (!m_context.keys_cache.has(m_activity_key))
		m_context.keys_cache.set(m_activity_key, act.size);

	auto res = act.map.insert(std::make_pair(m_user, 1)); //Trys to insert new record in map for the user
	if (!res.second) // if the record wasn't inserted
		++res.first->second; // increments old statistics

	msgpack::sbuffer buff;
	msgpack::pack(buff, act); // packs the activity statistics chunk

	return ioremap::elliptics::data_pointer::copy(buff.data(), buff.size());
}

}
