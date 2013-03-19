historydb
=========

History DB is a trully scalable (hundreds of millions updates per day) distributed archive system
with per user and per day activity statistics.

History DB uses elliptics (http://reverbrain.com/elliptics) as storage backend.
It supports 2 record types: user logs (updated via append or write) and activity.
Each HistoryDB record has unique string id, which is used as primary key.
Activity is a secondary index which hosts information about what primary keys were
updated during specified period (by default this equals to 1 day).

User log key consists of two fields: username (string prefix) and timestamp (this is actually 20-bytes string)
Activity key is timestamp only, but it is shareded among N subkeys (where N is specified in config).
One can specify own activity prefix if needed.

User log is a blob which can be either appended or rewritten.
Activity is a map of usernames and update counters.


User guide
===========

Interface of the History DB presented in iprovider header file.

	create_provider() - creates instance of iprovider. The instance is used to manipulate the library.

	iprovider::set_session_parameters() - sets parameters for all sessions.
		It includes vector of elliptics groups (replicas) in which HistoryDB stores data.

	iprovider::add_user_activity() - appends data to user log and increments its activity counter.

	iprovider::repartition_activity() - repartition current activy shards.

	iprovider::for_user_logs() - iterates over user's logs in specified time period.
	iprovider::for_active_user() - iterates over activity logs in specified time period.

One can grab user logs for specified for specified period of time as well as list of all users,
who were active (had at least one log update) during requested period of time.
