#include "provider.h"
#include <vector>
#include <time.h>
#include <msgpack.hpp>
#include <iomanip>
#include <boost/format.hpp>
#include <boost/thread/locks.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread/thread.hpp>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

//log macro adds HDB: prefix to each log record
#define LOG(l, ...) dnet_log_raw(m_node.get_native(), l, "HDB: "__VA_ARGS__)

namespace history {
namespace consts {
	const char* 	LOG_FILE		= "log.log";
	const int		LOG_LEVEL		= DNET_LOG_DEBUG;

	const uint32_t	SEC_PER_DAY		= 24 * 60 * 60;

	const uint32_t	RAND_SIZE		= 100;
	const auto		NULL_KEY_STR	= "0000000000";
} /* namespace consts */

/*Merges two map. res_map will contain result of merging it with merge_map
*/
template<typename K, typename V>
void merge(std::map<K, V>& res_map, const std::map<K, V>& merge_map)
{
	auto res_it = res_map.begin(); // sets res_it to begin of res_map
	size_t size;
	for(auto it = merge_map.begin(), itEnd = merge_map.end(); it != itEnd; ++it) {	// iterates by merge_map pairs
		size = res_map.size(); 														// saves old res_map size
		res_it = res_map.insert(res_it, *it);										// trys insert pair from merge_map into res_map at res_it and sets to res_it iterator to the inserted element. If pair would be inserted then size of res_map will be increased.
		if (size == res_map.size())													// checks saved size with current size of res_map if they are equal - item wasn't inserted and we should add data from it with res_it.
			res_it->second += it->second;
	}
}

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
		increment_activity(user, time);		// Increments user's activity statistics which are stored by time id.
	else
		increment_activity(user, key);		// Increments user's activity statistics which are stored by custom key.
}

void provider::add_user_data(const std::string& user, uint64_t time, void* data, uint32_t size)
{
	auto s = create_session(DNET_IO_FLAGS_APPEND);

	auto skey = str(boost::format("%s%020d") % user % (time / consts::SEC_PER_DAY));

	auto write_res = s.write_data(skey, ioremap::elliptics::data_pointer::from_raw(data, size), 0);	// Trys to write data to user's log

	if (write_res.size() < m_min_writes) {	// Checks number of successfull results and if it is less minimum then throw exception 
		LOG(DNET_LOG_ERROR, "Can't write data while adding data to user log. Success write: %zu minimum: %d, key: %s\n", write_res.size(), m_min_writes, skey.c_str());
		throw ioremap::elliptics::error(-1, "Data wasn't written to the minimum number of groups");
	}
	LOG(DNET_LOG_DEBUG, "Writed data to user log user: %s time: %" PRIu64 " data size: %d\n", user.c_str(), time, size);
}

void provider::increment_activity(const std::string& user, uint64_t time)
{
	increment_activity(user, str(boost::format("%020d") % (time / consts::SEC_PER_DAY)));
}

void provider::increment_activity(const std::string& user, const std::string& key)
{
	std::map<std::string, uint32_t> map;
	std::string skey;
	uint32_t size;

	generate_activity_key(key, skey, size);		// Generates key and number of chunks for activity statistics.

	auto s = create_session();

	try {	// trys to read current data from key and unpack it into map.
		auto file = s.read_latest(skey, 0, 0)->file();
		file = file.skip<uint32_t>();
		if (!file.empty()) {
			msgpack::unpacked msg;
			msgpack::unpack(&msg, file.data<const char>(), file.size());
			msg.get().convert(&map);
		}
	}
	catch(std::exception& e) {}

	auto res = map.insert(std::make_pair(user, 1));	//Trys to insert new record in map for the user
	if (!res.second)								// if the record wasn't inserted
		++res.first->second;						// increments old statistics

	msgpack::sbuffer sbuf;
	msgpack::pack(sbuf, map);						// packs map

	std::vector<char> data;
	data.reserve(sizeof(uint32_t) + sbuf.size());						// reserves place in vector for the data
	data.insert(data.end(), (char*)&size, (char*)&size + sizeof(size));	// inserts into vector number of chunks for the key 
	data.insert(data.end(), sbuf.data(), sbuf.data() + sbuf.size());	// inserts packed map

	auto write_res = s.write_data(skey, ioremap::elliptics::data_pointer::from_raw(&data.front(), data.size()), 0);	// write data into elliptics

	if (write_res.size() < m_min_writes) { // checks number of successfull results and if it is less then minimum then throw exception
		LOG(DNET_LOG_ERROR, "Can't write data while incrementing activity. Success write: %zu minimum: %d, key: %s\n", write_res.size(), m_min_writes, skey.c_str());
		throw ioremap::elliptics::error(-1, "Data wasn't written to the minimum number of groups");
	}
	LOG(DNET_LOG_DEBUG, "Incremented activity for user: %s key:%s\n", user.c_str(), skey.c_str());
}

uint32_t provider::get_chunks_count(ioremap::elliptics::session& s, const std::string& key)
{
	{
		boost::lock_guard<boost::mutex> lock(m_key_mutex);	// lock mutex to synchronize number of chunks creation
		auto it = m_key_cache.find(key);	// trys to get it from cache
		if (it != m_key_cache.end())		// if it is in cache
			return it->second;				// return data from cache
	}

	try {
		auto file = s.read_latest(key + consts::NULL_KEY_STR, 0, 0)->file();	// trys to read it from zero chunk
		if (!file.empty())														// if it isn't empty
			return file.data<uint32_t>()[0];									// returns it
	}
	catch(std::exception& e) {}
	return -1;	// in the other case returns -1
}

void provider::generate_activity_key(const std::string& base_key, std::string& res_key, uint32_t& size)
{
	res_key = base_key;
	auto s = create_session();
	size = get_chunks_count(s, base_key);					// get number of chunks for the base_key
	if (size == (uint32_t)-1) {								// if number of chunks is unknown
		boost::lock_guard<boost::mutex> lock(m_key_mutex);	// lock mutex to synchronize number of chunks creation
		size = consts::RAND_SIZE;									// set number
		res_key += consts::NULL_KEY_STR;							// set key to zero chunk key
		m_key_cache.insert(std::pair<std::string, uint32_t>(base_key, size));	// saves number of chunks for base_key in cache
	}
	else
	{
		boost::lock_guard<boost::mutex> lock(m_key_mutex);	// lock mutex to synchronize number of chunks creation
		auto rnd = rand() % size;
		res_key += str(boost::format("%010d") % rnd);
	}

	LOG(DNET_LOG_DEBUG, "Generated key: %s", res_key.c_str());
}

void provider::repartition_activity(const std::string& key, uint32_t parts)
{
	repartition_activity(key, key, parts);
}

void provider::repartition_activity(const std::string& old_key, const std::string& new_key, uint32_t parts)
{
	auto s = create_session();
	auto size = get_chunks_count(s, old_key);	// gets number of chunk for old key
	if (size == (uint32_t)-1)					// if it is equal to -1 then data in old_key doesn't exsists so nothin to repartition
		return;

	std::map<std::string, uint32_t> res;					// full map
	std::map<std::string, uint32_t> tmp;					// temporary map
	std::string skey;

	for(uint32_t i = 0; i < size; ++i) {
		skey = old_key + str(boost::format("%010d") % i);	// iterates chunks by old_key 
		get_map_from_key(s, skey, tmp);						// gets map from chunk
		merge(res, tmp);									// merges it with result map
		try {
			s.remove(skey);									// try to remove old chunk becase it is useless now
		}
		catch(std::exception& e) {}
	}

	auto elements_in_chunk = res.size() / parts;						// calculates number of elements in new chunk
	elements_in_chunk = elements_in_chunk == 0 ? 1 : elements_in_chunk;	// if number of chunk more then elements in result map then keep only 1 element in first chunks
	std::vector<char> data;

	uint32_t chunk_no = 0;	// chunk number in which data will be written
	bool written = true;

	for(auto it = res.begin(), it_next = res.begin(), itEnd = res.end(); it != itEnd; it = it_next) {	// iterates throw result map
		it_next = std::next(it, elements_in_chunk);			// sets it_next by it plus number of elements in one chunk
		std::map<std::string, uint32_t> tmp(it, it_next);	// creates sub-map from it to it_next

		msgpack::sbuffer sbuf;
		msgpack::pack(sbuf, tmp);							// packs the sub-map
		data.clear();
		data.reserve(sizeof(uint32_t) + sbuf.size());		// reserves place for packed sub-map
		data.insert(data.end(), (char*)&parts, (char*)&parts + sizeof(size));	// inserts number of chunks
		data.insert(data.end(), sbuf.data(), sbuf.data() + sbuf.size());		// inserts packed sub-map

		skey = new_key + str(boost::format("%010d") % chunk_no);

		auto write_res = s.write_data(skey, ioremap::elliptics::data_pointer::from_raw(&data.front(), data.size()), 0);	// writes data to the elliptics

		if (write_res.size() < m_min_writes) {	// checks number of successfull results and if it is less then minimum then mark that some write was failed
			LOG(DNET_LOG_ERROR, "Can't write data while repartition activity. Success write: %zu minimum: %d, key: %s\n", write_res.size(), m_min_writes, skey.c_str());
			written = false;
		}

		++chunk_no; // increments chunk number
	}

	{
		boost::lock_guard<boost::mutex> lock_g(m_key_mutex);
		auto it = m_key_cache.find(old_key);
		if (it != m_key_cache.end())
			m_key_cache.erase(it);
		m_key_cache.insert(std::make_pair(new_key, parts));
	}

	if (!written)	// checks if some writes was failed if so throw exception
		throw ioremap::elliptics::error(-1, "Some activity wasn't written to the minimum number of groups");
}

void provider::repartition_activity(uint64_t time, uint32_t parts)
{
	repartition_activity(str(boost::format("%020d") % (time / consts::SEC_PER_DAY)), parts);
}

void provider::repartition_activity(uint64_t time, const std::string& new_key, uint32_t parts)
{
	repartition_activity(str(boost::format("%020d") % (time / consts::SEC_PER_DAY)), new_key, parts);
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
	return get_active_users(str(boost::format("%020d") % (time / consts::SEC_PER_DAY)));
}

std::map<std::string, uint32_t> provider::get_active_users(const std::string& key)
{
	LOG(DNET_LOG_INFO, "Getting active users with key: %s\n", key.c_str());
	auto s = create_session();

	std::map<std::string, uint32_t> ret;
	std::map<std::string, uint32_t> tmp;

	uint32_t size = get_chunks_count(s, key);	// gets number of chunks for the key
	if (size == (uint32_t)-1) {					// if it is equal to -1 so there is no activity statistics yet
		LOG(DNET_LOG_INFO, "Active users statistics is empty for key:%s\n", key.c_str());
		return ret;								// returns empty map
	}

	std::string skey;
	
	for(uint32_t i = 0; i < size; ++i) {									// iterates by chunks
		get_map_from_key(s, key + str(boost::format("%010d") % i), tmp);	// gets map from chunk
		merge(ret, tmp);													// merge map from chunk into result map
	}

	return ret;		// returns result map
}

void provider::for_user_logs(const std::string& user, uint64_t begin_time, uint64_t end_time,
		std::function<bool(const std::string& user, uint64_t time, void* data, uint32_t size)> func)
{
	LOG(DNET_LOG_INFO, "Iterating by user logs for user: %s, begin time: %" PRIu64 " end time: %" PRIu64 " \n", user.c_str(), begin_time, end_time);
	auto s = create_session();

	for(auto time = begin_time; time <= end_time; time += consts::SEC_PER_DAY) {	// iterates by user logs
		try {
			auto skey = str(boost::format("%s%020d") % user % (time / consts::SEC_PER_DAY));
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
	for_active_users(str(boost::format("%020d") % (time / consts::SEC_PER_DAY)), func);
}

void provider::get_map_from_key(ioremap::elliptics::session& s, const std::string& key, std::map<std::string, uint32_t>& ret)
{
	ret.clear();	// clear result map
	try {
		auto file = s.read_latest(key, 0, 0)->file();	// reads file from elliptics
		if (!file.empty()) {							// if the file isn't empty
			file = file.skip<uint32_t>();				// skips number of chunks
			msgpack::unpacked msg;
			msgpack::unpack(&msg, file.data<const char>(), file.size());	// unpack map
			msg.get().convert(&ret);
		}
	}
	catch(std::exception& e) {}
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

	session.set_cflags(0);							// sets cflags to 0
	session.set_ioflags(ioflags | DNET_IO_FLAGS_CACHE );	// sets ioflags with DNET_IO_FLAGS_CACHE

	session.set_groups(m_groups);	// sets groups

	return session;
}

} /* namespace history */
