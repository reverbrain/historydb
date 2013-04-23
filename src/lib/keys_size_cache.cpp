#include "keys_size_cache.h"

namespace history {

uint32_t keys_size_cache::get(const std::string& key)
{
	boost::shared_lock<boost::shared_mutex> lock(m_mutex);

	auto it = m_keys_sizes.find(key); //finds key in map

	return it == m_keys_sizes.end() ? -1 : it->second; // if we have such key in map returns known size otherwise return -1
}

void keys_size_cache::set(const std::string& key, uint32_t size)
{
	boost::unique_lock<boost::shared_mutex> lock(m_mutex);

	auto res = m_keys_sizes.insert(std::make_pair(key, size)); // try to insert new size for key
	if (!res.second) // if it failed
		res.first->second = size; // rewrites old size
}

void keys_size_cache::remove(const std::string& key)
{
	boost::unique_lock<boost::shared_mutex> lock(m_mutex);
	m_keys_sizes.erase(key); // removes key
}

bool keys_size_cache::has(const std::string& key)
{
	boost::shared_lock<boost::shared_mutex> lock(m_mutex);
	return m_keys_sizes.find(key) != m_keys_sizes.end();
}

} /* namespace history */
