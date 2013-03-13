#ifndef HISTORY_IPROVIDER_H
#define HISTORY_IPROVIDER_H

#include <memory>
#include <stdint.h>
#include <list>
#include <string>
#include <map>

namespace History
{
	enum Activity
	{
		kRead	= 0
	,	kWrite
	,	kCheckEmail
	};

	struct ActivityData
	{
		std::string user;
		Activity activity;
		uint32_t time;
	};

	class IProvider
	{
	public:
		virtual ~IProvider() {}

		/* Connects to elliptics
		*/
		virtual void Connect() = 0;

		/* Disconnects from elliptics
		*/
		virtual void Disconnect() = 0;

		/* Add activity record to certain user
		*/
		virtual void AddActivity(const std::string& user, Activity activity, uint32_t time) const = 0;

		/* Gets activities statistics for certain user for specified time period
		*/
		virtual std::list<ActivityData> GetActivities(const std::string& user, uint32_t begin_time, uint32_t end_time) const = 0;

		/* Gets activities for all users for specified period
		*/
		virtual std::map<std::string, uint32_t> GetActivities(Activity activity, uint32_t begin_time, uint32_t end_time) const = 0;

		/* Iterates by active users for specified activity and time period
		*/
		virtual void ForEachUser(Activity activity, uint32_t begin_time, uint32_t end_time, std::function<bool(const std::string&, uint32_t)> func) const = 0;

		/* Iterates by certain user activities for specified time period
		*/
		virtual void ForEachActivities(const std::string& user, uint32_t begin_time, uint32_t end_time, std::function<bool(ActivityData)> func) const = 0;
	};

	extern std::shared_ptr<IProvider> CreateProvider();
}

#endif //HISTORY_IPROVIDER_H
