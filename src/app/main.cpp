#include <iprovider.h>
#include <iostream>
#include <time.h>

int main(int argc, char* argv[])
{
	auto provider = History::CreateProvider();
	
	if(provider.get() == NULL)
	{
		std::cout << "Error! Provider hasn't been created!\n";
		return -1;
	}

	provider->Connect();

	auto tm = time(NULL);

	provider->AddActivity("BlaUser1", History::kRead, tm++);
	provider->AddActivity("BlaUser1", History::kRead, tm++);
	provider->AddActivity("BlaUser1", History::kRead, tm - 40 * 60 * 60);
	provider->AddActivity("BlaUser1", History::kWrite, tm++);
	provider->AddActivity("BlaUser1", History::kWrite, tm++);

	tm -= 24 * 60 * 60;

	provider->AddActivity("BlaUser2", History::kRead, tm++);
	provider->AddActivity("BlaUser2", History::kWrite, tm++);
	provider->AddActivity("BlaUser2", History::kRead, tm++);
	provider->AddActivity("BlaUser2", History::kRead, tm - 10 * 60 * 60);
	provider->AddActivity("BlaUser2", History::kWrite, tm++);

	auto a1 = provider->GetActivities("BlaUser1", time(NULL) - 50 * 60 * 60, time(NULL) + 50 * 60 * 60);
	for(auto it = a1.begin(), itEnd = a1.end(); it != itEnd; ++it)
	{
		std::cout << it->user << " " << it->activity << " " << it->time << std::endl;
	}

	std::cout << std::endl;

	auto a2 = provider->GetActivities("BlaUser2", time(NULL) - 50 * 60 * 60, time(NULL) + 50 * 60 * 60);
	for(auto it = a2.begin(), itEnd = a2.end(); it != itEnd; ++it)
	{
		std::cout << it->user << " " << it->activity << " " << it->time << std::endl;
	}

	std::cout << std::endl;

	auto map = provider->GetActivities(History::kRead, time(NULL) - 50 * 60 * 60, time(NULL) + 50 * 60 * 60);

	for(auto it = map.begin(), itEnd = map.end(); it != itEnd; ++it)
	{
		std::cout << it->first << " " << it->second << std::endl;
	}

	provider->ForEachActivities("BlaUser1", time(NULL) - 50 * 60 * 60, time(NULL) + 50 * 60 * 60, [](History::ActivityData data)
	{
		std::cout << "ACTIV LAMBDA: " << data.user << " " << data.activity << " " << data.time << std::endl;
		return true;
	});

	provider->ForEachUser(History::kRead, time(NULL) - 50 * 60 * 60, time(NULL) + 50 * 60 * 60, [](const std::string& user, uint32_t count)
	{
		std::cout << "USER LAMBDA: " << user << " " << count << std::endl;
		return true;
	});

	return 0;
}