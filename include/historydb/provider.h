#ifndef HISTORY_PROVIDER_H
#define HISTORY_PROVIDER_H

#include <memory>
#include <stdint.h>
#include <string>
#include <vector>
#include <list>
#include <set>

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
	*/
	void add_activity(const std::string& user, uint64_t time);

	/* Adds user to activity statistics
		user - name of user
		subkey - custom key for activity statistics
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
		callback - complete callback
	*/
	void add_activity(const std::string& user, const std::string& subkey, std::function<void(bool added)> callback);

	/* Adds data to user logs and user to activity statistics
		user - name of user
		time - timestamp for log and for activity statistics
		data - user log data
	*/
	void add_log_with_activity(const std::string& user, uint64_t time, const std::vector<char>& data);

	/* Adds data to user logs and user to activity statistics
		user - name of user
		subkey - custom key for logs and for activity statistics
		data - user log data
	*/
	void add_log_with_activity(const std::string& user, const std::string& subkey, const std::vector<char>& data);

	/* Async adds data to user logs and user to activity statistics
		user - name of user
		time - timestamp for log and for activity statistics
		data - user log data
		callback - complete callback
	*/
	void add_log_with_activity(const std::string& user, uint64_t time, const std::vector<char>& data, std::function<void(bool added)> callback);

	/* Async adds data to user logs and user to activity statistics
		user - name of user
		subkey - custom key for logs and for activity statistics
		data - user log data
		callback - complete callback
	*/
	void add_log_with_activity(const std::string& user, const std::string& subkey, const std::vector<char>& data, std::function<void(bool added)> callback);

	/* Gets user's logs for specified period
		user - name of user
		begin_time - begin of the time period
		end_time - end of the time period
		returns list of vectors where vector is a logs of one day
	*/
	std::vector<char> get_user_logs(const std::string& user, uint64_t begin_time, uint64_t end_time);

	/* Gets user's logs for subkeys
		user - name of user
		subkeys - custom keys of user logs
		returns list of vector where vector is a logs of one subkey
	*/
	std::vector<char> get_user_logs(const std::string& user, const std::vector<std::string>& subkeys);

	/* Async gets user's logs for specified period
		user - name of user
		begin_time - begin of the time period
		end_time - end of the time period
		returns list of vectors where vector is a logs of one day
	*/
	void get_user_logs(const std::string& user, uint64_t begin_time, uint64_t end_time, std::function<void(const std::vector<char>& data)> callback);

	/* Async gets user's logs for subkeys
		user - name of user
		subkeys - custom keys of user logs
		returns list of vector where vector is a logs of one subkey
	*/
	void get_user_logs(const std::string& user, const std::vector<std::string>& subkeys, std::function<void(const std::vector<char>& data)> callback);

	/* Gets active users with activity statistics for specified period
		time - timestamp of the activity statistics day
		returns list of active users
	*/
	std::set<std::string> get_active_users(uint64_t begin_time, uint64_t end_time);

	/* Gets active users with activity statistics for specified subkeys
		subkey - custom key of activity statistics
		return list of active users
	*/
	std::set<std::string> get_active_users(const std::vector<std::string>& subkeys);

	/* Async gets active users with activity statistics for specified period
		time - timestamp of the activity statistics day
		returns list of active users
	*/
	void get_active_users(uint64_t begin_time, uint64_t end_time, std::function<void(const std::set<std::string> &active_users)> callback);

	/* Async gets active users with activity statistics for specified subkeys
		subkey - custom key of activity statistics
		return list of active users
	*/
	void get_active_users(const std::vector<std::string>& subkeys, std::function<void(const std::set<std::string> &active_users)> callback);


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
	void for_active_users(uint64_t begin_time, uint64_t end_time, std::function<bool(const std::set<std::string>& active_users)> callback);

	/* Runs throgh activity statistics for specified subkeys and calls callback on each activity statistics
		subkeys - custom keys of activity statistics
		callback - on active users callback
	*/
	void for_active_users(const std::vector<std::string>& subkeys, std::function<bool(const std::set<std::string>& active_users)> callback);

private:
	provider(const provider&) = delete;
	provider& operator=(const provider&) = delete;

	class impl;
	std::shared_ptr<impl>	impl_;
};

extern int get_log_level(const std::string& log_level);

} /* namespace history */

#endif //HISTORY_PROVIDER_H
