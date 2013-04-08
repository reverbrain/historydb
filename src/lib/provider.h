#ifndef HISTORY_SRC_LIB_PROVIDER_H
#define HISTORY_SRC_LIB_PROVIDER_H

#include <map>
#include <boost/thread/shared_mutex.hpp>

#include <elliptics/cppdef.h>

#include "historydb/iprovider.h"
#include "keys_size_cache.h"
#include "features.h"

namespace ioremap { namespace elliptics { class session; }}

namespace history {

	struct activity;

	class provider: public iprovider
	{
	public:
		provider(const char* server_addr, const int server_port, const int family, const char* log_file, const int log_level);
		virtual ~provider();

		virtual void set_session_parameters(const std::vector<int>& groups, uint32_t min_writes);

		virtual void add_user_activity(const std::string& user, uint64_t time, void* data, uint32_t size, const std::string& key = std::string());
		virtual void add_user_activity(const std::string& user, uint64_t time, void* data, uint32_t size, std::function<void(bool log_writed, bool statistics_updated)> func, const std::string& key = std::string());

		virtual void repartition_activity(const std::string& key, uint32_t parts);
		virtual void repartition_activity(const std::string& old_key, const std::string& new_key, uint32_t parts);
		virtual void repartition_activity(uint64_t time, uint32_t parts);
		virtual void repartition_activity(uint64_t time, const std::string& new_key, uint32_t parts);

		virtual std::list<std::vector<char>> get_user_logs(const std::string& user, uint64_t begin_time, uint64_t end_time);

		virtual std::map<std::string, uint32_t> get_active_users(uint64_t time);
		virtual std::map<std::string, uint32_t> get_active_users(const std::string& key);

		virtual void for_user_logs(const std::string& user, uint64_t begin_time, uint64_t end_time, std::function<bool(const std::string& user, uint64_t time, void* data, uint32_t size)> func);

		virtual void for_active_users(uint64_t time, std::function<bool(const std::string& user, uint32_t number)> func);
		virtual void for_active_users(const std::string& key, std::function<bool(const std::string& user, uint32_t number)> func);

	private:

		std::shared_ptr<features> make_features();

		std::shared_ptr<features> shared_features();

		provider(const provider&);
		provider &operator =(const provider &);


		std::vector<int>							m_groups;		// groups of elliptics
		uint32_t									m_min_writes;	// minimum number of succeeded writes for each write attempt

		ioremap::elliptics::file_logger				m_log;			// logger
		ioremap::elliptics::node					m_node;			// elliptics node

		keys_size_cache								m_keys_cache;
	};
} /* namespace history */

#endif //HISTORY_SRC_LIB_PROVIDER_H
