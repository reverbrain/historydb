#ifndef HISTORY_SRC_LIB_PROVIDER_H
#define HISTORY_SRC_LIB_PROVIDER_H

#include <iprovider.h>
#include <elliptics/cppdef.h>
#include <map>
#include <boost/thread/shared_mutex.hpp>

namespace History
{
	class Provider: public IProvider
	{
	public:
		Provider();
		virtual ~Provider();

		virtual void Connect(const char* server_addr, const int server_port);
		virtual void Disconnect();

		virtual void SetSessionParameters(const std::vector<int>& groups, uint32_t min_writes);

		virtual void AddUserActivity(const std::string& user, uint64_t time, void* data, uint32_t size, const std::string& key = std::string()) const;

		virtual void RepartitionActivity(const std::string& key, uint32_t parts) const;
		virtual void RepartitionActivity(const std::string& old_key, const std::string& new_key, uint32_t parts) const;
		virtual void RepartitionActivity(uint64_t time, uint32_t parts) const;
		virtual void RepartitionActivity(uint64_t time, const std::string& new_key, uint32_t parts) const;

		virtual void ForUserLogs(const std::string& user, uint64_t begin_time, uint64_t end_time, std::function<bool(const std::string& user, uint64_t time, void* data, uint32_t size)> func) const;

		virtual void ForActiveUser(uint64_t time, std::function<bool(const std::string& user, uint32_t number)> func) const;
		virtual void ForActiveUser(const std::string& key, std::function<bool(const std::string& user, uint32_t number)> func) const;


	private:
		ELLIPTICS_DISABLE_COPY(Provider)
		std::shared_ptr<ioremap::elliptics::session> CreateSession(uint64_t ioflags = 0) const;

		void AddUserData(const std::string& user, uint64_t time, void* data, uint32_t size) const;
		void IncrementActivity(const std::string& user, uint64_t time) const;
		void IncrementActivity(const std::string& user, const std::string& key) const;

		uint32_t GetKeySpreadSize(ioremap::elliptics::session& s, const std::string& key) const;
		void GenerateActivityKey(const std::string& base_key, std::string& res_key, uint32_t& size) const;

		void GetMapFromKey(ioremap::elliptics::session& s, const std::string& key, std::map<std::string, uint32_t>& ret) const;

		//Temporary
		void Clean() const;

		dnet_node_status								node_status_;
		std::auto_ptr<ioremap::elliptics::file_logger>	log_;
		std::auto_ptr<ioremap::elliptics::node> 		node_;
		std::vector<int>								groups_;
		uint32_t										min_writes_;
		boost::shared_mutex								connect_mutex_;
	};
}

#endif //HISTORY_SRC_LIB_PROVIDER_H
