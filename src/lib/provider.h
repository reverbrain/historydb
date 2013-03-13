#ifndef HISTORY_SRC_LIB_PROVIDER_H
#define HISTORY_SRC_LIB_PROVIDER_H

#include <iprovider.h>
#include <elliptics/cppdef.h>

namespace History
{
	class Provider: public IProvider
	{
	public:
		Provider();
		virtual ~Provider();

		/* Connects to elliptics
		*/
		virtual void Connect();

		/* Disconnects from elliptics
		*/
		virtual void Disconnect();

		/* Add activity record to certain user
		*/
		virtual void AddActivity(const std::string& user, Activity activity, uint32_t time) const;

		/* Gets activities statistics for certain user for specified period
		*/
		virtual std::list<ActivityData> GetActivities(const std::string& user, uint32_t begin_time, uint32_t end_time) const;

		/* Gets activities for all users for specified period
		*/
		virtual std::map<std::string, uint32_t> GetActivities(Activity activity, uint32_t begin_time, uint32_t end_time) const;

		/* Iterates by active users
		*/
		virtual void ForEachUser(Activity activity, uint32_t begin_time, uint32_t end_time, std::function<bool(const std::string&, uint32_t)> func) const;

		/* Iterates by certain user activities for specified period
		*/
		virtual void ForEachActivities(const std::string& user, uint32_t begin_time, uint32_t end_time, std::function<bool(ActivityData)> func) const;

	private:

		ELLIPTICS_DISABLE_COPY(Provider)
		std::shared_ptr<ioremap::elliptics::session> CreateSession(uint64_t ioflags = 0) const;
		inline ioremap::elliptics::key GetUserKey(const std::string& user, uint32_t time) const;
		inline ioremap::elliptics::key GetLogKey(Activity activity, uint32_t time, uint32_t file = -1) const;

		void AddActivityToUser(const std::string& user, Activity activity, uint32_t time) const;
		void AddActivityToLog(const std::string& user, Activity activiy, uint32_t time) const;

		//Temporary
		void Clean() const;

		dnet_node_status								node_status_;
		std::auto_ptr<ioremap::elliptics::file_logger>	log_;
		std::auto_ptr<ioremap::elliptics::node> 		node_;
	};
}

#endif //HISTORY_SRC_LIB_PROVIDER_H
