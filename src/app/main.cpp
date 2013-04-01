#include <iostream>
#include <time.h>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include "historydb/iprovider.h"

#include "test2.h"

char UMM[] = "User made money\n";
char UCM[] = "User check mail\n";
char UCR[]	= "User clear recycle\n";

char USER1[] = "BlaUser1";
char USER2[] = "BlaUser2";

void print_usage(char* s) {
	std::cout << "Usage: " << s << "\n"
	<< " -r addr:port:family    - adds a route to the given node\n"
	<< " -g groups              - groups id to connect which are separated by ','\n"
	<< " -t tests               - numbers of tests which should be runned separated by ','\n"
	;
}

void test1(std::shared_ptr<history::iprovider> provider) {
	std::cout << "Run test1" << std::endl;
	auto tm = 0;

	provider->add_user_activity(USER1, tm++, UMM, sizeof(UMM));
	provider->add_user_activity(USER1, tm++, UCM, sizeof(UCM));
	provider->add_user_activity(USER1, tm++, UMM, sizeof(UMM));
	provider->add_user_activity(USER1, tm++, UCR, sizeof(UCR));
	provider->add_user_activity(USER1, tm++, UMM, sizeof(UMM));
	provider->add_user_activity(USER1, tm++, UMM, sizeof(UMM));
	provider->add_user_activity(USER1, tm++, UCR, sizeof(UCR));
	provider->add_user_activity(USER1, tm++, UMM, sizeof(UMM));
	provider->add_user_activity(USER1, tm++, UMM, sizeof(UMM));

	provider->add_user_activity(USER2, tm++, UCR, sizeof(UCR));
	provider->add_user_activity(USER2, tm++, UCR, sizeof(UCR));
	provider->add_user_activity(USER2, tm++, UCM, sizeof(UCM));
	provider->add_user_activity(USER2, tm++, UMM, sizeof(UMM));
	provider->add_user_activity(USER2, tm++, UCR, sizeof(UCR));
	provider->add_user_activity(USER2, tm++, UMM, sizeof(UMM));
	provider->add_user_activity(USER2, tm++, UCM, sizeof(UCM));

	provider->for_user_logs(USER1, 3, tm, [](const std::string& user, uint64_t time, void* data, uint32_t size) {
		std::cout << "LOG1 LAMBDA: user: " << user << " " << std::string((char*)data, size) << " " << time << std::endl;
		return true;
	});

	provider->for_user_logs(USER2, 0, 10, [](const std::string& user, uint64_t time, void* data, uint32_t size) {
		std::cout << "LOG2 LAMBDA: user: " << user << " " << std::string((char*)data, size) << " " << time << std::endl;
		return true;
	});

	provider->for_active_users(tm, [](const std::string& user, uint32_t number) {
		std::cout << "ACT1 LAMBDA: " << user << " " << number << std::endl;
		return true;
	});

	provider->repartition_activity(tm, 10);

	provider->for_active_users(tm, [](const std::string& user, uint32_t number) {
		std::cout << "ACT2 LAMBDA: " << user << " " << number << std::endl;
		return true;
	});
}

void test3(std::shared_ptr<history::iprovider> provider) {
	auto tm = 1;
	provider->add_user_activity(USER1, tm++, UMM, sizeof(UMM), [](bool log_writed, bool statistics_updated) {
		std::cout	<< "TEST3: add_user_activity: log: " << (log_writed ? "writed" : "not writed")
					<< " statistics: " << (statistics_updated ? "updated" : "not updated")
					<< std::endl;
	});

	uint32_t ind = 0;

	while(ind < 2) {

		provider->for_user_logs(USER1, 0, tm, [&ind](const std::string& user, uint64_t time, void* data, uint32_t size) {
			std::cout << "TEST3: LOG1 LAMBDA: user: " << user << " " << std::string((char*)data, size) << " " << time << std::endl;
			++ind;
			return true;
		});

		provider->for_active_users(tm, [&ind](const std::string& user, uint32_t number) {
			std::cout << "TEST3: ACT1 LAMBDA: " << user << " " << number << std::endl;
			++ind;
			return true;
		});
	}
}

void run_test(int test_no, std::shared_ptr<history::iprovider> provider) {
	switch(test_no) {
		case 1:	test1(provider); break;
		case 2:	test2(provider); break;
		case 3: test3(provider); break;
	}
}

int main(int argc, char* argv[]) {
	int ch, err = 0;
	std::string remote_addr;
	int port = -1;
	int family = -1;
	std::vector<int> groups;
	std::vector<int> tests;

	try {
		while((ch = getopt(argc, argv, "r:g:t:")) != -1) {
			switch(ch) {
				case 'r': {
					std::vector<std::string> strs;
					boost::split(strs, optarg, boost::is_any_of(":"));

					if(strs.size() != 3)
						throw std::invalid_argument("-r");

					remote_addr = strs[0];
					port = boost::lexical_cast<int>(strs[1]);
					family = boost::lexical_cast<int>(strs[2]);
				}
				break;
				case 'g': {
					std::vector<std::string> strs;
					boost::split(strs, optarg, boost::is_any_of(","));

					groups.reserve(strs.size());
					for(auto it = strs.begin(), itEnd = strs.end(); it != itEnd; ++it) {
						groups.push_back(boost::lexical_cast<int>(*it));
					}
				}
				break;
				case 't': {
					std::vector<std::string> strs;
					boost::split(strs, optarg, boost::is_any_of(","));

					tests.reserve(strs.size());
					for(auto it = strs.begin(), itEnd = strs.end(); it != itEnd; ++it) {
						tests.push_back(boost::lexical_cast<int>(*it));
					}
				}
				break;
			}
		}
	}
	catch(...) {
		err = -1;
	}

	if(err) {
		print_usage(argv[0]);
		return err;
	}

	std::shared_ptr<history::iprovider> provider(history::create_provider(remote_addr.c_str(), port, family));
	
	if(provider.get() == NULL) {
		std::cout << "Error! Provider hasn't been created!\n";
		return -1;
	}

	provider->set_session_parameters(groups, 1);

	for(auto it = tests.begin(), itEnd = tests.end(); it != itEnd; ++it) {
		run_test(*it, provider);
	}

	return 0;
}
