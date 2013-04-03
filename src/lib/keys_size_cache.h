#ifndef HISTORY_SRC_LIB_KEYS_SIZE_CACHE_H
#define HISTORY_SRC_LIB_KEYS_SIZE_CACHE_H

#include <stdint.h>
#include <string>
#include <map>
#include <boost/thread/shared_mutex.hpp>

namespace history {

class keys_size_cache
{
public:
	/* Gets count of chunk for key. If key isn't in cache return -1
		key - activity statistics key
	*/
	uint32_t get(const std::string& key);
	/* Sets count of chunk for key: saves it for future gets
		key - activity statistics key
		size - count of chunk for key
	*/
	void set(const std::string& key, uint32_t size);
	/* Removed value from cache.
		key - activity statistics key
	*/
	void remove(const std::string& key);
private:
	boost::shared_mutex				m_mutex;		// mutex for sync working with cache
	std::map<std::string, uint32_t>	m_keys_sizes;	// counts of activity statistics chunks cache
};

} /* namespace history */

#endif // HISTORY_SRC_LIB_KEYS_SIZE_CACHE_H
