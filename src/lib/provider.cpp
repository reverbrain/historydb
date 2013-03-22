#include "provider.h"
#include <vector>
#include <time.h>
#include <msgpack.hpp>
#include <iomanip>
#include <boost/thread/locks.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread/thread.hpp>
#include "activity.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

//log macro adds HDB: prefix to each log record
#define LOG(l, ...) dnet_log_raw(m_node.get_native(), l, "HDB: "__VA_ARGS__)

namespace history {
namespace consts {
	const char* 	LOG_FILE			= "log.log";		// path to log file
	const int		LOG_LEVEL			= DNET_LOG_DEBUG;	// log level

	const uint32_t	SEC_PER_DAY			= 24 * 60 * 60;		// number of seconds in one day. used for calculation days

	const uint32_t	CHUNKS_COUNT		= 1000;				// default count of activtiy statistics chucks

	const uint32_t	WRITES_BEFORE_FAIL	= 3;				// Number of attempts of write_cas before returning fail result
} /* namespace consts */

std::shared_ptr<iprovider> create_provider(const char* server_addr, const int server_port, const int family)
{
	return std::make_shared<provider>(server_addr, server_port, family);
}

provider::provider(const char* server_addr, const int server_port, const int family)
: m_log(consts::LOG_FILE, consts::LOG_LEVEL)
, m_node(m_log)
{
	srand(time(NULL));

	m_node.add_remote(server_addr, server_port, family);	// Adds connection parameters to the node.

	LOG(DNET_LOG_INFO, "Created provider for %s:%d:%d\n", server_addr, server_port, family);
}

provider::~provider()
{
	LOG(DNET_LOG_INFO, "Destroyed provder\n");
}

void provider::set_session_parameters(const std::vector<int>& groups, uint32_t min_writes)
{
	m_groups = groups;
	m_min_writes = min_writes > m_groups.size() ? m_groups.size() : min_writes;

	LOG(DNET_LOG_INFO, "Session parameters:\n");
	for(auto it = m_groups.begin(), it_end = m_groups.end(); it != it_end; ++it)
		LOG(DNET_LOG_INFO, "group:  %d\n", *it);
	LOG(DNET_LOG_INFO, "minimum number of successfull writes: %d\n", min_writes);
}

void provider::add_user_activity(const std::string& user, uint64_t time, void* data, uint32_t size, const std::string& key)
{
	LOG(DNET_LOG_INFO, "Add activity user: %s time: %" PRIu64 " data size: %d custom key: %s\n", user.c_str(), time, size, key.c_str());

	add_user_data(user, time, data, size);	// Adds data to the user log

	if (key.empty())
		increment_activity(user, make_key(time));		// Increments user's activity statistics which are stored by time id.
	else
		increment_activity(user, key);		// Increments user's activity statistics which are stored by custom key.
}

void provider::repartition_activity(const std::string& key, uint32_t chunks)
{
	repartition_activity(key, key, chunks);
}

void provider::repartition_activity(const std::string& old_key, const std::string& new_key, uint32_t chunks)
{
	auto s = create_session();
	auto size = get_chunks_count(s, old_key);	// gets number of chunk for old key
	if (size == (uint32_t)-1)					// if it is equal to -1 then data in old_key doesn't exsists so nothin to repartition
		return;

	activity res, tmp;
	res.size = chunks;
	std::string skey;

	for(uint32_t i = 0; i < size; ++i) {
		if(!get_chunk(s, old_key, i, tmp))	// Tries to get activity chunk
			continue;						// if no chunk continue to the next chunk

		merge(res, tmp);					// merges chunk
		try {
			s.remove(skey);					// try to remove old chunk becase it is useless now
		}
		catch(std::exception& e) {}
	}

	auto elements_in_chunk = res.map.size() / chunks;						// calculates number of elements in new chunk
	elements_in_chunk = elements_in_chunk == 0 ? 1 : elements_in_chunk;	// if number of chunk more then elements in result map then keep only 1 element in first chunks
	std::vector<char> data;

	uint32_t chunk_no = 0;	// chunk number in which data will be written
	bool written = true;

	tmp.size = chunks;

	for(auto it = res.map.begin(), it_next = res.map.begin(), itEnd = res.map.end(); it != itEnd; it = it_next) {	// iterates throw result map
		it_next = std::next(it, elements_in_chunk);			// sets it_next by it plus number of elements in one chunk
		tmp.map = std::map<std::string, uint32_t>(it, it_next);	// creates sub-map from it to it_next

		msgpack::sbuffer sbuf;
		msgpack::pack(sbuf, tmp);							// packs the activity statistics chunk

		skey = make_chunk_key(new_key, chunk_no);

		auto write_res = write_data(s, skey, sbuf.data(), sbuf.size());	// writes data to the elliptics

		if (!write_res) {	// checks number of successfull results and if it is less then minimum then mark that some write was failed
			LOG(DNET_LOG_ERROR, "Can't write data while repartition activity key: %s\n", skey.c_str());
			written = false;
		}

		++chunk_no; // increments chunk number
	}

	m_keys_cache.remove(old_key);
	m_keys_cache.set(new_key, chunks);

	if (!written)	// checks if some writes was failed if so throw exception
		throw ioremap::elliptics::error(-1, "Some activity wasn't written to the minimum number of groups");
}

void provider::repartition_activity(uint64_t time, uint32_t chunks)
{
	repartition_activity(make_key(time), chunks);
}

void provider::repartition_activity(uint64_t time, const std::string& new_key, uint32_t chunks)
{
	repartition_activity(make_key(time), new_key, chunks);
}

std::list<std::vector<char>> provider::get_user_logs(const std::string& user, uint64_t begin_time, uint64_t end_time)
{
	LOG(DNET_LOG_INFO, "Getting logs for user: %s begin_time: %" PRIu64 " end_time: %" PRIu64 "\n", user.c_str(), begin_time, end_time);
	std::list<std::vector<char>> ret;

	for_user_logs(user, begin_time, end_time, [=, &ret](const std::string& user, uint64_t time, void* data, uint32_t size) {	// iterates by user logs
		ret.push_back(std::vector<char>((char*)data, (char*)data + size));														// combine all logs in result list
		LOG(DNET_LOG_INFO, "Got log for user: %s time: %" PRIu64 " size: %d\n", user.c_str(), time, size);
		return true;
	});
	return ret;	//returns combined user logs.
}

std::map<std::string, uint32_t> provider::get_active_users(uint64_t time)
{
	return get_active_users(make_key(time));
}

std::map<std::string, uint32_t> provider::get_active_users(const std::string& key)
{
	LOG(DNET_LOG_INFO, "Getting active users with key: %s\n", key.c_str());
	auto s = create_session();

	activity ret, tmp;

	uint32_t size = get_chunks_count(s, key);	// gets number of chunks for the key
	if (size == (uint32_t)-1) {					// if it is equal to -1 so there is no activity statistics yet
		LOG(DNET_LOG_INFO, "Active users statistics is empty for key:%s\n", key.c_str());
		return ret.map;							// returns empty map
	}

	std::string skey;
	
	for(uint32_t i = 0; i < size; ++i) {	// iterates by chunks
		tmp.map.clear();
		if(get_chunk(s, key, i, tmp)) {		// gets chunk
			merge(ret, tmp);				// merge map from chunk into result map
		}
	}

	return ret.map;		// returns result map
}

void provider::for_user_logs(const std::string& user, uint64_t begin_time, uint64_t end_time,
		std::function<bool(const std::string& user, uint64_t time, void* data, uint32_t size)> func)
{
	LOG(DNET_LOG_INFO, "Iterating by user logs for user: %s, begin time: %" PRIu64 " end time: %" PRIu64 " \n", user.c_str(), begin_time, end_time);
	auto s = create_session();

	for(auto time = begin_time; time <= end_time; time += consts::SEC_PER_DAY) {	// iterates by user logs
		try {
			auto skey = make_key(user, time);
			LOG(DNET_LOG_INFO, "Try to read user logs for user: %s time: %" PRIu64 " key: %s\n", user.c_str(), time, skey.c_str());
			auto read_res = s.read_latest(skey, 0, 0);
			auto file = read_res->file();	// reads user log file

			if (file.empty())	// if the file is empty
				continue;		// skip it and go to the next

			if (!func(user, time, file.data(), file.size()))	// calls user's callback with user's log file data
				return;
		}
		catch(std::exception& e) {} // skips standard exception while iterating
	}
}

void provider::for_active_users(uint64_t time, std::function<bool(const std::string& user, uint32_t number)> func)
{
	for_active_users(make_key(time), func);
}

void provider::for_active_users(const std::string& key, std::function<bool(const std::string& user, uint32_t number)> func)
{
	LOG(DNET_LOG_INFO, "Iterating by active users for key:%s\n", key.c_str());
	std::map<std::string, uint32_t> res = get_active_users(key);	// gets activity map for the key

	for(auto it = res.begin(), itEnd = res.end(); it != itEnd; ++it) {	// iterates by users from activity map
		if (!func(it->first, it->second))	//calls callback for each user activity from activity map
			break;
	}
}

ioremap::elliptics::session provider::create_session(uint64_t ioflags)
{
	ioremap::elliptics::session session(m_node);	// creates session and connects it with node

	session.set_cflags(0);	// sets cflags to 0
	session.set_ioflags(ioflags /*| DNET_IO_FLAGS_CACHE */);	// sets ioflags with DNET_IO_FLAGS_CACHE

	session.set_groups(m_groups);	// sets groups

	return session;
}

void provider::add_user_data(const std::string& user, uint64_t time, void* data, uint32_t size)
{
	auto s = create_session(DNET_IO_FLAGS_APPEND);

	auto skey = make_key(user, time);

	auto write_res = write_data(s, skey, data, size);	// Trys to write data to user's log

	if (!write_res) {	// Checks number of successfull results and if it is less minimum then throw exception 
		LOG(DNET_LOG_ERROR, "Can't write data while adding data to user log key: %s\n", skey.c_str());
		throw ioremap::elliptics::error(-1, "Data wasn't written to the minimum number of groups");
	}
	LOG(DNET_LOG_DEBUG, "Writed data to user log user: %s time: %" PRIu64 " data size: %d\n", user.c_str(), time, size);
}

void provider::increment_activity(const std::string& user, const std::string& key)
{
	uint32_t attempt = 0;
	
	while(	attempt++ < consts::WRITES_BEFORE_FAIL &&		// Tries const::WRITES_BEFORE_FAIL times
			!try_increment_activity(user, key)) {}			// to increment user activity statistics

	if (attempt > 3) { // checks number of successfull results and if it is less then minimum then throw exception
		throw ioremap::elliptics::error(-1, "Data wasn't written to the minimum number of groups");
	}
}

std::string provider::make_key(const std::string& user, uint64_t time)
{
	return user + "." + boost::lexical_cast<std::string>(time / consts::SEC_PER_DAY);
}

std::string provider::make_key(uint64_t time)
{
	return boost::lexical_cast<std::string>(time / consts::SEC_PER_DAY);
}

std::string provider::make_chunk_key(const std::string& key, uint32_t chunk)
{
	return key + "." + boost::lexical_cast<std::string>(chunk);
}

bool provider::get_chunk(ioremap::elliptics::session& s, const std::string& key, uint32_t chunk, activity& act, dnet_id* checksum)
{
	if(checksum)
		s.transform(std::string(), *checksum);
	try
	{
		auto skey = make_chunk_key(key, chunk);			// generate chunk key
		auto file = s.read_latest(skey, 0, 0)->file();	// trys to read chunk data
		if(checksum)
			s.transform(file, *checksum);
		if (!file.empty()) {							// if it isn't empty
			msgpack::unpacked msg;
			msgpack::unpack(&msg, file.data<const char>(), file.size());	// unpack chunk
			msg.get().convert(&act);
			return true;								// returns it
		}
	}
	catch(std::exception& e) {}
	return false;
}

uint32_t provider::get_chunks_count(ioremap::elliptics::session& s, const std::string& key)
{
	auto count = m_keys_cache.get(key);
	if(count != (uint32_t)-1)
		return count;

	activity act;
	if(get_chunk(s, key, 0, act)) {			// if chunk has successfully readed
		m_keys_cache.set(key, act.size);	// saves size in cache
		return act.size;					// returns count of chunks
	}

	return -1;	// in the other case returns -1
}

bool provider::try_increment_activity(const std::string& user, const std::string& key)
{
	activity act;
	uint32_t chunk = 0;

	auto s = create_session();

	act.size = get_chunks_count(s, key);
	if(act.size == (uint32_t)-1) {
		act.size = consts::CHUNKS_COUNT;
		chunk = 0;
	}
	else
		chunk = rand(act.size);

	dnet_id checksum;
	get_chunk(s, key, chunk, act, &checksum);

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

bool provider::write_data(ioremap::elliptics::session& s, const std::string& key, void* data, uint32_t size)
{
	auto dp = ioremap::elliptics::data_pointer::from_raw(data, size);
	auto write_res = s.write_data(key, dp, 0);	// write data into elliptics

	return write_res.size() >= m_min_writes;	// checks number of successfull results and if it is less then minimum then throw exception
}

bool provider::write_data(ioremap::elliptics::session& s, const std::string& key, void* data, uint32_t size, const dnet_id& checksum)
{
	auto dp = ioremap::elliptics::data_pointer::from_raw(data, size);
	auto write_res = s.write_cas(key, dp, checksum, 0);	// write data into elliptics

	return write_res.size() >= m_min_writes;
}

uint32_t provider::rand(uint32_t max)
{
	return ::rand() % max;
}

void provider::merge(activity& res_chunk, const activity& merge_chunk) const
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

uint32_t provider::keys_size_cache::get(const std::string& key)
{
	boost::shared_lock<boost::shared_mutex> lock(m_mutex);

	auto it = m_keys_sizes.find(key);	//finds key in map

	return it == m_keys_sizes.end() ? -1 : it->second;	// if we have such key in map returns known size otherwise return -1
}

void provider::keys_size_cache::set(const std::string& key, uint32_t size)
{
	boost::unique_lock<boost::shared_mutex> lock(m_mutex);

	auto res = m_keys_sizes.insert(std::make_pair(key, size));	// try to insert new size for key
	if(!res.second)												// if it failed
		res.first->second = size;								// rewrites old size
}

void provider::keys_size_cache::remove(const std::string& key)
{
	boost::unique_lock<boost::shared_mutex> lock(m_mutex);
	m_keys_sizes.erase(key);	// removes key
}

} /* namespace history */
