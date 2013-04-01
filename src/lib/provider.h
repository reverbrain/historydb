#ifndef HISTORY_SRC_LIB_PROVIDER_H
#define HISTORY_SRC_LIB_PROVIDER_H

#include <iprovider.h>
#include <elliptics/cppdef.h>
#include <map>
#include <boost/thread/shared_mutex.hpp>

namespace ioremap { namespace elliptics { class session; }}

namespace history {

	struct activity;

	class provider: public iprovider
	{
	public:
		provider(const char* server_addr, const int server_port, const int family);
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
		provider(const provider&);
		provider &operator =(const provider &);

		/* Creates and initializes session
			ioflags - io flags
			return session
		*/
		ioremap::elliptics::session create_session(uint64_t ioflags = 0);

		std::shared_ptr<ioremap::elliptics::session> shared_session(uint64_t ioflags = 0);

		/* Add data to the daily user log
			user - name of user
			time - timestamp of log
			data - pointer to the log data
			size - size of log data
		*/
		void add_user_data(const std::string& user, uint64_t time, void* data, uint32_t size);

		void add_user_data(const std::string& user, uint64_t time, void* data, uint32_t size, std::function<void(bool writed)> func);

		/* Increments user activity statistics
			user -nane of user
			time - timestamp
		*/
		void increment_activity(const std::string& user, const std::string& key);

		void increment_activity(const std::string& user, const std::string& key, std::function<void(bool updated)> func);

		/* Tries increments user activity statistics. If fails return false. It is used by increment_activity.
			user - name of user
			key - key of activity statistics
		*/
		bool try_increment_activity(const std::string& user, const std::string& key);

		void try_increment_activity(const std::string& user, const std::string& key, std::function<void(bool updated)> func);

		/* Makes user log id from user name and timestamp
			user - name of user
			time - timestamp
		*/
		std::string make_key(const std::string& user, uint64_t time);

		/* Makes general activity statistics key
		*/
		std::string make_key(uint64_t time);

		/* Makes chunk key from general key
			key - general activity statistics key
			chunk - number of chunk
		*/
		std::string make_chunk_key(const std::string& key, uint32_t chunk);

		/* Gets chunk data
			s - session
			key - general activity statistics key
			chunk - number of chunk
			act - where chunk data should be data written
			checksum - where checksum should be written
			returns true if chunk has successfully read and has been written to act (and to checksum)
					otherwise returns false
		*/
		bool get_chunk(ioremap::elliptics::session& s, const std::string& key, uint32_t chunk, activity& act, dnet_id* checksum = NULL);

		void get_chunk(std::shared_ptr<ioremap::elliptics::session> s, const std::string& key, uint32_t chunk, std::function<void(bool exist, activity act, dnet_id checksum)> func);

		/* Writes data to elliptics in specified session
			s - elliptics session
			key - id of file where data should be written
			data - pointer to the data
			size - size of data.
		*/
		bool write_data(ioremap::elliptics::session& s, const std::string& key, void* data, uint32_t size);

		void write_data(ioremap::elliptics::session& s, const std::string& key, void* data, uint32_t size, std::function<void(bool writed)> func);

		/* Writes data to elliptics in specified session by using write_cas method and checksum
			s - elliptics session
			key - id of file where data should be written
			data - pointer to the data
			size - size of data
			checksum - checksum
		*/
		bool write_data(ioremap::elliptics::session& s, const std::string& key, void* data, uint32_t size, const dnet_id& checksum);

		void write_data(std::shared_ptr<ioremap::elliptics::session> s, const std::string& key, void* data, uint32_t size, const dnet_id& checksum, std::function<void(bool writed)> func);

		/* Generate random value in range [0, max)
			max - the upper limit of random range
		*/
		uint32_t rand(uint32_t max);

		/*Merges two chunks. res_chunk will contain result of merging
			res_chunk - chunk in which result will be written
			merge_chunk - chunk data from which will be added to res_chunk
		*/
		void merge(activity& res_chunk, const activity& merge_chunk) const;

		/* Gets full activity statistics for specified general tree
			s - session
			key - general key
		*/
		activity get_activity(ioremap::elliptics::session& s, const std::string& key);


		std::vector<int>							m_groups;		// groups of elliptics
		uint32_t									m_min_writes;	// minimum number of succeeded writes for each write attempt

		ioremap::elliptics::file_logger				m_log;			// logger
		ioremap::elliptics::node					m_node;			// elliptics node

		class keys_size_cache
		{
		public:
			/* Gets count of chunk for key. If key isn't in cache return -1
				key - activity statistics key
			*/
			inline uint32_t get(const std::string& key);
			/* Sets count of chunk for key: saves it for future gets
				key - activity statistics key
				size - count of chunk for key
			*/
			inline void set(const std::string& key, uint32_t size);
			/* Removed value from cache.
				key - activity statistics key
			*/
			inline void remove(const std::string& key);
		private:
			boost::shared_mutex				m_mutex;		// mutex for sync working with cache
			std::map<std::string, uint32_t>	m_keys_sizes;	// counts of activity statistics chunks cache
		};

		keys_size_cache								m_keys_cache;
	};
} /* namespace history */

#endif //HISTORY_SRC_LIB_PROVIDER_H
