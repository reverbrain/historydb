#ifndef HISTORY_IPROVIDER_H
#define HISTORY_IPROVIDER_H

#include <memory>
#include <stdint.h>
#include <string>
#include <vector>
#include <map>
#include <list>

namespace history {

	class iprovider
	{
	public:
		virtual ~iprovider() {}

		/* Sets parameters for elliptic's sessions.
			groups - groups with which History DB will works
			min_writes - for each write attempt some group or groups could fail write. min_writes - minimum numbers of groups which not failed write.
		*/
		virtual void set_session_parameters(const std::vector<int>& groups, uint32_t min_writes) = 0;

		/* Adds data to user-key file and increments user activity statistics.
			user - name of user in which file data should be wrote
			time - current time, or time when data was created.
			data - pointer to the data
			size - size of the data 
			key - custom key for activity statistics. It used for identification activity statistics file in elliptics. If key is std::string() generated id will be used instead of custom key.
		*/
		virtual void add_user_activity(const std::string& user, uint64_t time, void* data, uint32_t size, const std::string& key = std::string()) = 0;

		/*	Repartitions activity's statistics.
			key/old_key - current key of activity files
			new_key - new key for activity files
			time - timstamp of activity
			parts - numbers of parts in new decomposition
		*/
		virtual void repartition_activity(const std::string& key, uint32_t parts) = 0;
		virtual void repartition_activity(const std::string& old_key, const std::string& new_key, uint32_t parts) = 0;
		virtual void repartition_activity(uint64_t time, uint32_t parts) = 0;
		virtual void repartition_activity(uint64_t time, const std::string& new_key, uint32_t parts) = 0;

		/* Gets user's logs for specified period
		*/
		virtual std::list<std::vector<char>> get_user_logs(const std::string& user, uint64_t begin_time, uint64_t end_time) = 0;
		/* Gets active users with activity statistics for specified day
		*/
		virtual std::map<std::string, uint32_t> get_active_user(uint64_t time) = 0;
		/* Gets active users with activity statistics for specified key
		*/
		virtual std::map<std::string, uint32_t> get_active_user(const std::string& key) = 0;


		/* Iterates by user's logs and calls func for each log record
		*/
		virtual void for_user_logs(const std::string& user, uint64_t begin_time, uint64_t end_time, std::function<bool(const std::string& user, uint64_t time, void* data, uint32_t size)> func) = 0;

		/* Iterates by activity statistics and calls func for each user
		*/
		virtual void for_active_user(uint64_t time, std::function<bool(const std::string& user, uint32_t number)> func) = 0;
		virtual void for_active_user(const std::string& key, std::function<bool(const std::string& user, uint32_t number)> func) = 0;
	};

	extern std::shared_ptr<iprovider> create_provider(const char* server_addr, const int server_port, const int family);
} /* namespace history */

#endif //HISTORY_IPROVIDER_H
