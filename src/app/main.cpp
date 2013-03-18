#include <iprovider.h>
#include <iostream>
#include <time.h>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include "test2.h"

char kUMM[] = "User made money\n";
char kUCM[] = "User check mail\n";
char kUCR[]	= "User clear recycle\n";

char kUser1[] = "BlaUser1";
char kUser2[] = "BlaUser2";

void print_usage(char* s)
{
	std::cout << "Usage: " << s << "\n"
	<< " -r addr:port:family    - adds a route to the given node\n"
	<< " -g groups              - groups id to connect which are separated by ','\n"
	<< " -t tests               - numbers of tests which should be runned separated by ','\n"
	;
}

void Test1(std::shared_ptr<History::IProvider> provider)
{
	std::cout << "Run Test1" << std::endl;
	auto tm = 0;

	provider->AddUserActivity(kUser1, tm++, kUMM, sizeof(kUMM));
	provider->AddUserActivity(kUser1, tm++, kUCM, sizeof(kUCM));
	provider->AddUserActivity(kUser1, tm++, kUMM, sizeof(kUMM));
	provider->AddUserActivity(kUser1, tm++, kUCR, sizeof(kUCR));
	provider->AddUserActivity(kUser1, tm++, kUMM, sizeof(kUMM));
	provider->AddUserActivity(kUser1, tm++, kUMM, sizeof(kUMM));
	provider->AddUserActivity(kUser1, tm++, kUCR, sizeof(kUCR));
	provider->AddUserActivity(kUser1, tm++, kUMM, sizeof(kUMM));
	provider->AddUserActivity(kUser1, tm++, kUMM, sizeof(kUMM));

	provider->AddUserActivity(kUser2, tm++, kUCR, sizeof(kUCR));
	provider->AddUserActivity(kUser2, tm++, kUCR, sizeof(kUCR));
	provider->AddUserActivity(kUser2, tm++, kUCM, sizeof(kUCM));
	provider->AddUserActivity(kUser2, tm++, kUMM, sizeof(kUMM));
	provider->AddUserActivity(kUser2, tm++, kUCR, sizeof(kUCR));
	provider->AddUserActivity(kUser2, tm++, kUMM, sizeof(kUMM));
	provider->AddUserActivity(kUser2, tm++, kUCM, sizeof(kUCM));

	provider->ForUserLogs(kUser1, 3, tm, [](const std::string& user, uint64_t time, void* data, uint32_t size)
	{
		std::cout << "LOG1 LAMBDA: " << std::string((char*)data, size) << " " << time << std::endl;
		return true;
	});

	provider->ForUserLogs(kUser2, 0, 10, [](const std::string& user, uint64_t time, void* data, uint32_t size)
	{
		std::cout << "LOG2 LAMBDA: " << std::string((char*)data, size) << " " << time << std::endl;
		return true;
	});

	provider->ForActiveUser(tm, [](const std::string& user, uint32_t number)
	{
		std::cout << "ACT1 LAMBDA: " << user << " " << number << std::endl;
		return true;
	});

	provider->RepartitionActivity(tm, 10);

	provider->ForActiveUser(tm, [](const std::string& user, uint32_t number)
	{
		std::cout << "ACT2 LAMBDA: " << user << " " << number << std::endl;
		return true;
	});
}

void RunTest(int test_no, std::shared_ptr<History::IProvider> provider)
{
	switch(test_no)
	{
		case 1:	Test1(provider); break;
		case 2:	Test2(provider); break;
	}
}

int main(int argc, char* argv[])
{
	int ch, err = 0;
	std::string remote_addr;
	int port = -1;
	int family = -1;
	std::vector<int> groups;
	std::vector<int> tests;

	try
	{
		while((ch = getopt(argc, argv, "r:g:t:")) != -1)
		{
			switch(ch)
			{
				case 'r':
				{
					std::vector<std::string> strs;
					boost::split(strs, optarg, boost::is_any_of(":"));

					if(strs.size() != 3)
						throw std::invalid_argument("-r");

					remote_addr = strs[0];
					port = boost::lexical_cast<int>(strs[1]);
					family = boost::lexical_cast<int>(strs[2]);
				}
				break;
				case 'g':
				{
					std::vector<std::string> strs;
					boost::split(strs, optarg, boost::is_any_of(","));

					groups.reserve(strs.size());
					for(auto it = strs.begin(), itEnd = strs.end(); it != itEnd; ++it)
					{
						groups.push_back(boost::lexical_cast<int>(*it));
					}
				}
				break;
				case 't':
				{
					std::vector<std::string> strs;
					boost::split(strs, optarg, boost::is_any_of(","));

					tests.reserve(strs.size());
					for(auto it = strs.begin(), itEnd = strs.end(); it != itEnd; ++it)
					{
						tests.push_back(boost::lexical_cast<int>(*it));
					}
				}
				break;
			}
		}
	}
	catch(...)
	{
		err = -1;
	}

	if(err)
	{
		print_usage(argv[0]);
		return err;
	}

	std::shared_ptr<History::IProvider> provider(History::CreateProvider());
	
	if(provider.get() == NULL)
	{
		std::cout << "Error! Provider hasn't been created!\n";
		return -1;
	}

	provider->Connect(remote_addr.c_str(), port, family);

	provider->SetSessionParameters(groups, 1);

	for(auto it = tests.begin(), itEnd = tests.end(); it != itEnd; ++it)
	{
		RunTest(*it, provider);
	}

	return 0;
}