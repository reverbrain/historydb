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
	std::cout << ">>" << __FUNCTION__ << "()\n\n";

	srand(time(NULL));

	std::cout << "\n<<" << __FUNCTION__ << "()\n\n";
}

Provider::~Provider()
{
	std::cout << ">>" << __FUNCTION__ << "()\n\n";

	Disconnect();

	std::cout << "\n<<" << __FUNCTION__ << "()\n\n";
}

void Provider::Connect()
{
	std::cout << ">>" << __FUNCTION__ << "()\n\n";

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

	std::cout << "\n<<" << __FUNCTION__ << "()\n\n";
}

void Provider::Disconnect()
{
	std::cout << ">>" << __FUNCTION__ << "()\n\n";
	
	//Temporary must be deleted later!!!
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

	node_.reset(nullptr);
	log_.reset(nullptr);

	std::cout << "\n<<" << __FUNCTION__ << "()\n\n";
}

void Provider::AddActivity(const std::string& user, Activity activity, uint32_t time) const
{
	std::cout << ">>" << __FUNCTION__ << "()\n\n";

	AddActivityToUser(user, activity, time);
	AddActivityToLog(user, activity, time);

	std::cout << "\n<<" << __FUNCTION__ << "()\n\n";
}

void Provider::GetActivities(const std::string& user, uint32_t begin_time, uint32_t end_time) const
{
	std::cout << ">>" << __FUNCTION__ << "()\n\n";

	auto s = CreateSession();

	for(uint32_t time = begin_time; time <= end_time; time += Const::kSecPerDay)
	{
		try
		{
			auto read_res = s->read_data(GetUserKey(user, time), 0, 0);
			auto file = read_res->file();
			if(file.empty())
				std::cout << "Empty file!\n";
			else
			{
				std::cout << "Data read:\n";// << file.to_string() << "    " << file.to_string().size() << std::endl;
				for(size_t i = 0; i < file.size() / sizeof(uint32_t); ++i)
				{
					std::cout << file.data<uint32_t>()[i] << " ";
				}
				std::cout << "    " << file.size();
				std::cout << std::endl;
			}
		}
		catch(std::exception& e)
		{}
	}


	std::cout << "\n<<" << __FUNCTION__ << "()\n\n";
}

void Provider::GetActivities(uint32_t begin_time, uint32_t end_time) const
{

}

void Provider::ForEachActiveUser() const
{
	std::cout << ">>" << __FUNCTION__ << "()\n\n";

	auto s = CreateSession();

	std::cout << "\n<<" << __FUNCTION__ << "()\n\n";
}

void Provider::ForEachUserActivities() const
{
	std::cout << ">>" << __FUNCTION__ << "()\n\n";

	auto s = CreateSession();

	std::cout << "\n<<" << __FUNCTION__ << "()\n\n";
}

std::shared_ptr<ioremap::elliptics::session> Provider::CreateSession(uint64_t ioflags) const
{
	std::cout << ">>" << __FUNCTION__ << "()\n";

	auto session = std::make_shared<ioremap::elliptics::session>(*node_);

	std::vector<int> groups;
	groups.push_back(2);

	session->set_cflags(0);
	session->set_ioflags(ioflags);

	session->set_groups(groups);

	std::cout << "<<" << __FUNCTION__ << "()\n";

	return session;
}

inline ioremap::elliptics::key Provider::GetUserKey(const std::string& user, uint32_t time) const
{
	auto key = str(boost::format("%s%010d") % user % (time / Const::kSecPerDay));

	if(Const::kKeys.find(key) == Const::kKeys.end())
		Const::kKeys.insert(key);

	std::cout << "Key: " << key << "\n";
	
	return ioremap::elliptics::key(key);
}

inline ioremap::elliptics::key Provider::GetLogKey(Activity activity, uint32_t time) const
{
	auto key = str(boost::format("%010d%010d%010d") % activity % (time / Const::kSecPerDay) % (rand() % Const::kRandSize));
	
	if(Const::kKeys.find(key) == Const::kKeys.end())
		Const::kKeys.insert(key);

	std::cout << "Key: " << key << "\n";

	return ioremap::elliptics::key(key);
}

void Provider::AddActivityToUser(const std::string& user, Activity activity, uint32_t time) const
{
	std::cout << ">>" << __FUNCTION__ << "()\n";

	std::vector<uint32_t> raw_data(2);

	raw_data[0] = activity;
	raw_data[1] = time;

	auto s = CreateSession(DNET_IO_FLAGS_APPEND);

	auto write_res = s->write_data(GetUserKey(user, time), ioremap::elliptics::data_pointer::from_raw(&raw_data.front(), raw_data.size() * sizeof(uint32_t)), 0);

	std::cout << "<<" << __FUNCTION__ << "()\n";
}

void Provider::AddActivityToLog(const std::string& user, Activity activiy, uint32_t time) const
{
	std::cout << ">>" << __FUNCTION__ << "()\n";

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
			auto obj = msg.get();
			std::cout << obj << std::endl;
			obj.convert(&map);
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

	std::cout << "<<" << __FUNCTION__ << "()\n";
}


}
