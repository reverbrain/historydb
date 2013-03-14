#include <iprovider.h>
#include <iostream>
#include <time.h>

char kUMM[] = "User made money ";
char kUCM[] = "User check mail ";
char kUCR[]	= "User clear recycle ";

char kUser1[] = "BlaUser1";
char kUser2[] = "BlaUser2";

int main(int argc, char* argv[])
{
	auto provider = History::CreateProvider();
	
	if(provider.get() == NULL)
	{
		std::cout << "Error! Provider hasn't been created!\n";
		return -1;
	}

	provider->Connect("localhost", 1025);
	std::vector<int> groups;
	groups.push_back(2);

	provider->SetSessionParameters(groups, 1);

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

	return 0;
}