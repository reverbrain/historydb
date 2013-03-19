#include "provider.h"
#include <vector>
#include <time.h>
#include <msgpack.hpp>
#include <iomanip>
#include <boost/format.hpp>
#include <boost/thread/locks.hpp>

//Temporary includes
#include <iostream>
#include <set>

namespace history
{

namespace consts
{
	const char* 	LOG_FILE		= "/dev/stderr";
	const int		LOG_LEVEL		= DNET_LOG_ERROR;

	const uint32_t	SEC_PER_DAY		= 24 * 60 * 60;

	const uint32_t	RAND_SIZE		= 100;
	const auto		NULL_KEY_STR	= "0000000000";
}

template<typename K, typename V>
void merge(std::map<K, V>& res_map, const std::map<K, V>& merge_map)
{
	auto mIt = res_map.begin();
	for(auto it = merge_map.begin(), itEnd = merge_map.end(); it != itEnd; ++it)
	{
		auto size = res_map.size();
		mIt = res_map.insert(mIt, *it);
		if(size == res_map.size())
			mIt->second += it->second;
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

	if(dnet_add_state(m_node.get_native(), const_cast<char*>(server_addr), server_port, family, 0))
		throw ioremap::elliptics::error(-1, "Cannot connect to elliptics\n");
}

provider::~provider()
{
}

void provider::set_session_parameters(const std::vector<int>& groups, uint32_t min_writes)
{
	m_groups = groups;
	m_min_writes = min_writes > m_groups.size() ? m_groups.size() : min_writes;
}

void provider::add_user_activity(const std::string& user, uint64_t time, void* data, uint32_t size, const std::string& key) const
{
	add_user_data(user, time, data, size);

	if(key.empty())
		increment_activity(user, time);
	else
		increment_activity(user, key);
}

void provider::add_user_data(const std::string& user, uint64_t time, void* data, uint32_t size) const
{
	auto s = create_session(DNET_IO_FLAGS_APPEND);

	auto skey = str(boost::format("%s%020d") % user % (time / consts::SEC_PER_DAY));

	auto write_res = s->write_data(skey, ioremap::elliptics::data_pointer::from_raw(data, size), 0);

	if(write_res.size() < m_min_writes)
		throw ioremap::elliptics::error(-1, "Data wasn't written to the minimum number of groups");
}

void provider::increment_activity(const std::string& user, uint64_t time) const
{
	increment_activity(user, str(boost::format("%020d") % (time / consts::SEC_PER_DAY)));
}

uint32_t provider::get_key_spread_size(ioremap::elliptics::session& s, const std::string& key) const
{
	auto it = m_key_cache.find(key);
	if(it != m_key_cache.end())
		return it->second;

	try
	{
		auto file = s.read_latest(key + consts::NULL_KEY_STR, 0, 0)->file();
		if(!file.empty())
		{
			return file.data<uint32_t>()[0];
		}
	}
	catch(std::exception& e)
	{}
	return -1;
}

void provider::generate_activity_key(const std::string& base_key, std::string& res_key, uint32_t& size) const
{
	res_key = base_key;
	auto s = create_session();
	size = get_key_spread_size(*s, base_key);
	if(size != (uint32_t)-1)
	{
		{
			boost::lock_guard<boost::mutex> lock(m_key_mutex);
			static uint32_t val = 0;
			val = (val + 1) % size;
			res_key += str(boost::format("%010d") % val);
		}
		return;
	}

	size = get_key_spread_size(*s, base_key);

	if(size == (uint32_t)-1)
	{
		size = consts::RAND_SIZE;
		res_key += consts::NULL_KEY_STR;
	}
	else
	{
		res_key += str(boost::format("%010d") % (rand() % size));
	}

	m_key_cache.insert(std::pair<std::string, uint32_t>(base_key, size));
}

void provider::increment_activity(const std::string& user, const std::string& key) const
{
	std::map<std::string, uint32_t> map;
	std::string skey;
	uint32_t size;

	generate_activity_key(key, skey, size);

	auto s = create_session();

	try
	{
		auto file = s->read_latest(skey, 0, 0)->file();
		file = file.skip<uint32_t>();
		if(!file.empty())
		{
			msgpack::unpacked msg;
			msgpack::unpack(&msg, file.data<const char>(), file.size());
			msg.get().convert(&map);
		}
	}
	catch(std::exception& e)
	{}

	auto res = map.insert(std::make_pair(user, 1));
	if(!res.second)
	{
		++res.first->second;
	}

	msgpack::sbuffer sbuf;
	msgpack::pack(sbuf, map);

	std::vector<char> data;
	data.reserve(sizeof(uint32_t) + sbuf.size());
	data.insert(data.end(), (char*)&size, (char*)&size + sizeof(size));
	data.insert(data.end(), sbuf.data(), sbuf.data() + sbuf.size());

	auto write_res = s->write_data(skey, ioremap::elliptics::data_pointer::from_raw(&data.front(), data.size()), 0);

	if(write_res.size() < m_min_writes)
		throw ioremap::elliptics::error(-1, "Data wasn't written to the minimum number of groups");
}

void provider::repartition_activity(const std::string& key, uint32_t parts) const
{
	repartition_activity(key, key, parts);
}

void provider::repartition_activity(const std::string& old_key, const std::string& new_key, uint32_t parts) const
{
	auto s = create_session();
	auto size = get_key_spread_size(*s, old_key);
	if(size == (uint32_t)-1)
		return;

	std::map<std::string, uint32_t> res;
	std::map<std::string, uint32_t> tmp;
	std::string skey;

	for(uint32_t i = 0; i < size; ++i)
	{
		skey = old_key + str(boost::format("%010d") % i);
		get_map_from_key(*s, skey, tmp);
		merge(res, tmp);
		try
		{
			s->remove(skey);
		}
		catch(std::exception& e)
		{}
	}

	auto elements_in_part = res.size() / parts;
	elements_in_part = elements_in_part == 0 ? 1 : elements_in_part;
	std::vector<char> data;

	uint32_t part_no = 0;
	bool written = true;

	for(auto it = res.begin(), it_next = res.begin(), itEnd = res.end(); it != itEnd; it = it_next)
	{
		it_next = std::next(it, elements_in_part);
		std::map<std::string, uint32_t> tmp(it, it_next);

		msgpack::sbuffer sbuf;
		msgpack::pack(sbuf, tmp);
		data.clear();
		data.reserve(sizeof(uint32_t) + sbuf.size());
		data.insert(data.end(), (char*)&parts, (char*)&parts + sizeof(size));
		data.insert(data.end(), sbuf.data(), sbuf.data() + sbuf.size());

		skey = new_key + str(boost::format("%010d") % part_no);

		auto write_res = s->write_data(skey, ioremap::elliptics::data_pointer::from_raw(&data.front(), data.size()), 0);

		if(write_res.size() < m_min_writes)
			written = false;

		++part_no;
	}

	{
		boost::lock_guard<boost::mutex> lock_g(m_key_mutex);
		auto it = m_key_cache.find(old_key);
		if(it != m_key_cache.end())
			m_key_cache.erase(it);
		m_key_cache.insert(std::make_pair(new_key, parts));
	}

	if(!written)
		throw ioremap::elliptics::error(-1, "Some activity wasn't written to the minimum number of groups");
}

void provider::repartition_activity(uint64_t time, uint32_t parts) const
{
	repartition_activity(str(boost::format("%020d") % (time / consts::SEC_PER_DAY)), parts);
}

void provider::repartition_activity(uint64_t time, const std::string& new_key, uint32_t parts) const
{
	repartition_activity(str(boost::format("%020d") % (time / consts::SEC_PER_DAY)), new_key, parts);
}

std::list<std::vector<char>> provider::get_user_logs(const std::string& user, uint64_t begin_time, uint64_t end_time) const
{
	std::list<std::vector<char>> ret;

	for_user_logs(user, begin_time, end_time, [&ret](const std::string& user, uint64_t time, void* data, uint32_t size)
	{
		ret.push_back(std::vector<char>((char*)data, (char*)data + size));
		return true;
	});

	return ret;
}

std::map<std::string, uint32_t> provider::get_active_user(uint64_t time) const
{
	return get_active_user(str(boost::format("%020d") % (time / consts::SEC_PER_DAY)));
}

std::map<std::string, uint32_t> provider::get_active_user(const std::string& key) const
{
	auto s = create_session();

	std::map<std::string, uint32_t> ret;
	std::map<std::string, uint32_t> tmp;

	uint32_t size = get_key_spread_size(*s, key);
	if(size == (uint32_t)-1)
		return ret;

	std::string skey;
	
	for(uint32_t i = 0; i < size; ++i)
	{
		get_map_from_key(*s, key + str(boost::format("%010d") % i), tmp);
		merge(ret, tmp);
	}

	return ret;
}

void provider::for_user_logs(const std::string& user, uint64_t begin_time, uint64_t end_time,
		std::function<bool(const std::string& user, uint64_t time, void* data, uint32_t size)> func) const
{
	auto s = create_session();

	for(auto time = begin_time; time <= end_time; time += consts::SEC_PER_DAY)
	{
		try
		{
			auto skey = str(boost::format("%s%020d") % user % (time / consts::SEC_PER_DAY));
			auto read_res = s->read_latest(skey, 0, 0);
			auto file = read_res->file();

			if(file.empty())
				continue;

			if(!func(user, time, file.data(), file.size()))
				return;
		}
		catch(std::exception& e)
		{}
	}
}

void provider::for_active_user(uint64_t time, std::function<bool(const std::string& user, uint32_t number)> func) const
{
	for_active_user(str(boost::format("%020d") % (time / consts::SEC_PER_DAY)), func);
}

void provider::get_map_from_key(ioremap::elliptics::session& s, const std::string& key, std::map<std::string, uint32_t>& ret) const
{
	ret.clear();
	try
	{
		auto file = s.read_latest(key, 0, 0)->file();
		if(!file.empty())
		{
			file = file.skip<uint32_t>();
			msgpack::unpacked msg;
			msgpack::unpack(&msg, file.data<const char>(), file.size());
			msg.get().convert(&ret);
		}
	}
	catch(std::exception& e)
	{}
}

void provider::for_active_user(const std::string& key, std::function<bool(const std::string& user, uint32_t number)> func) const
{
	std::map<std::string, uint32_t> res = get_active_user(key);

	for(auto it = res.begin(), itEnd = res.end(); it != itEnd; ++it)
	{
		if(!func(it->first, it->second))
			break;
	}
}

std::shared_ptr<ioremap::elliptics::session> provider::create_session(uint64_t ioflags) const
{
	auto session = std::make_shared<ioremap::elliptics::session>(m_node);

	session->set_cflags(0);
	session->set_ioflags(ioflags | DNET_IO_FLAGS_CACHE );

	session->set_groups(m_groups);

	return session;
}

}
