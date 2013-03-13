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
	
	provider->AddActivity("BlaUser1", History::kWrite, tm++);
	provider->AddActivity("BlaUser1", History::kWrite, tm++);
	provider->AddActivity("BlaUser1", History::kWrite, tm++);

	provider->GetActivities("BlaUser1", time(NULL) - 50 * 60 * 60, time(NULL) + 50 * 60 * 60);

	/*provider->ForEachActiveUser();

	provider->ForEachUserActivities();*/

	return 0;
}