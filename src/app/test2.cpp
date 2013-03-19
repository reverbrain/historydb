#include "test2.h"
#include <iprovider.h>
#include <iostream>
#include <boost/thread.hpp>
#include <boost/format.hpp>

namespace consts {
	const uint32_t THREADS_NO		= 10;
	const uint32_t ACTIVITIES_NO	= 100;//500000000;
	const uint32_t USERS_NO			= 30;//30000000;
	char REQUEST[]					= "OP*Y)(*YHJBOUIyr79r6fiukv3ou4yg89s&T^(AS*&DGILASughjo987t2439ygLIYsg&UIA%^EDTR920upIDHSBKITF897tygd";
} /* namespace consts */

uint32_t requests	= 0;
boost::mutex		mutex;
uint64_t			current_time;

bool update()
{
	boost::lock_guard<boost::mutex> lock(mutex);

	if(requests++ < consts::ACTIVITIES_NO)
		return true;

	return false;
}

void test_method(std::shared_ptr<history::iprovider> provider, uint32_t no)
{
	while(update()) {
		auto user = str(boost::format("user%d") % (rand() % consts::USERS_NO));
		provider->add_user_activity(user, current_time, consts::REQUEST, sizeof(consts::REQUEST));
	}
	//std::cout << "test_method: " << no << std::endl;
}

void test2(std::shared_ptr<history::iprovider> provider)
{
	std::cout << "Run test2" << std::endl;

	current_time = time(NULL);

	update();
	auto user = str(boost::format("user%d") % (rand() % consts::USERS_NO));
	provider->add_user_activity(user, current_time, consts::REQUEST, sizeof(consts::REQUEST));

	srand(current_time);

	std::list<boost::thread> threads;
	for(uint32_t i = 0; i < consts::THREADS_NO; ++i) {
		threads.push_back(boost::thread([provider, i]() {
			test_method(provider, i);
		}));
	}

	while(!threads.empty()) {
		threads.begin()->join();
		threads.erase(threads.begin());
	}

	auto elapsed_time = time(NULL) - current_time;

	std::cout << "Elapsed time: " << elapsed_time << " seconds" << std::endl;

	uint32_t total = 0;

	provider->for_active_user(current_time, [&total](const std::string& user, uint32_t number) {
		total += number;
		//std::cout << "WW " << user << " " << number << std::endl;
		return true;
	});

	std::cout << "Total activities: " << total << std::endl;
}