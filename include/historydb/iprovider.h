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
			min_writes - for each write attempt some group or groups could fail write. min_writes - minimum numbers of groups which shouldn't fail write.
		*/
		virtual void set_session_parameters(const std::vector<int>& groups, uint32_t min_writes) = 0;

		/* Adds data to user log file and increments user activity statistics.
			user - name of user
			time - current time, or time when data was created.
			data - pointer to the data for user log
			size - size of the data for user log
			key - custom key for activity statistics. It used for identification activity statistics file in elliptics. If key is std::string() generated id will be used instead of custom key.
		*/
		virtual void add_user_activity(const std::string& user, uint64_t time, void* data, uint32_t size, const std::string& key = std::string()) = 0;

		/* Async adds data to user log file and increments user activity statistics.
			user - name of user
			time - current time, or time when data was created.
			data - pointer to the data for user log
			size - size of the data for user log
			func - callback which will be called after all
			key - custom key for activity statistics. It used for identification activity statistics file in elliptics. If key is std::string() generated id will be used instead of custom key.
		*/
		virtual void add_user_activity(const std::string& user, uint64_t time, void* data, uint32_t size, std::function<void(bool log_writed, bool statistics_updated)> func, const std::string& key = std::string()) = 0;

		/*	Repartitions activity's statistics.
			key/old_key - current key of activity files
			new_key - new key for activity files
			time - timstamp of activity
			chunks - numbers of chunks in new decomposition
		*/
		virtual void repartition_activity(const std::string& key, uint32_t chunks) = 0;
		virtual void repartition_activity(const std::string& old_key, const std::string& new_key, uint32_t chunks) = 0;
		virtual void repartition_activity(uint64_t time, uint32_t chunks) = 0;
		virtual void repartition_activity(uint64_t time, const std::string& new_key, uint32_t chunks) = 0;

		/* Gets user's logs for specified period
			user - name of user
			begin_time - begin of the time period
			end_time - end of the time period
			returns list of vectors where vector is a logs of one day
		*/
		virtual std::list<std::vector<char>> get_user_logs(const std::string& user, uint64_t begin_time, uint64_t end_time) = 0;

		/* Gets active users with activity statistics for specified day
			time - day of activity statistics
			returns map of active user with activity statistics
		*/
		virtual std::map<std::string, uint32_t> get_active_users(uint64_t time) = 0;

		/* Gets active users with activity statistics for specified key
			key - custom key of activity statistics
			return map of active user with activity statistics
		*/
		virtual std::map<std::string, uint32_t> get_active_users(const std::string& key) = 0;

		/* Iterates by user's logs and calls func for each log record
			user - name of user
			begin_time - begin of the time period
			end_time -end of the time period
			func - callback functor
		*/
		virtual void for_user_logs(const std::string& user, uint64_t begin_time, uint64_t end_time, std::function<bool(const std::string& user, uint64_t time, void* data, uint32_t size)> func) = 0;

		/* Iterates by activity statistics and calls func for each user
			time - day of activity statistics
		*/
		virtual void for_active_users(uint64_t time, std::function<bool(const std::string& user, uint32_t number)> func) = 0;

		/* Iterates by activity statistics and calls func for each user
			key - custom key of activity statistics
		*/
		virtual void for_active_users(const std::string& key, std::function<bool(const std::string& user, uint32_t number)> func) = 0;
	};

	/* Creates inctance of history::iprovider
		server_addr - elliptics address
		server_port - elliptics port
		family - inet family
	*/
	extern std::shared_ptr<iprovider> create_provider(const char* server_addr, const int server_port, const int family, const char* log_file, const int log_level);
	extern int get_log_level(const char* log_level);
} /* namespace history */

#endif //HISTORY_IPROVIDER_H
