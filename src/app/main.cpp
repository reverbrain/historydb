/*
 * Copyright 2013+ Kirill Smorodinnikov <shaitkir@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#include <iostream>
#include <time.h>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include "historydb/provider.h"

#include "test2.h"

char UMM[]		= "User made money\n";
char UCM[]		= "User check mail\n";
char UCR[]		= "User clear recycle\n";

char USER1[]	= "BlaUser1";
char USER2[]	= "BlaUser2";
char LOG_FILE[]	= "/tmp/hdb_log"; // path to log file
int LOG_LEVEL	= 0; // log level

void print_usage(char* s)
{
	std::cout << "Usage: " << s << "\n"
	<< " -r addr:port:family    - adds a route to the given node\n"
	<< " -g groups              - groups id to connect which are separated by ','\n"
	<< " -t tests               - numbers of tests which should be runned separated by ','\n"
	;
}

bool test1_for1(const std::vector<char>& data)
{
	std::cout << "LOG1 LAMBDA: " << std::string((char*)&data.front(), data.size()) << " " << time << std::endl;
	return true;
}

bool test1_for2(const std::vector<char>& data)
{
	std::cout << "LOG2 LAMBDA: " << std::string((char*)&data.front(), data.size()) << " " << time << std::endl;
	return true;
}

bool test1_for3(const std::set<std::string>& user)
{
	std::cout << "ACT1 LAMBDA: " << user.size() << " " << std::endl;
	return true;
}

bool test1_for4(const std::set<std::string>& user)
{
	std::cout << "ACT2 LAMBDA: " << user.size() << " " << std::endl;
	return true;
}

std::vector<char> make_vector(const char* data, uint32_t size)
{
	return std::vector<char>(data, data + size);
}

void test1(std::shared_ptr<history::provider> provider)
{
	std::cout << "Run test1" << std::endl;
	auto tm = 0;

	provider->add_log(USER1, tm++, make_vector(UMM, sizeof(UMM)));
	provider->add_log(USER1, tm++, make_vector(UCM, sizeof(UCM)));
	provider->add_log(USER1, tm++, make_vector(UMM, sizeof(UMM)));
	provider->add_log(USER1, tm++, make_vector(UCR, sizeof(UCR)));
	provider->add_log(USER1, tm++, make_vector(UMM, sizeof(UMM)));
	provider->add_log(USER1, tm++, make_vector(UMM, sizeof(UMM)));
	provider->add_log(USER1, tm++, make_vector(UCR, sizeof(UCR)));
	provider->add_log(USER1, tm++, make_vector(UMM, sizeof(UMM)));
	provider->add_log(USER1, tm++, make_vector(UMM, sizeof(UMM)));

	provider->add_log(USER2, tm++, make_vector(UCR, sizeof(UCR)));
	provider->add_log(USER2, tm++, make_vector(UCR, sizeof(UCR)));
	provider->add_log(USER2, tm++, make_vector(UCM, sizeof(UCM)));
	provider->add_log(USER2, tm++, make_vector(UMM, sizeof(UMM)));
	provider->add_log(USER2, tm++, make_vector(UCR, sizeof(UCR)));
	provider->add_log(USER2, tm++, make_vector(UMM, sizeof(UMM)));
	provider->add_log(USER2, tm++, make_vector(UCM, sizeof(UCM)));

	provider->for_user_logs(USER1, 3, tm, test1_for1);

	provider->for_user_logs(USER2, 0, 10, test1_for2);

	provider->for_active_users(tm, tm + 1, test1_for3);

	//provider->repartition_activity(tm, 10);

	provider->for_active_users(tm, tm + 1, test1_for4);
}

void test3_add(bool log_writed)
{
	std::cout	<< "TEST3: add_log: " << (log_writed ? "writed" : "not writed")
				<< std::endl;
}

bool test3_for1(uint32_t& ind, const std::vector<char>& data)
{
	std::cout << "TEST3: LOG1 LAMBDA: " << std::string((char*)&data.front(), data.size()) << std::endl;
	++ind;
	return true;
}

bool test3_for2(uint32_t& ind, const std::set<std::string>& user)
{
	std::cout << "TEST3: ACT1 LAMBDA: " << user.size() << " " << std::endl;
	++ind;
	return true;
}

void test3(std::shared_ptr<history::provider> provider)
{
	auto tm = 1;
	provider->add_log(USER1, tm++, make_vector(UMM, sizeof(UMM)), test3_add);

	uint32_t ind = 0;

	while(ind < 2) {

		provider->for_user_logs(USER1, 0, tm, boost::bind(&test3_for1, boost::ref(ind), _1));

		provider->for_active_users(tm, tm + 1, boost::bind(&test3_for2, boost::ref(ind), _1));
	}
}

void test4_callback(bool added)
{
	std::cout << "Test4 res: " << (added ? "added" : "failed") << std::endl;
}

void test4(std::shared_ptr<history::provider> provider)
{
	std::cout << "TEST4:" << std::endl;

	provider->add_log("PU", 0, make_vector("ASDSADA", 8), &test4_callback);

	boost::this_thread::sleep(boost::posix_time::milliseconds(100));
}

void run_test(int test_no, std::shared_ptr<history::provider> provider)
{
	switch(test_no) {
		case 1:	test1(provider); break;
		case 2:	test2(provider); break;
		case 3: test3(provider); break;
		case 4: test4(provider); break;
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

	try {
		while((ch = getopt(argc, argv, "r:g:t:")) != -1) {
			switch(ch) {
				case 'r': {
					std::vector<std::string> strs;
					boost::split(strs, optarg, boost::is_any_of(":"));

					if (strs.size() != 3)
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
					for (auto it = strs.begin(), itEnd = strs.end(); it != itEnd; ++it) {
						groups.push_back(boost::lexical_cast<int>(*it));
					}
				}
				break;
				case 't': {
					std::vector<std::string> strs;
					boost::split(strs, optarg, boost::is_any_of(","));

					tests.reserve(strs.size());
					for (auto it = strs.begin(), itEnd = strs.end(); it != itEnd; ++it) {
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

	if (err) {
		print_usage(argv[0]);
		return err;
	}

	std::vector<history::server_info> servers;
	history::server_info info = {remote_addr.c_str(), port, family};
	servers.emplace_back(info);

	auto provider = std::make_shared<history::provider>(servers, std::vector<int>(), 0, LOG_FILE, LOG_LEVEL);

	if (provider.get() == NULL) {
		std::cout << "Error! Provider hasn't been created!\n";
		return -1;
	}

	provider->set_session_parameters(groups, 1);

	for (auto it = tests.begin(), itEnd = tests.end(); it != itEnd; ++it) {
		run_test(*it, provider);
	}

	return 0;
}
