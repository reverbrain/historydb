#include "test2.h"
#include <iprovider.h>
#include <iostream>
#include <boost/thread.hpp>
#include <boost/format.hpp>

namespace Const
{
	const uint32_t kThreadNo		= 10;
	const uint32_t kActivitiesNo	= 10000;//500000000;
	const uint32_t kUsersNo			= 30;//30000000;
	char kRequest[]					= "OP*Y)(*YHJBOUIyr79r6fiukv3ou4yg89s&T^(AS*&DGILASughjo987t2439ygLIYsg&UIA%^EDTR920upIDHSBKITF897tygd";
}

uint32_t requests	= 0;
boost::mutex		mutex;
uint64_t			current_time;

bool Update()
{
	boost::lock_guard<boost::mutex> lock(mutex);

	if(requests++ < Const::kActivitiesNo)
		return true;

	return false;
}

void TestMethod(std::shared_ptr<History::IProvider> provider, uint32_t no)
{
	while(Update())
	{
		auto user = str(boost::format("user%d") % (rand() % Const::kUsersNo));
		provider->AddUserActivity(user, current_time, Const::kRequest, sizeof(Const::kRequest));
	}
	//std::cout << "TestMethod: " << no << std::endl;
}

void Test2(std::shared_ptr<History::IProvider> provider)
{
	std::cout << "Run Test2" << std::endl;

	current_time = time(NULL);

	Update();
	auto user = str(boost::format("user%d") % (rand() % Const::kUsersNo));
	provider->AddUserActivity(user, current_time, Const::kRequest, sizeof(Const::kRequest));

	srand(current_time);

	std::list<boost::thread> threads;
	for(uint32_t i = 0; i < Const::kThreadNo; ++i)
	{
		threads.push_back(boost::thread([provider, i]()
		{
			TestMethod(provider, i);
		}));
	}

	while(!threads.empty())
	{
		threads.begin()->join();
		threads.erase(threads.begin());
	}

	auto elapsed_time = time(NULL) - current_time;

	std::cout << "Elapsed time: " << elapsed_time << " seconds" << std::endl;

	uint32_t total = 0;

	provider->ForActiveUser(current_time, [&total](const std::string& user, uint32_t number)
	{
		total += number;
		//std::cout << "WW " << user << " " << number << std::endl;
		return true;
	});

	std::cout << "Total activities: " << total << std::endl;
}