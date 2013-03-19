#ifndef HISTORY_SRC_LIB_PROVIDER_H
#define HISTORY_SRC_LIB_PROVIDER_H

#include <iprovider.h>
#include <elliptics/cppdef.h>
#include <map>
#include <boost/thread/shared_mutex.hpp>

namespace history
{
	class provider: public iprovider
	{
	public:
		provider();
		virtual ~provider();

		virtual void connect(const char* server_addr, const int server_port, const int family);
		virtual void disconnect();

		virtual void set_session_parameters(const std::vector<int>& groups, uint32_t min_writes);

		virtual void add_user_activity(const std::string& user, uint64_t time, void* data, uint32_t size, const std::string& key = std::string()) const;

		virtual void repartition_activity(const std::string& key, uint32_t parts) const;
		virtual void repartition_activity(const std::string& old_key, const std::string& new_key, uint32_t parts) const;
		virtual void repartition_activity(uint64_t time, uint32_t parts) const;
		virtual void repartition_activity(uint64_t time, const std::string& new_key, uint32_t parts) const;

		virtual std::list<std::vector<char>> get_user_logs(const std::string& user, uint64_t begin_time, uint64_t end_time) const;

		virtual std::map<std::string, uint32_t> get_active_user(uint64_t time) const;
		virtual std::map<std::string, uint32_t> get_active_user(const std::string& key) const;

		virtual void for_user_logs(const std::string& user, uint64_t begin_time, uint64_t end_time, std::function<bool(const std::string& user, uint64_t time, void* data, uint32_t size)> func) const;

		virtual void for_active_user(uint64_t time, std::function<bool(const std::string& user, uint32_t number)> func) const;
		virtual void for_active_user(const std::string& key, std::function<bool(const std::string& user, uint32_t number)> func) const;


	private:
		ELLIPTICS_DISABLE_COPY(provider)
		std::shared_ptr<ioremap::elliptics::session> create_session(uint64_t ioflags = 0) const;

		void add_user_data(const std::string& user, uint64_t time, void* data, uint32_t size) const;
		void increment_activity(const std::string& user, uint64_t time) const;
		void increment_activity(const std::string& user, const std::string& key) const;

		uint32_t get_key_spread_size(ioremap::elliptics::session& s, const std::string& key) const;
		void generate_activity_key(const std::string& base_key, std::string& res_key, uint32_t& size) const;

		void get_map_from_key(ioremap::elliptics::session& s, const std::string& key, std::map<std::string, uint32_t>& ret) const;

		dnet_node_status								m_node_status;
		std::auto_ptr<ioremap::elliptics::file_logger>	m_log;
		std::auto_ptr<ioremap::elliptics::node> 		m_node;
		std::vector<int>								m_groups;
		uint32_t										m_min_writes;
		mutable boost::shared_mutex						m_connect_mutex;
		mutable boost::mutex							m_key_mutex;
		mutable std::map<std::string, uint32_t>			m_key_cache;
	};
}

#endif //HISTORY_SRC_LIB_PROVIDER_H
