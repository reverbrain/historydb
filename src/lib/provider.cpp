#include "provider.h"
#include <vector>
#include <time.h>
#include <msgpack.hpp>
#include <iomanip>
#include <boost/format.hpp>

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
	const auto		k0				= "000000000000000";



	//Temporary must be deleted later!!!
	std::set<std::string>	kKeys;
	std::string kKeyName	= "Blabla";
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

void Provider::Connect(const char* server_addr, const int server_port)
{
	try
	{
		log_.reset(new ioremap::elliptics::file_logger(Const::kLogFile, Const::kLogLevel));
		node_.reset(new ioremap::elliptics::node(*log_));

		if(dnet_add_state(node_->get_native(), const_cast<char*>(server_addr), server_port, AF_INET, 0))
		{
			std::cout << "Error! Cannot connect to elliptics\n";
			return;
		}
	}
	catch(const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
	}
}

void Provider::Disconnect()
{
	//Temporary must be deleted later!!!
	Clean();

	node_.reset(nullptr);
	log_.reset(nullptr);
}

void Provider::SetSessionParameters(const std::vector<int>& groups, uint32_t min_writes)
{
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
	auto s = CreateSession(DNET_IO_FLAGS_APPEND);

	auto skey = str(boost::format("%s%015d") % user % (time / Const::kSecPerDay));

	if(Const::kKeys.find(skey) == Const::kKeys.end())
		Const::kKeys.insert(skey);

	std::vector<char> vdata;
	vdata.reserve(sizeof(uint64_t) + sizeof(uint32_t) + size);
	vdata.insert(vdata.end(), (char*)&time, (char*)&time + sizeof(uint64_t));
	vdata.insert(vdata.end(), (char*)&size, (char*)&size + sizeof(uint32_t));
	vdata.insert(vdata.end(), (char*)data, (char*)data + size);

	auto write_res = s->write_data(ioremap::elliptics::key(skey), ioremap::elliptics::data_pointer::from_raw(&vdata.front(), vdata.size()), 0);
}

void Provider::IncrementActivity(const std::string& user, uint64_t time) const
{
	IncrementActivity(user, str(boost::format("%015d") % (time / Const::kSecPerDay)));
}

void Provider::GenerateActivityKey(const std::string& base_key, std::string& res_key, uint32_t& size) const
{
	res_key = base_key;
	size = 100;
	auto s = CreateSession();
	
	try
	{
		auto file = s->read_data(ioremap::elliptics::key(base_key + Const::k0), 0, 0)->file();
		if(!file.empty())
		{
			size = file.data<uint32_t>()[0];
			res_key += str(boost::format("%010d") % (rand() % size));
		}
		else
		{
			res_key += Const::k0;
		}
	}
	catch(std::exception& e)
	{
		res_key += Const::k0;
	}
}

void Provider::IncrementActivity(const std::string& user, const std::string& key) const
{
	auto s = CreateSession();
	std::map<std::string, uint32_t> map;
	std::string skey;
	uint32_t size;

	GenerateActivityKey(key, skey, size);

	const auto w_key = ioremap::elliptics::key(skey);

	if(Const::kKeys.find(skey) == Const::kKeys.end())
		Const::kKeys.insert(skey);

	try
	{
		auto file = s->read_data(key, 0, 0)->file();
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

	std::vector<char> data(sbuf.size() + sizeof(uint32_t));
	memcpy(&data.front(), &size, sizeof(size));
	memcpy(&data.front() + sizeof(size), sbuf.data(), sbuf.size());

	auto write_res = s->write_data(key, ioremap::elliptics::data_pointer::from_raw(&data.front(), data.size()), 0);
}

void Provider::RepartitionActivity(const std::string& key, uint32_t parts) const
{

}

void Provider::RepartitionActivity(const std::string& old_key, const std::string& new_key, uint32_t parts) const
{

}

void Provider::RepartitionActivity(uint64_t time, uint32_t parts) const
{

}

void Provider::RepartitionActivity(uint32_t time, const std::string& new_key, uint32_t parts) const
{

}

void Provider::ForUserLogs(const std::string& user, uint64_t begin_time, uint64_t end_time, std::function<bool(const std::string& user, uint64_t time, void* data, uint32_t size)> func) const
{
	auto s = CreateSession();

	uint64_t tm = 0;
	uint32_t size = 0;

	for(auto time = begin_time; time <= end_time; time += Const::kSecPerDay)
	{
		try
		{
			auto skey = str(boost::format("%s%015d") % user % (time / Const::kSecPerDay));
			const auto w_key = ioremap::elliptics::key(skey);
			auto read_res = s->read_data(w_key, 0, 0);
			auto file = read_res->file();

			while(!file.empty())
			{
				tm = file.data<uint64_t>()[0];
				file = file.skip<uint64_t>();

				size = file.data<uint32_t>()[0];
				file = file.skip<uint32_t>();

				if(	tm >= begin_time &&
					tm <= end_time)
				{
					if(!func(user, tm, file.data(), size))
						return;
				}

				file = file.skip(size);
			}
		}
		catch(std::exception& e)
		{}
	}
}

void Provider::ForActiveUser(uint64_t time, std::function<bool(const std::string&, uint32_t)> func) const
{

}

void Provider::ForActiveUser(const std::string& key, std::function<bool(const std::string&, uint32_t)> func) const
{

}

std::shared_ptr<ioremap::elliptics::session> Provider::CreateSession(uint64_t ioflags) const
{
	auto session = std::make_shared<ioremap::elliptics::session>(*node_);

	session->set_cflags(0);
	session->set_ioflags(ioflags);

	session->set_groups(groups_);

	return session;
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

void Provider::Clean() const
{
	if(node_.get() != NULL && log_.get() != NULL)
	{
		auto s = CreateSession();
		for(auto it = Const::kKeys.begin(), itEnd = Const::kKeys.end(); it != itEnd; ++it)
		{
			try
			{
				s->remove(ioremap::elliptics::key(*it));
			}
			catch(std::exception& e)
			{}
		}
	}
	Const::kKeys.clear();
}


}
