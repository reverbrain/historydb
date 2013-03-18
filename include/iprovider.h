#ifndef HISTORY_IPROVIDER_H
#define HISTORY_IPROVIDER_H

#include <memory>
#include <stdint.h>
#include <string>
#include <vector>
#include <map>
#include <list>

namespace History
{
	class IProvider
	{
	public:
		virtual ~IProvider() {}

		/* Connects to elliptics
		*/
		virtual void Connect(const char* server_addr, const int server_port, const int family) = 0;

		/* Disconnects from elliptics
		*/
		virtual void Disconnect() = 0;

		/* Sets parameters for elliptic's sessions.
			groups - groups with which History DB will works
			min_writes - for each write attempt some group or groups could fail write. min_writes - minimum numbers of groups which not failed write.
		*/
		virtual void SetSessionParameters(const std::vector<int>& groups, uint32_t min_writes) = 0;

		/* Adds data to user-key file and increments user activity statistics.
			user - name of user in which file data should be wrote
			time - current time, or time when data was created.
			data - pointer to the data
			size - size of the data 
			key - custom key for activity statistics. It used for identification activity statistics file in elliptics. If key is std::string() generated id will be used instead of custom key.
		*/
		virtual void AddUserActivity(const std::string& user, uint64_t time, void* data, uint32_t size, const std::string& key = std::string()) const = 0;

		/*	Repartitions activity's statistics.
			key/old_key - current key of activity files
			new_key - new key for activity files
			time - timstamp of activity
			parts - numbers of parts in new decomposition
		*/
		virtual void RepartitionActivity(const std::string& key, uint32_t parts) const = 0;
		virtual void RepartitionActivity(const std::string& old_key, const std::string& new_key, uint32_t parts) const = 0;
		virtual void RepartitionActivity(uint64_t time, uint32_t parts) const = 0;
		virtual void RepartitionActivity(uint64_t time, const std::string& new_key, uint32_t parts) const = 0;

		/* Gets user's logs for specified period
		*/
		virtual std::list<std::vector<char>> GetUserLogs(const std::string& user, uint64_t begin_time, uint64_t end_time) const = 0;
		/* Gets active users with activity statistics for specified day
		*/
		virtual std::map<std::string, uint32_t> GetActiveUser(uint64_t time) const = 0;
		/* Gets active users with activity statistics for specified key
		*/
		virtual std::map<std::string, uint32_t> GetActiveUser(const std::string& key) const = 0;


		/* Iterates by user's logs and calls func for each log record
		*/
		virtual void ForUserLogs(const std::string& user, uint64_t begin_time, uint64_t end_time, std::function<bool(const std::string& user, uint64_t time, void* data, uint32_t size)> func) const = 0;

		/* Iterates by activity statistics and calls func for each user
		*/
		virtual void ForActiveUser(uint64_t time, std::function<bool(const std::string& user, uint32_t number)> func) const = 0;
		virtual void ForActiveUser(const std::string& key, std::function<bool(const std::string& user, uint32_t number)> func) const = 0;
	};

	extern std::shared_ptr<IProvider> CreateProvider();
}

#endif //HISTORY_IPROVIDER_H
