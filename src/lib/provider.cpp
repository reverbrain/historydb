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

namespace History
{

namespace Const
{
	const char* 	kLogFile		= "/dev/stderr";
	const int		kLogLevel		= DNET_LOG_ERROR;

	const uint32_t	kSecPerDay		= 24 * 60 * 60;

	const uint32_t	kRandSize		= 100;
	const auto		k0				= "0000000000";
}

template<typename K, typename V>
void Merge(std::map<K, V>& res_map, const std::map<K, V>& merge_map)
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

std::shared_ptr<IProvider> CreateProvider()
{
	return std::make_shared<Provider>();
}

Provider::Provider()
{
	srand(time(NULL));
}

Provider::~Provider()
{
	Disconnect();
}

void Provider::Connect(const char* server_addr, const int server_port, const int family)
{
	boost::unique_lock<boost::shared_mutex> lock(connect_mutex_);
	try
	{
		log_.reset(new ioremap::elliptics::file_logger(Const::kLogFile, Const::kLogLevel));
		node_.reset(new ioremap::elliptics::node(*log_));

		if(dnet_add_state(node_->get_native(), const_cast<char*>(server_addr), server_port, family, 0))
			throw ioremap::elliptics::error(-1, "Cannot connect to elliptics\n");
	}
	catch(const ioremap::elliptics::error& e)
	{
		throw;
	}
	catch(const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}
}

void Provider::Disconnect()
{
	boost::unique_lock<boost::shared_mutex> lock(connect_mutex_);

	node_.reset(nullptr);
	log_.reset(nullptr);
}

void Provider::SetSessionParameters(const std::vector<int>& groups, uint32_t min_writes)
{
	boost::unique_lock<boost::shared_mutex> lock(connect_mutex_);
	groups_ = groups;
	min_writes_ = min_writes > groups_.size() ? groups_.size() : min_writes;
}

void Provider::AddUserActivity(const std::string& user, uint64_t time, void* data, uint32_t size, const std::string& key) const
{
	AddUserData(user, time, data, size);

	if(key.empty())
		IncrementActivity(user, time);
	else
		IncrementActivity(user, key);
}

void Provider::AddUserData(const std::string& user, uint64_t time, void* data, uint32_t size) const
{
	boost::shared_lock<boost::shared_mutex> lock(connect_mutex_);
	auto s = CreateSession(DNET_IO_FLAGS_APPEND);

	auto skey = str(boost::format("%s%020d") % user % (time / Const::kSecPerDay));

	auto write_res = s->write_data(skey, ioremap::elliptics::data_pointer::from_raw(data, size), 0);

	if(write_res.size() < min_writes_)
		throw ioremap::elliptics::error(-1, "Data wasn't written to the minimum number of groups");
}

void Provider::IncrementActivity(const std::string& user, uint64_t time) const
{
	IncrementActivity(user, str(boost::format("%020d") % (time / Const::kSecPerDay)));
}

uint32_t Provider::GetKeySpreadSize(ioremap::elliptics::session& s, const std::string& key) const
{
	auto it = key_cache_.find(key);
	if(it != key_cache_.end())
		return it->second;

	try
	{
		auto file = s.read_latest(key + Const::k0, 0, 0)->file();
		if(!file.empty())
		{
			return file.data<uint32_t>()[0];
		}
	}
	catch(std::exception& e)
	{}
	return -1;
}

void Provider::GenerateActivityKey(const std::string& base_key, std::string& res_key, uint32_t& size) const
{
	res_key = base_key;
	auto s = CreateSession();
	size = GetKeySpreadSize(*s, base_key);
	if(size != (uint32_t)-1)
	{
		{
			boost::lock_guard<boost::mutex> lock(key_mutex_);
			static uint32_t val = 0;
			val = (val + 1) % size;
			res_key += str(boost::format("%010d") % val);
		}
		return;
	}

	boost::unique_lock<boost::shared_mutex> lock(connect_mutex_);
	size = GetKeySpreadSize(*s, base_key);

	if(size == (uint32_t)-1)
	{
		size = Const::kRandSize;
		res_key += Const::k0;
	}
	else
	{
		res_key += str(boost::format("%010d") % (rand() % size));
	}

	key_cache_.insert(std::pair<std::string, uint32_t>(base_key, size));
}

void Provider::IncrementActivity(const std::string& user, const std::string& key) const
{
	std::map<std::string, uint32_t> map;
	std::string skey;
	uint32_t size;

	GenerateActivityKey(key, skey, size);

	boost::shared_lock<boost::shared_mutex> lock(connect_mutex_);
	auto s = CreateSession();

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

	if(write_res.size() < min_writes_)
		throw ioremap::elliptics::error(-1, "Data wasn't written to the minimum number of groups");
}

void Provider::RepartitionActivity(const std::string& key, uint32_t parts) const
{
	RepartitionActivity(key, key, parts);
}

void Provider::RepartitionActivity(const std::string& old_key, const std::string& new_key, uint32_t parts) const
{
	boost::shared_lock<boost::shared_mutex> lock(connect_mutex_);
	auto s = CreateSession();
	auto size = GetKeySpreadSize(*s, old_key);
	if(size == (uint32_t)-1)
		return;

	std::map<std::string, uint32_t> res;
	std::map<std::string, uint32_t> tmp;
	std::string skey;

	for(uint32_t i = 0; i < size; ++i)
	{
		skey = old_key + str(boost::format("%010d") % i);
		GetMapFromKey(*s, skey, tmp);
		Merge(res, tmp);
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

	for(auto it = res.begin(), itNext = res.begin(), itEnd = res.end(); it != itEnd; it = itNext)
	{
		itNext = std::next(it, elements_in_part);
		std::map<std::string, uint32_t> tmp(it, itNext);

		msgpack::sbuffer sbuf;
		msgpack::pack(sbuf, tmp);
		data.clear();
		data.reserve(sizeof(uint32_t) + sbuf.size());
		data.insert(data.end(), (char*)&parts, (char*)&parts + sizeof(size));
		data.insert(data.end(), sbuf.data(), sbuf.data() + sbuf.size());

		skey = new_key + str(boost::format("%010d") % part_no);

		auto write_res = s->write_data(skey, ioremap::elliptics::data_pointer::from_raw(&data.front(), data.size()), 0);

		if(write_res.size() < min_writes_)
			written = false;

		++part_no;
	}

	{
		boost::lock_guard<boost::mutex> lock_g(key_mutex_);
		auto it = key_cache_.find(old_key);
		if(it != key_cache_.end())
			key_cache_.erase(it);
		key_cache_.insert(std::make_pair(new_key, parts));
	}

	if(!written)
		throw ioremap::elliptics::error(-1, "Some activity wasn't written to the minimum number of groups");
}

void Provider::RepartitionActivity(uint64_t time, uint32_t parts) const
{
	RepartitionActivity(str(boost::format("%020d") % (time / Const::kSecPerDay)), parts);
}

void Provider::RepartitionActivity(uint64_t time, const std::string& new_key, uint32_t parts) const
{
	RepartitionActivity(str(boost::format("%020d") % (time / Const::kSecPerDay)), new_key, parts);
}

std::list<std::vector<char>> Provider::GetUserLogs(const std::string& user, uint64_t begin_time, uint64_t end_time) const
{
	std::list<std::vector<char>> ret;

	ForUserLogs(user, begin_time, end_time, [&ret](const std::string& user, uint64_t time, void* data, uint32_t size)
	{
		ret.push_back(std::vector<char>((char*)data, (char*)data + size));
		return true;
	});

	return ret;
}

std::map<std::string, uint32_t> Provider::GetActiveUser(uint64_t time) const
{
	return GetActiveUser(str(boost::format("%020d") % (time / Const::kSecPerDay)));
}

std::map<std::string, uint32_t> Provider::GetActiveUser(const std::string& key) const
{
	boost::shared_lock<boost::shared_mutex> lock(connect_mutex_);
	auto s = CreateSession();

	std::map<std::string, uint32_t> ret;
	std::map<std::string, uint32_t> tmp;

	uint32_t size = GetKeySpreadSize(*s, key);
	if(size == (uint32_t)-1)
		return ret;

	std::string skey;
	
	for(uint32_t i = 0; i < size; ++i)
	{
		GetMapFromKey(*s, key + str(boost::format("%010d") % i), tmp);
		Merge(ret, tmp);
	}

	return ret;
}

void Provider::ForUserLogs(const std::string& user, uint64_t begin_time, uint64_t end_time,
		std::function<bool(const std::string& user, uint64_t time, void* data, uint32_t size)> func) const
{
	boost::shared_lock<boost::shared_mutex> lock(connect_mutex_);
	auto s = CreateSession();

	for(auto time = begin_time; time <= end_time; time += Const::kSecPerDay)
	{
		try
		{
			auto skey = str(boost::format("%s%020d") % user % (time / Const::kSecPerDay));
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

void Provider::ForActiveUser(uint64_t time, std::function<bool(const std::string& user, uint32_t number)> func) const
{
	ForActiveUser(str(boost::format("%020d") % (time / Const::kSecPerDay)), func);
}

void Provider::GetMapFromKey(ioremap::elliptics::session& s, const std::string& key, std::map<std::string, uint32_t>& ret) const
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

void Provider::ForActiveUser(const std::string& key, std::function<bool(const std::string& user, uint32_t number)> func) const
{
	std::map<std::string, uint32_t> res = GetActiveUser(key);

	for(auto it = res.begin(), itEnd = res.end(); it != itEnd; ++it)
	{
		if(!func(it->first, it->second))
			break;
	}
}

std::shared_ptr<ioremap::elliptics::session> Provider::CreateSession(uint64_t ioflags) const
{
	auto session = std::make_shared<ioremap::elliptics::session>(*node_);

	session->set_cflags(0);
	session->set_ioflags(ioflags | DNET_IO_FLAGS_CACHE );

	session->set_groups(groups_);

	return session;
}

}
