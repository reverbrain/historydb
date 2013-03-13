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

	const char* 	kRemoteAddr		= "localhost";
	const int		kRemotePort		= 1025;

	const uint32_t	kSecPerDay		= 24 * 60 * 60;

	const uint32_t	kRandSize		= 1;



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

void Provider::Connect()
{
	try
	{
		log_.reset(new ioremap::elliptics::file_logger(Const::kLogFile, Const::kLogLevel));
		node_.reset(new ioremap::elliptics::node(*log_));

		if(dnet_add_state(node_->get_native(), const_cast<char*>(Const::kRemoteAddr), Const::kRemotePort, AF_INET, 0))
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

void Provider::AddActivity(const std::string& user, Activity activity, uint32_t time) const
{
	AddActivityToUser(user, activity, time);
	AddActivityToLog(user, activity, time);
}

std::list<ActivityData> Provider::GetActivities(const std::string& user, uint32_t begin_time, uint32_t end_time) const
{
	std::list<ActivityData> ret;

	auto s = CreateSession();

	for(auto time = begin_time; time <= end_time; time += Const::kSecPerDay)
	{
		try
		{
			auto read_res = s->read_data(GetUserKey(user, time), 0, 0);
			auto file = read_res->file();

			if(!file.empty())
			{
				auto data = file.data<uint32_t>();
				auto size = file.size() / sizeof(uint32_t);
				for(size_t i = 0; i < size;)
				{
					ActivityData adata;
					adata.activity = static_cast<Activity>(data[i++]);
					adata.time = data[i++];
					adata.user = user;
					ret.push_back(adata);
				}
			}
		}
		catch(std::exception& e)
		{}
	}

	return ret;
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

std::map<std::string, uint32_t> Provider::GetActivities(Activity activity, uint32_t begin_time, uint32_t end_time) const
{
	auto s = CreateSession();

	std::map<std::string, uint32_t> res;

	for(auto time = begin_time; time <= end_time; time += Const::kSecPerDay)
	{
		for(uint32_t i = 0; i < Const::kRandSize; ++i)
		{
			try
			{
				auto read_res = s->read_data(GetLogKey(activity, time, i), 0, 0);
				auto file = read_res->file();
				if(!file.empty())
				{
					std::map<std::string, uint32_t> tmp;
					msgpack::unpacked msg;
					msgpack::unpack(&msg, file.data<const char>(), file.size());
					msg.get().convert(&tmp);

					Merge(res, tmp);
				}
			}
			catch(std::exception& e)
			{}
		}
	}

	return res;
}

void Provider::ForEachActiveUser() const
{
	std::cout << ">>" << __FUNCTION__ << "()\n\n";

	auto s = CreateSession();

	std::cout << "\n<<" << __FUNCTION__ << "()\n\n";
}

void Provider::ForEachActivities(const std::string& user, uint32_t begin_time, uint32_t end_time, std::function<bool(ActivityData)> func) const
{
	auto s = CreateSession();

	for(auto time = begin_time; time <= end_time; time += Const::kSecPerDay)
	{
		try
		{
			auto read_res = s->read_data(GetUserKey(user, time), 0, 0);
			auto file = read_res->file();

			if(!file.empty())
			{
				auto data = file.data<uint32_t>();
				auto size = file.size() / sizeof(uint32_t);
				for(size_t i = 0; i < size;)
				{
					ActivityData adata;
					adata.activity = static_cast<Activity>(data[i++]);
					adata.time = data[i++];
					adata.user = user;

					func(adata);
				}
			}
		}
		catch(std::exception& e)
		{}
	}
}

std::shared_ptr<ioremap::elliptics::session> Provider::CreateSession(uint64_t ioflags) const
{
	auto session = std::make_shared<ioremap::elliptics::session>(*node_);

	std::vector<int> groups;
	groups.push_back(2);

	session->set_cflags(0);
	session->set_ioflags(ioflags);

	session->set_groups(groups);

	return session;
}

inline ioremap::elliptics::key Provider::GetUserKey(const std::string& user, uint32_t time) const
{
	auto key = str(boost::format("%s%010d") % user % (time / Const::kSecPerDay));

	if(Const::kKeys.find(key) == Const::kKeys.end())
		Const::kKeys.insert(key);

	std::cout << "KEY: " << key << std::endl;
	
	return ioremap::elliptics::key(key);
}

inline ioremap::elliptics::key Provider::GetLogKey(Activity activity, uint32_t time, uint32_t file) const
{
	file = file == (uint32_t)-1 ? (rand() % Const::kRandSize) : file;
	auto key = str(boost::format("%010d%010d%010d") % activity % (time / Const::kSecPerDay) % file);
	
	if(Const::kKeys.find(key) == Const::kKeys.end())
		Const::kKeys.insert(key);

	std::cout << "KEY: " << key << std::endl;

	return ioremap::elliptics::key(key);
}

void Provider::AddActivityToUser(const std::string& user, Activity activity, uint32_t time) const
{
	std::vector<uint32_t> raw_data(2);

	raw_data[0] = activity;
	raw_data[1] = time;

	auto s = CreateSession(DNET_IO_FLAGS_APPEND);

	auto write_res = s->write_data(GetUserKey(user, time), ioremap::elliptics::data_pointer::from_raw(&raw_data.front(), raw_data.size() * sizeof(uint32_t)), 0);
}

void Provider::AddActivityToLog(const std::string& user, Activity activiy, uint32_t time) const
{
	auto s = CreateSession();
	std::map<std::string, uint32_t> map;

	const auto key = GetLogKey(activiy, time);

	try
	{
		auto file = s->read_data(key, 0, 0)->file();
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

	auto write_res = s->write_data(key, ioremap::elliptics::data_pointer::from_raw(sbuf.data(), sbuf.size()), 0);
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
