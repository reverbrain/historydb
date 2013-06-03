#include "historydb/provider.h"
#include "provider_impl.cpp"

#include <boost/algorithm/string.hpp>

namespace history {

namespace consts {
	const uint32_t SECONDS_IN_DAY = 24 * 60 * 60; // number of seconds in one day. used for calculation days
}

std::string time_to_subkey(uint64_t time)
{
	return boost::lexical_cast<std::string>(time / consts::SECONDS_IN_DAY);
}

std::vector<std::string> time_period_to_subkeys(uint64_t begin_time, uint64_t end_time)
{
	uint64_t begin = begin_time / consts::SECONDS_IN_DAY;
	uint64_t end = (end_time / consts::SECONDS_IN_DAY) + 1;

	std::vector<std::string> ret;

	if(end > begin)
		ret.reserve(end - begin);

	for(; begin < end; ++begin) {
		ret.emplace_back(boost::lexical_cast<std::string>(begin));
	}

	return ret;
}

provider::provider(const std::vector<server_info>& servers, const std::vector<int>& groups, uint32_t min_writes, const std::string& log_file, const int log_level)
: impl_(std::make_shared<impl>(servers, groups, min_writes, log_file, log_level))
{}

provider::provider(const std::vector<std::string>& servers, const std::vector<int>& groups, uint32_t min_writes, const std::string& log_file, const int log_level)
: impl_(std::make_shared<impl>(servers, groups, min_writes, log_file, log_level))
{}

void provider::set_session_parameters(const std::vector<int>& groups, uint32_t min_writes)
{
	impl_->set_session_parameters(groups, min_writes);
}

void provider::add_log(const std::string& user, uint64_t time, const std::vector<char>& data)
{
	impl_->add_log(user, time_to_subkey(time), data);
}

void provider::add_log(const std::string& user, const std::string& subkey, const std::vector<char>& data)
{
	impl_->add_log(user, subkey, data);
}

void provider::add_log(const std::string& user, uint64_t time, const std::vector<char>& data, std::function<void(bool added)> callback)
{
	impl_->add_log(user, time_to_subkey(time), data, callback);
}

void provider::add_log(const std::string& user, const std::string& subkey, const std::vector<char>& data, std::function<void(bool added)> callback)
{
	impl_->add_log(user, subkey, data, callback);
}

void provider::add_activity(const std::string& user, uint64_t time)
{
	impl_->add_activity(user, time_to_subkey(time));
}

void provider::add_activity(const std::string& user, const std::string& subkey)
{
	impl_->add_activity(user, subkey);
}

void provider::add_activity(const std::string& user, uint64_t time, std::function<void(bool added)> callback)
{
	impl_->add_activity(user, time_to_subkey(time), callback);
}

void provider::add_activity(const std::string& user, const std::string& subkey, std::function<void(bool added)> callback)
{
	impl_->add_activity(user, subkey, callback);
}

/*void provider::add_log_and_activity(const std::string& user, uint64_t time, const std::vector<char>& data)
{
	impl_->add_log_and_activity(user, time_to_subkey(time), data);
}

void provider::add_log_and_activity(const std::string& user, const std::string& subkey, const std::vector<char>& data)
{
	impl_->add_log_and_activity(user, subkey, data);
}*/

/*void provider::add_log_and_activity(const std::string& user, uint64_t time, const std::vector<char>& data, std::function<void(bool log_added, bool activity_added)> callback)
{
	impl_->add_log_and_activity(user, time_to_subkey(time), data, callback);
}

void provider::add_log_and_activity(const std::string& user, const std::string& subkey, const std::vector<char>& data, std::function<void(bool log_added, bool activity_added)> callback)
{
	impl_->add_log_and_activity(user, subkey, data, callback);
}*/

std::list<std::vector<char>> provider::get_user_logs(const std::string& user, uint64_t begin_time, uint64_t end_time)
{
	return impl_->get_user_logs(user, time_period_to_subkeys(begin_time, end_time));
}

std::list<std::vector<char>> provider::get_user_logs(const std::string& user, const std::vector<std::string>& subkeys)
{
	return impl_->get_user_logs(user, subkeys);
}

std::list<std::string> provider::get_active_users(uint64_t time)
{
	return impl_->get_active_users(time_to_subkey(time));
}

std::list<std::string> provider::get_active_users(const std::string& subkey)
{
	return impl_->get_active_users(subkey);
}

void provider::for_user_logs(const std::string& user, uint64_t begin_time, uint64_t end_time, std::function<bool(const std::vector<char>& data)> callback)
{
	impl_->for_user_logs(user, time_period_to_subkeys(begin_time, end_time), callback);
}

void provider::for_user_logs(const std::string& user, const std::vector<std::string>& subkeys, std::function<bool(const std::vector<char>& data)> callback)
{
	impl_->for_user_logs(user, subkeys, callback);
}

void provider::for_active_users(uint64_t begin_time, uint64_t end_time, std::function<bool(const std::list<std::string>& active_users)> callback)
{
	impl_->for_active_users(time_period_to_subkeys(begin_time, end_time), callback);
}

void provider::for_active_users(const std::vector<std::string>& subkeys, std::function<bool(const std::list<std::string>& active_users)> callback)
{
	impl_->for_active_users(subkeys, callback);
}


int get_log_level(const std::string& log_level)
{
			if (boost::iequals(log_level,	"DATA"))	return DNET_LOG_DATA;
	else	if (boost::iequals(log_level,	"ERROR"))	return DNET_LOG_ERROR;
	else	if (boost::iequals(log_level,	"INFO"))	return DNET_LOG_INFO;
	else	if (boost::iequals(log_level,	"NOTICE"))	return DNET_LOG_NOTICE;
	else	if (boost::iequals(log_level,	"DEBUG"))	return DNET_LOG_DEBUG;
	return DNET_LOG_ERROR;
}

} /* namespace history */
