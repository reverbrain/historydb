#ifndef HISTORY_SRC_LIB_PROVIDER_H
#define HISTORY_SRC_LIB_PROVIDER_H

#include <iprovider.h>
#include <elliptics/cppdef.h>
#include <map>
#include <boost/thread/shared_mutex.hpp>

namespace history {

	class provider: public iprovider
	{
	public:
		provider(const char* server_addr, const int server_port, const int family);
		virtual ~provider();

		virtual void set_session_parameters(const std::vector<int>& groups, uint32_t min_writes);

		virtual void add_user_activity(const std::string& user, uint64_t time, void* data, uint32_t size, const std::string& key = std::string());

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
		ELLIPTICS_DISABLE_COPY(provider)

		/* Creates and initializes session
			ioflags - io flags
			return session
		*/
		ioremap::elliptics::session create_session(uint64_t ioflags = 0);

		/* Add data to the daily user log
			user - name of user
			time - timestamp of log
			data - pointer to the log data
			size - size of log data
		*/
		void add_user_data(const std::string& user, uint64_t time, void* data, uint32_t size);

		/* Increments user activity statistics
			user - name of user
			time - timestamp
		*/
		void increment_activity(const std::string& user, uint64_t time);

		/* Increments user activity statistics
			user -nane of user
			time - timestamp
		*/
		void increment_activity(const std::string& user, const std::string& key);

		/*	Gets number of chunks for key
			s - session
			key - key of activity statistics
		*/
		uint32_t get_chunks_count(ioremap::elliptics::session& s, const std::string& key);

		/*	Generates activity statistics chunk key by random
			base_key - activity statistics main key
			res_key - used to return generated activity statistics chunk key
			size - used to return number of chunks for key
		*/
		void generate_activity_key(const std::string& base_key, std::string& res_key, uint32_t& size);

		/* Gets activity statistics chunk map from key
			s - session
			key - key of activity statistics chunk
			ret - result map
		*/
		void get_map_from_key(ioremap::elliptics::session& s, const std::string& key, std::map<std::string, uint32_t>& ret);

		std::vector<int>							m_groups;		// groups of elliptics
		uint32_t									m_min_writes;	// minimum number of succeeded writes for each write attempt
		mutable boost::mutex						m_key_mutex;	// mutex for sync working with m_key_cache
		mutable std::map<std::string, uint32_t>		m_key_cache;	// cache of numbers of activity statistics chunks

		ioremap::elliptics::file_logger				m_log;			// logger
		ioremap::elliptics::node					m_node;			// elliptics node
	};
} /* namespace history */

#endif //HISTORY_SRC_LIB_PROVIDER_H
