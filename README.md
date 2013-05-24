historydb
=========

History DB is a trully scalable (hundreds of millions updates per day) distributed archive system
with per user and per day activity statistics.

History DB uses [elliptics](http://reverbrain.com/elliptics) as storage backend.
It supports 2 record types: user logs (updated via append or write) and activity.
Each user logs file hosts user logs for certain user and for specified period (by default this equals ot 1 day).
Activity is secondary index which hosts information about
who has activity during specified period (by default this equals to 1 day).

User log primary key is `username (string prefix) + '.' + timestamp (of the day)`.

Activity primary key is `timestamp (of the day) + '.' + chunk number`.

All daily activity is devided to chunks.
One can specify own activity prefix if needed.

User log is a blob which can be either appended or rewritten.


User guide
===========

Interface of the History DB presented in `iprovider.h` file.

	create_provider() - creates instance of iprovider. The instance is used to manipulate the library.

	iprovider::set_session_parameters() - sets parameters for all sessions.
		It includes vector of elliptics groups (replicas) in which HistoryDB stores data and
		minimum number of succeded writes.

	iprovider::add_log - appends data to user log

	iprovider::add_activity - updates user activity

	iprovider::add_user_activity() - appends data to user log and updates user activity.
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

HTTP interface
=========
HistoryDB has follow HTTP interface implemented via fastcgi-daemon2:

	"/add_log" - adds record to user logs.
		Parameters:
			user - name of the user
			data - data of the log record
			timestamp - timestamp of log record

	"/add_activity" - marks user as active in the day.
		Parameters:
			user - name of the user
			[timestamp] - timestamp of user activity
			[key] - custom key for storing user's activity
	
	"/add_user_activity" - adds record to user logs and marks user as active in the day.
		Parameters:
			user - name of the user
			data - data of the log record
			[timestamp] - timestamp of user activity
			[key] - custom key for storing user's activity
	
	"/get_active_users" - returns users who was active in the day.
		Parameters:
			timestamp - timestamp around of which will be searching for active users
			key - custom key of activity statistics
		Use only one parameter. If both are specified the key will be used.
	
	"/get_user_logs" - returns logs of user.
		Parameters:
			user - name of the user
			begin_time - timestamp of the first day for which user's log should be picked
			end_time - timestamp of the last day for which user's log should be picked
			
	"/" - has no parameters. If all is ok - returns HTTP 200. May be used for checking service.

Fastcgi-daemon2 config file
=========

HistoryDB component element should have follow children:

<pre>&lt;log_file&gt;/path/to/log_file&lt;/log_file&gt; - path to elliptics client logs

&lt;log_level&gt;LOG_LEVEL&lt;/log_level&gt; - valid values = { DATA, ERROR, INFO, NOTICE, DEBUG }

&lt;elliptics&gt; - one &lt;elliptics&gt; for each elliptics node
	&lt;addr&gt;address&lt;/addr&gt; - address of elliptics node
	&lt;port&gt;port&lt;/port&gt; - listening port on elliptics node
	&lt;family&gt;family&lt;/family&gt; - protocol family
&lt;/elliptics&gt;

&lt;group&gt;group_number&lt;/group&gt; - group number with which historydb will works. One <group> for each elliptics group.

&lt;min_writes&gt;number&lt;/min_writes&gt; - minimum number of succeded writes in groups. For example, if historydb tries to write in 5 groups and min_writes is 3
the attemp will be failed if write will be succeded in less then 3 groups.
</pre>
