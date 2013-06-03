#ifndef HISTORY_IPROVIDER_H
#define HISTORY_IPROVIDER_H

#include <memory>
#include <stdint.h>
#include <string>
#include <vector>
#include <list>

namespace history {

struct server_info
{
	std::string addr;
	int port;
	int family;
};

class provider
{
public:
	provider(const std::vector<server_info>& servers, const std::vector<int>& groups, uint32_t min_writes, const std::string& log_file, const int log_level);
	provider(const std::vector<std::string>& servers, const std::vector<int>& groups, uint32_t min_writes, const std::string& log_file, const int log_level);

	/* Sets parameters for elliptic's sessions.
		groups - groups with which History DB will works
		min_writes - for each write attempt some group or groups could fail write. min_writes - minimum numbers of groups which shouldn't fail write.
	*/
	void set_session_parameters(const std::vector<int>& groups, uint32_t min_writes);

	/* Adds data to user logs
		user - name of user
		time - timestamp of the log record
		data - user log data
	*/
	void add_log(const std::string& user, uint64_t time, const std::vector<char>& data);

	/* Adds data to user logs
		user - name of user
		subkey - custom key for user logs
		data - user log data
	*/
	void add_log(const std::string& user, const std::string& subkey, const std::vector<char>& data);

	/* Async adds data to user logs
		user - name of user
		time - timestamp of the log record
		data - user log data
		callback - complete callback
	*/
	void add_log(const std::string& user, uint64_t time, const std::vector<char>& data, std::function<void(bool added)> callback);

	/* Async adds data to user logs
		user - name of user
		subkey - custom key for user logs
		data - user log data
		callback - complete callback
	*/
	void add_log(const std::string& user, const std::string& subkey, const std::vector<char>& data, std::function<void(bool added)> callback);

	/* Adds user to activity statistics
		user - name of user
		time - timestamp of activity statistics
		key - custom key of activity statistics
	*/
	void add_activity(const std::string& user, uint64_t time);

	/* Adds user to activity statistics
		user - name of user
		subkey - custom key for activity statistics
		key - custom key of activity statistics
	*/
	void add_activity(const std::string& user, const std::string& subkey);


	/* Async adds user to activity statistics
		user - name of user
		time - timestamp of activity statistics
		callback - complete callback
	*/
	void add_activity(const std::string& user, uint64_t time, std::function<void(bool added)> callback);


	/* Async adds user to activity statistics
		user - name of user
		subkey - custom key for activity statistics
		key - custom key of activity statistics
		callback - complete callback
	*/
	void add_activity(const std::string& user, const std::string& subkey, std::function<void(bool added)> callback);

	/* Adds data to user logs and user to activity statistics
		user - name of user
		time - timestamp of log and activity statistics
		data - user log data
	*/
	//void add_log_and_activity(const std::string& user, uint64_t time, const std::vector<char>& data);

	/* Adds data to user logs and user to activity statistics
		user - name of user
		key - custom key of logs and activity statistics
		data - user log data
	*/
	//void add_log_and_activity(const std::string& user, const std::string& subkey, const std::vector<char>& data);

	/* Async adds data to user logs and user to activity statistics
		user - name of user
		time - timestamp of log and activity statistics
		data - user log data
		callback - complete callback
	*/
	//void add_log_and_activity(const std::string& user, uint64_t time, const std::vector<char>& data, std::function<void(bool log_added, bool activity_added)> callback);

	/* Async adds data to user logs and user to activity statistics
		user - name of user
		key - custom key of logs and activity statistics
		data - user log data
		callback - complete callback
	*/
	//void add_log_and_activity(const std::string& user, const std::string& subkey, const std::vector<char>& data, std::function<void(bool log_added, bool activity_added)> callback);

	/* Gets user's logs for specified period
		user - name of user
		begin_time - begin of the time period
		end_time - end of the time period
		returns list of vectors where vector is a logs of one day
	*/
	std::list<std::vector<char>> get_user_logs(const std::string& user, uint64_t begin_time, uint64_t end_time);

	/* Gets user's logs for subkeys
		user - name of user
		subkeys - custom keys of user logs
		returns list of vector where vector is a logs of one subkey
	*/
	std::list<std::vector<char>> get_user_logs(const std::string& user, const std::vector<std::string>& subkeys);

	/* Gets active users with activity statistics for specified day
		time - timestamp of the activity statistics day
		returns list of active users
	*/
	std::list<std::string> get_active_users(uint64_t time);

	/* Gets active users with activity statistics for specified subkey
		subkey - custom key of activity statistics
		return list of active users
	*/
	std::list<std::string> get_active_users(const std::string& subkey);

	/* Runs through users logs for specified time period and calls callback on each log file
		user - name of user
		begin_time - begin of the time period
		end_time - end of the time period
		callback - on log file callback
	*/
	void for_user_logs(const std::string& user, uint64_t begin_time, uint64_t end_time, std::function<bool(const std::vector<char>& data)> callback);

	/* Runs through users logs for specified subkeys and calls callback on each log file
		user - name of user
		subkeys - custom keys of user logs
		callback - on log file callback
	*/
	void for_user_logs(const std::string& user, const std::vector<std::string>& subkeys, std::function<bool(const std::vector<char>& data)> callback);

	/* Runs throgh activity statistics for specified time period and calls callback on each activity statistics
		begin_time - begin of the time period
		end_time - end of the time period
		callback - on active users callback
	*/
	void for_active_users(uint64_t begin_time, uint64_t end_time, std::function<bool(const std::list<std::string>& active_users)> callback);

	/* Runs throgh activity statistics for specified subkeys and calls callback on each activity statistics
		subkeys - custom keys of activity statistics
		callback - on active users callback
	*/
	void for_active_users(const std::vector<std::string>& subkeys, std::function<bool(const std::list<std::string>& active_users)> callback);

private:
	provider(const provider&) = delete;
	provider& operator=(const provider&) = delete;

	class impl;
	std::shared_ptr<impl>	impl_;
};

extern int get_log_level(const std::string& log_level);

} /* namespace history */

#endif //HISTORY_IPROVIDER_H
