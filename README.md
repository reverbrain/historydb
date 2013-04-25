historydb
=========

History DB is a trully scalable (hundreds of millions updates per day) distributed archive system
with per user and per day activity statistics.

History DB uses [elliptics](http://reverbrain.com/elliptics) as storage backend.
It supports 2 record types: user logs (updated via append or write) and activity.
Each HistoryDB record has unique string id, which is used as primary key.
Each user logs file hosts user logs for certain user and for specified period (by default this equals ot 1 day).
Activity is secondary index which hosts information about how much acitivy
was generated by user during specified period (by default this equals to 1 day).

User log primary key is `username (string prefix) + '.' + timestamp (of the day)`.

Activity primary key is `timestamp (of the day) + '.' + chunk number`.

All daily activity is devided to chunks.
One can specify own activity prefix if needed.

User log is a blob which can be either appended or rewritten.
Activity is a map of usernames and update counters.


User guide
===========

Interface of the History DB presented in `iprovider.h` file.

	create_provider() - creates instance of iprovider. The instance is used to manipulate the library.

	iprovider::set_session_parameters() - sets parameters for all sessions.
		It includes vector of elliptics groups (replicas) in which HistoryDB stores data and
		minimum number of succeded writes.

	iprovider::add_user_activity() - appends data to user log and increments its activity counter.
		There are two implementation of this function: synchronous and asynchronous.

	iprovider::get_user_logs() - gets user logs.

	iprovider::get_active_user() - gets active user for specified day.

	iprovider::for_user_logs() - iterates over user's logs in specified time period.
	iprovider::for_active_user() - iterates over activity logs in specified time period.

One can grab user logs for specified for specified period of time as well as list of all users,
who were active (had at least one log update) during requested period of time.

Tutorial
=========

Firstly, complete [elliptics tutorial](http://doc.reverbrain.com/elliptics:server-tutorial).
It is neccessary to start work with elliptics.

Download source tree

	git clone http://github.com/reverbrain/historydb.git

Building library

	cd historydb
	debuild
	sudo dpkg -i ../historydb_0.1.0.0_amd64.deb

Now you can start to use HistoryDB library:
Include `historydb/iprovider.h`.
Create `iprovider` instance by calling `create_provider` method.
Use iprovider instance to write/read user logs, update activity, gets active user statistics, repartition shards etc.

Fastcgi-daemon2 config file
=========

HistoryDB component element should have follow children:

<log_file>/path/to/log_file</log_file> - path to elliptics client logs

<log_level>LOG_LEVEL</log_level> - valid values = { DATA, ERROR, INFO, NOTICE, DEBUG }

<elliptics> - one <elliptics> for each elliptics node
	<addr>address</addr> - address of elliptics node
	<port>port</port> - listening port on elliptics node
	<family>family</family> - protocol family
</elliptics>

<group>group_number</group> - group number with which historydb will works. One <group> for each elliptics group.

<min_writes>1</min_writes> - minimum number of succeded writes in groups. For example, if historydb tries to write in 5 groups and min_writes is 3
the attemp will be failed if write will be succeded in less then 3 groups.
