#include "features.h"

#include <msgpack.hpp>

#include <boost/thread/locks.hpp>
#include <boost/thread/thread.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>
#include <boost/random.hpp>

#include "activity.h"
#include "keys_size_cache.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

//log macro adds HDB: prefix to each log record
#define LOG(l, ...) dnet_log_raw(m_node.get_native(), l, "HDB: "__VA_ARGS__)

namespace history {
namespace consts {
	const int		LOG_LEVEL			= DNET_LOG_DEBUG;	// log level

	const uint32_t	SEC_PER_DAY			= 24 * 60 * 60;		// number of seconds in one day. used for calculation days

	const uint32_t	CHUNKS_COUNT		= 1000;				// default count of activtiy statistics chucks

	const uint32_t	WRITES_BEFORE_FAIL	= 3;				// Number of attempts of write_cas before returning fail result
} /* namespace consts */

features::features(ioremap::elliptics::file_logger& log, ioremap::elliptics::node& node, const std::vector<int>& groups, const uint32_t& min_writes, keys_size_cache& keys_cache)
: m_log(log)
, m_node(node)
, m_groups(groups)
, m_min_writes(min_writes)
, m_keys_cache(keys_cache)
{}

void features::add_user_activity(const std::string& user, uint64_t time, void* data, uint32_t size, const std::string& key)
{
	LOG(DNET_LOG_INFO, "Add activity user: %s time: %" PRIu64 " data size: %d custom key: %s\n", user.c_str(), time, size, key.c_str());

	add_user_data(user, time, data, size);	// Adds data to the user log

	if (key.empty())
		increment_activity(user, make_key(time));		// Increments user's activity statistics which are stored by time id.
	else
		increment_activity(user, key);		// Increments user's activity statistics which are stored by custom key.

	m_self.reset();
}

void features::add_user_activity(const std::string& user, uint64_t time, void* data, uint32_t size, std::function<void(bool log_written, bool statistics_updated)> func, const std::string& key)
{
	LOG(DNET_LOG_INFO, "Async: Add activity user: %s time: %" PRIu64 " data size: %d custom key: %s\n", user.c_str(), time, size, key.c_str());

	m_add_user_activity_callback = func;

	m_user = user;
	m_key = key.empty() ? make_key(time) : key;

	async_add_user_data(user, time, data, size);
}

void features::repartition_activity(const std::string& key, uint32_t chunks)
{
	repartition_activity(key, key, chunks);
	m_self.reset();
}

void features::repartition_activity(const std::string& old_key, const std::string& new_key, uint32_t chunks)
{
	auto s = create_session();
	activity res, tmp;

	res = get_activity(s, old_key);
	if(res.size == 0) {
		m_self.reset();
		return;
	}

	auto per_chunk = res.map.size() / chunks;	// calculates number of elements in new chunk
	per_chunk = per_chunk == 0 ? 1 : per_chunk;	// if number of chunk more then elements in result map then keep only 1 element in first chunks
	std::vector<char> data;

	uint32_t chunk_no = 0;	// chunk number in which data will be written
	bool written = true;

	tmp.size = chunks;

	for(auto it = res.map.begin(), it_next = res.map.begin(), itEnd = res.map.end(); it != itEnd; it = it_next) {	// iterates throw result map
		it_next = std::next(it, per_chunk);	// sets it_next by it plus number of elements in one chunk
		tmp.map = std::map<std::string, uint32_t>(it, it_next);	// creates sub-map from it to it_next

		msgpack::sbuffer sbuf;
		msgpack::pack(sbuf, tmp);							// packs the activity statistics chunk

		auto skey = make_chunk_key(new_key, chunk_no);

		auto write_res = write_data(s, skey, sbuf.data(), sbuf.size());	// writes data to the elliptics

		if (!write_res) {	// checks number of successfull results and if it is less then minimum then mark that some write was failed
			LOG(DNET_LOG_ERROR, "Can't write data while repartition activity key: %s\n", skey.c_str());
			written = false;
		}

		++chunk_no; // increments chunk number
	}

	m_keys_cache.remove(old_key);
	m_keys_cache.set(new_key, chunks);

	if (!written) {	// checks if some writes was failed if so throw exception
		LOG(DNET_LOG_ERROR, "Before throw\n");
		throw ioremap::elliptics::error(-1, "Some activity wasn't written to the minimum number of groups");
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
	LOG(DNET_LOG_INFO, "Getting logs for user: %s begin_time: %" PRIu64 " end_time: %" PRIu64 "\n", user.c_str(), begin_time, end_time);
	std::list<std::vector<char>> ret;

	for_user_logs(user, begin_time, end_time, boost::bind(&features::get_user_logs_callback, this, boost::ref(ret), _1, _2, _3, _4));

	LOG(DNET_LOG_DEBUG, "User logs cout: %d", (uint32_t)ret.size());

	m_self.reset();
	return ret;	//returns combined user logs.
}

std::map<std::string, uint32_t> features::get_active_users(uint64_t time)
{
	return get_active_users(make_key(time));
}

std::map<std::string, uint32_t> features::get_active_users(const std::string& key)
{
	LOG(DNET_LOG_INFO, "Getting active users with key: %s\n", key.c_str());
	auto s = create_session();

	auto ret = get_activity(s, key).map;	// returns result map

	m_self.reset();

	return ret;
}

void features::for_user_logs(const std::string& user, uint64_t begin_time, uint64_t end_time,
		std::function<bool(const std::string& user, uint64_t time, void* data, uint32_t size)> func)
{
	LOG(DNET_LOG_INFO, "Iterating by user logs for user: %s, begin time: %" PRIu64 " end time: %" PRIu64 " \n", user.c_str(), begin_time, end_time);
	auto s = create_session();

	for(auto time = begin_time; time <= end_time; time += consts::SEC_PER_DAY) {	// iterates by user logs
		try {
			auto skey = make_key(user, time);
			LOG(DNET_LOG_INFO, "Try to read user logs for user: %s time: %" PRIu64 " key: %s\n", user.c_str(), time, skey.c_str());
			ioremap::elliptics::sync_read_result read_res = s.read_latest(skey, 0, 0);
			auto file = read_res[0].file();	// reads user log file

			if (file.empty())	// if the file is empty
				continue;		// skip it and go to the next

			if (!func(user, time, file.data(), file.size()))	// calls user's callback with user's log file data
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
	std::map<std::string, uint32_t> res = get_active_users(key);	// gets activity map for the key

	for(auto it = res.begin(), itEnd = res.end(); it != itEnd; ++it) {	// iterates by users from activity map
		if (!func(it->first, it->second))	//calls callback for each user activity from activity map
			break;
	}

	m_self.reset();
}

ioremap::elliptics::session features::create_session(uint64_t ioflags)
{
	ioremap::elliptics::session session(m_node);	// creates session and connects it with node

	session.set_cflags(0);	// sets cflags to 0
	session.set_ioflags(ioflags /*| DNET_IO_FLAGS_CACHE */);	// sets ioflags with DNET_IO_FLAGS_CACHE

	session.set_groups(m_groups);	// sets groups

	return session;
}

std::shared_ptr<ioremap::elliptics::session> features::shared_session(uint64_t ioflags)
{
	auto session = std::make_shared<ioremap::elliptics::session>(m_node);

	session->set_cflags(0);	// sets cflags to 0
	session->set_ioflags(ioflags /*| DNET_IO_FLAGS_CACHE */);	// sets ioflags with DNET_IO_FLAGS_CACHE

	session->set_groups(m_groups);	// sets groups

	return session;
}

void features::add_user_data(const std::string& user, uint64_t time, void* data, uint32_t size)
{
	auto s = create_session(DNET_IO_FLAGS_APPEND);

	auto skey = make_key(user, time);

	auto write_res = write_data(s, skey, data, size);	// Trys to write data to user's log

	if (!write_res) {	// Checks number of successfull results and if it is less minimum then throw exception
		LOG(DNET_LOG_ERROR, "Can't write data while adding data to user log key: %s\n", skey.c_str());
		throw ioremap::elliptics::error(-1, "Data wasn't written to the minimum number of groups");
	}
	LOG(DNET_LOG_DEBUG, "written data to user log user: %s time: %" PRIu64 " data size: %d\n", user.c_str(), time, size);
}

void features::async_add_user_data(const std::string& user, uint64_t time, void* data, uint32_t size)
{

	auto s = create_session(DNET_IO_FLAGS_APPEND);

	auto skey = make_key(user, time);

	write_data(s, skey, data, size, boost::bind(&features::add_user_data_callback, this, _1, _2));
}

void features::increment_activity(const std::string& user, const std::string& key)
{
	uint32_t attempt = 0;

	while(	attempt++ < consts::WRITES_BEFORE_FAIL &&		// Tries const::WRITES_BEFORE_FAIL times
			!try_increment_activity(user, key)) {			// to increment user activity statistics
		LOG(DNET_LOG_DEBUG, "Exteral attemt to increment activity of user: %s with key %s attempt: %d\n", user.c_str(), key.c_str(), attempt);
	}

	if (attempt > 3) { // checks number of successfull results and if it is less then minimum then throw exception
		LOG(DNET_LOG_ERROR, "Before throw\n");
		throw ioremap::elliptics::error(-1, "Data wasn't written to the minimum number of groups");
	}
}

void features::async_increment_activity(const std::string& user, const std::string& key)
{
	LOG(DNET_LOG_DEBUG, "Async increment activity for user: %s and key: %s\n", user.c_str(), key.c_str());
	m_attempt = 0;

	async_try_increment_activity(user, key);
}

bool features::try_increment_activity(const std::string& user, const std::string& key)
{
	activity act;
	uint32_t chunk = 0;

	auto s = create_session();

	dnet_id checksum;

	auto size = m_keys_cache.get(key);	// gets number of chunks from cache
	if(	size == (uint32_t)-1 &&	// if it isn't in cache
		get_chunk(s, key, 0, act, &checksum)) {	// gets chunk with zero chunk number
		size = act.size;
	}

	if(size != (uint32_t)-1) {
		chunk = rand(size);
		get_chunk(s, key, chunk, act, &checksum);	// gets chunk from this chunk
	}
	else {
		act.size = consts::CHUNKS_COUNT;	// sets default value for act.size
		chunk = 0;	// and writes chunk to zero chunk
	}

	m_keys_cache.set(key, size);

	auto res = act.map.insert(std::make_pair(user, 1));	//Trys to insert new record in map for the user
	if (!res.second)									// if the record wasn't inserted
		++res.first->second;							// increments old statistics

	msgpack::sbuffer sbuf;
	msgpack::pack(sbuf, act);							// packs activity statistics chunk

	auto skey = make_chunk_key(key, chunk);

	auto write_res = write_data(s, skey, sbuf.data(), sbuf.size(), checksum);	// write data into elliptics with checksum

	if(!write_res)
		LOG(DNET_LOG_ERROR, "Can't write data while incrementing activity key: %s\n", skey.c_str());
	else
		LOG(DNET_LOG_DEBUG, "Incremented activity for user: %s key:%s\n", user.c_str(), skey.c_str());

	return write_res;
}

void features::async_try_increment_activity(const std::string& user, const std::string& key)
{
	LOG(DNET_LOG_DEBUG, "Async trying increment activity for user: %s and key: %s\n", user.c_str(), key.c_str());
	auto s = shared_session();

	m_chunk = 0;

	auto size = m_keys_cache.get(key);
	LOG(DNET_LOG_DEBUG, "Count of chunks for key %s = %d\n", key.c_str(), size);
	if(size == (uint32_t) -1) {
		get_chunk(s, key, 0, boost::bind(&features::size_callback, this, s, user, key, _1, _2, _3));
	}
	else {
		m_chunk = rand(size);
		get_chunk(s, key, m_chunk, boost::bind(&features::read_callback, this, s, user, key, _1, _2, _3));
	}
}

std::string features::make_key(const std::string& user, uint64_t time)
{
	return user + "." + boost::lexical_cast<std::string>(time / consts::SEC_PER_DAY);
}

std::string features::make_key(uint64_t time)
{
	return boost::lexical_cast<std::string>(time / consts::SEC_PER_DAY);
}

std::string features::make_chunk_key(const std::string& key, uint32_t chunk)
{
	return key + "." + boost::lexical_cast<std::string>(chunk);
}

bool features::get_chunk(ioremap::elliptics::session& s, const std::string& key, uint32_t chunk, activity& act, dnet_id* checksum)
{
	act.map.clear();
	if(checksum)
		s.transform(std::string(), *checksum);
	try {
		auto skey = make_chunk_key(key, chunk);	// generate chunk key
		ioremap::elliptics::sync_read_result res = s.read_latest(skey, 0, 0);
		auto file = res[0].file();	// trys to read chunk data
		if(checksum)
			s.transform(file, *checksum);
		if (!file.empty()) {	// if it isn't empty
			msgpack::unpacked msg;
			msgpack::unpack(&msg, file.data<const char>(), file.size());	// unpack chunk
			msg.get().convert(&act);
			return true;	// returns it
		}
	}
	catch(std::exception& e) {}
	return false;
}

void features::get_chunk(std::shared_ptr<ioremap::elliptics::session> s, const std::string& key, uint32_t chunk, std::function<void(bool exist, activity act, dnet_id checksum)> func)
{
	auto callback = boost::bind(&features::get_chunk_callback, this, func, s, _1);

	auto skey = make_chunk_key(key, chunk);
	s->read_latest(skey, 0, 0).connect(callback);
}

bool features::write_data(ioremap::elliptics::session& s, const std::string& key, void* data, uint32_t size)
{
	auto dp = ioremap::elliptics::data_pointer::from_raw(data, size);
	LOG(DNET_LOG_DEBUG, "Try write sync data to key: %s\n", key.c_str());
	auto write_res = s.write_data(key, dp, 0).get();	// write data into elliptics

	return write_res.size() >= m_min_writes;	// checks number of successfull results and if it is less then minimum then throw exception
}

void features::write_data(ioremap::elliptics::session& s, const std::string& key, void* data, uint32_t size, std::function<void(const ioremap::elliptics::sync_write_result& res, const ioremap::elliptics::error_info&)> func)
{
	auto dp = ioremap::elliptics::data_pointer::from_raw(data, size);
	LOG(DNET_LOG_DEBUG, "Try write async data to key: %s\n", key.c_str());

	s.write_data(key, dp, 0).connect(func);
}

bool features::write_data(ioremap::elliptics::session& s, const std::string& key, void* data, uint32_t size, const dnet_id& checksum)
{
	auto dp = ioremap::elliptics::data_pointer::from_raw(data, size);
	LOG(DNET_LOG_DEBUG, "Try write csum sync data to key: %s\n", key.c_str());
	auto write_res = s.write_cas(key, dp, checksum, 0).get();	// write data into elliptics

	return write_res.size() >= m_min_writes;
}

void features::write_data(std::shared_ptr<ioremap::elliptics::session> s, const std::string& key, void* data, uint32_t size, const dnet_id& checksum, std::function<void(const ioremap::elliptics::sync_write_result& res, const ioremap::elliptics::error_info&)> func)
{
	auto dp = ioremap::elliptics::data_pointer::from_raw(data, size);
	LOG(DNET_LOG_DEBUG, "Try write csum async data to key: %s\n", key.c_str());

	s->write_cas(key, dp, checksum, 0).connect(func);
}

uint32_t features::rand(uint32_t max)
{
	static boost::mutex mutex;
	boost::mutex::scoped_lock lock(mutex);
	static boost::mt19937 rnd;
	boost::uniform_int<> dist(0, max - 1);

	return dist(rnd);
}

void features::merge(activity& res_chunk, const activity& merge_chunk) const
{
	res_chunk.size = merge_chunk.size;
	auto res_it = res_chunk.map.begin(); // sets res_it to begin of res_map
	size_t size;
	for(auto it = merge_chunk.map.begin(), itEnd = merge_chunk.map.end(); it != itEnd; ++it) {	// iterates by merge_chunk map pairs
		size = res_chunk.map.size(); 															// saves old res_map size
		res_it = res_chunk.map.insert(res_it, *it);												// trys insert pair from merge_chunk map into res_map at res_it and sets to res_it iterator to the inserted element. If pair would be inserted then size of res_map will be increased.
		if (size == res_chunk.map.size())														// checks saved size with current size of res_map if they are equal - item wasn't inserted and we should add data from it with res_it.
			res_it->second += it->second;
	}
}

activity features::get_activity(ioremap::elliptics::session& s, const std::string& key)
{
	activity ret, tmp;
	ret.size = 0;

	auto size = m_keys_cache.get(key);	// gets size from cache
	uint32_t i = 0;
	if(size == (uint32_t)-1) {
		if(!get_chunk(s, key, i++, tmp)) {	// gets data from zero chunk
			LOG(DNET_LOG_INFO, "Activity statistics is empty for key:%s\n", key.c_str());
			return ret;	// if it is no zero chunk so nothing to return and returns empty map
		}

		merge(ret, tmp);	// merge chunks

		size = ret.size;	// set number of chunks to size
		m_keys_cache.set(key, size);
	}

	for(; i < size; ++i) {	// iterates by chunks
		if(get_chunk(s, key, i, tmp)) {		// gets chunk
			merge(ret, tmp);				// merge map from chunk into result map
		}
	}

	return ret;		// returns result map
}

void features::size_callback(std::shared_ptr<ioremap::elliptics::session> s, const std::string& user, const std::string& key, bool exist, activity act, dnet_id)
{
	LOG(DNET_LOG_DEBUG, "Size callback result file: %s\n", (exist ? "read" : "failed"));

	if(!exist) {
		m_keys_cache.set(key, consts::CHUNKS_COUNT);
		activity ac;
		ac.size = consts::CHUNKS_COUNT;
		m_chunk = 0;
		read_callback(s, user, key, false, ac, dnet_id());
		return;
	}

	m_chunk = rand(act.size);
	m_keys_cache.set(key, act.size);
	get_chunk(s, key, m_chunk, boost::bind(&features::read_callback, this, s, user, key, _1, _2, _3));
}

void features::read_callback(std::shared_ptr<ioremap::elliptics::session> s, const std::string& user, const std::string& key, bool exist, activity act, dnet_id checksum)
{
	LOG(DNET_LOG_DEBUG, "Read callback result: %s\n", (exist ? "read" : "failed"));

	auto res = act.map.insert(std::make_pair(user, 1));
	if(!res.second)
		++res.first->second;

	msgpack::sbuffer sbuf;
	msgpack::pack(sbuf, act);

	auto skey = make_chunk_key(key, m_chunk);

	write_data(s, skey, sbuf.data(), sbuf.size(), checksum, boost::bind(&features::write_callback, this, _1, _2));
}

void features::write_callback(const ioremap::elliptics::sync_write_result& res, const ioremap::elliptics::error_info& error)
{
	bool written = res.size() >= m_min_writes;

	LOG(DNET_LOG_DEBUG, "Write callback result: %s\n", (written ? "written" : "failed"));

	if(written || m_attempt >= consts::WRITES_BEFORE_FAIL)
	{
		m_stat_updated = written;
		m_add_user_activity_callback(m_log_written, m_stat_updated);
		m_self.reset();
		return;
	}

	++m_attempt;

	async_try_increment_activity(m_user, m_key);
}

bool features::get_user_logs_callback(std::list<std::vector<char>>& ret, const std::string& user, uint64_t time, void* data, uint32_t size)
{
	ret.push_back(std::vector<char>((char*)data, (char*)data + size));														// combine all logs in result list
	LOG(DNET_LOG_INFO, "Got log for user: %s time: %" PRIu64 " size: %d\n", user.c_str(), time, size);
	return true;
}

void features::add_user_data_callback(const ioremap::elliptics::sync_write_result& res, const ioremap::elliptics::error_info& error)
{
	bool written = res.size() >= m_min_writes;
	LOG(DNET_LOG_DEBUG, "Add user data callback result: %s\n", (written ? "written" : "failed"));
	m_log_written = written;

	async_increment_activity(m_user, m_key);
}

void features::get_chunk_callback(std::function<void(bool exist, activity act, dnet_id checksum)> func, std::shared_ptr<ioremap::elliptics::session> s, const ioremap::elliptics::sync_read_result& res)
{
	LOG(DNET_LOG_DEBUG, "Get chunk callback result\n");

	bool exists = false;
	dnet_id checksum;
	s->transform(std::string(), checksum);
	activity act;
	try {
		LOG(DNET_LOG_DEBUG, "Get chunk callback try handle file\n");
		auto file = res[0].file();
		s->transform(file, checksum);
		if(!file.empty()) {
			msgpack::unpacked msg;
			msgpack::unpack(&msg, file.data<const char>(), file.size());
			msg.get().convert(&act);
			exists = true;
		}
	}
	catch(std::exception& e) {}
	func(exists, act, checksum);
}

}
