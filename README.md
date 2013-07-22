[HistoryDB](http://doc.reverbrain.com/historydb:historydb)
=========

[History DB](http://doc.reverbrain.com/historydb:historydb) is a trully scalable (hundreds of millions updates per day) distributed archive system
with per user and per day activity statistics.

[History DB](http://doc.reverbrain.com/historydb:historydb) uses [elliptics](http://reverbrain.com/elliptics) as storage backend.
It supports 2 record types: user logs (updated via append or write) and activity.
Each user logs file hosts user logs for certain user and for specified period (by default this equals ot 1 day).
Activity is secondary index which hosts information about
who has activity during specified period (by default this equals to 1 day).

User log primary key is `username (string prefix) + '.' + timestamp (of the day)`.

Activity primary key is `timestamp (of the day) + '.' + chunk number`.

All daily activity is devided to chunks.
One can specify own activity prefix if needed.

User log is a blob which can be either appended or rewritten.


[User guide](http://doc.reverbrain.com/historydb:cplusplus_api)
===========

Interface of the [History DB](http://doc.reverbrain.com/historydb:historydb) presented in `provider.h` file.

	provider::provider() - contructor
	
	provider::set_session_parameters() - sets parameters for all sessions.
		It includes vector of elliptics groups (replicas) in which HistoryDB stores data and
		minimum number of succeded writes.

	provider::add_log - appends data to user log

	provider::add_activity - updates user activity
	
	provider::add_log_with_activity - appends data to user log and updates user activity

	provider::get_user_logs() - gets user logs.

	provider::get_active_user() - gets active user for specified day.

	provider::for_user_logs() - iterates over user's logs in specified time period.
	
	provider::for_active_user() - iterates over activity logs in specified time period.

One can grab user logs for specified for specified period of time as well as list of all users,
who were active (had at least one log update) during requested period of time.

[Tutorial](http://doc.reverbrain.com/historydb:tutorial)
=========
See [extended tutorial](http://doc.reverbrain.com/historydb:tutorial)

Firstly, complete [elliptics tutorial](http://doc.reverbrain.com/elliptics:server-tutorial).
It is neccessary to start work with elliptics.

Download source tree

	git clone http://github.com/reverbrain/historydb.git

Building library

	cd historydb
	debuild
	sudo dpkg -i ../historydb_*.deb

Now you can start to use HistoryDB library:
Include `historydb/provider.h`.
Create `provider` instance.
Use provider instance to write/read user logs, update activity, gets active user statistics, repartition shards etc.

[HTTP interface](http://doc.reverbrain.com/historydb:http_api)
=========
[HistoryDB](http://doc.reverbrain.com/historydb:historydb) has follow HTTP interface implemented via fastcgi-daemon2:

	"/add_log" POST - adds record to user logs.
		Parameters:
			user - name of the user
			data - data of the log record
			time or key. If both: key and time are specified - key will be used
				time - timestamp of record
				key - custom key of record

	"/add_activity" POST - marks user as active in the day.
		Parameters:
			user - name of the user
			time or key. If both: key and time are specified - key will be used
				time - timestamp of activity statistics
				key - custom key of activity statistics
				
	"/add_log_with_activity" POST - Appends log to user logs and updates user activity.
		Parameters:
			user - name of user
			data - data of log record
			time or key. If both: key and time are specified - key will be used
				time - timestamp of record
				key - custom key of record
	
	"/get_active_users" GET - returns users who was active in the day.
		Parameters:
			time or key. If both: key and time are specified - key will be used
				time - timestamp of activity statistics
				key - custom key of activity statistics
	
	"/get_user_logs" GET - returns logs of user.
		Parameters:
			user - name of the user
			begin_time and end_time - time period for logs
			
	"/" POST&GET - has no parameters. If all is ok - returns HTTP 200. May be used for checking service.

[Fastcgi-daemon2 config file](http://doc.reverbrain.com/historydb:http_configure)
=========

[HistoryDB](http://doc.reverbrain.com/historydb:historydb) component element should have follow children:

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

[HistoryDB Tool for aggregacting logs](http://doc.reverbrain.com/historydb:tools)
=========
The tool goes through activity statisitics for specified days, aggregates logs for each active users and saves it in specified new subkeys.

	Usage: hdb_tool.py KEYS NEW_KEY [options]

	Options:
		-h, --help            show this help message and exit
		-b BATCH_SIZE, --batch-size=BATCH_SIZE
			Number of keys in read_bulk/write_bulk batch [default:1024]
		-d, --debug           Enable debug output [default: False]
		-r ELLIPTICS_REMOTE, --remote=ELLIPTICS_REMOTE
			Elliptics node address [default: none]
		-g ELLIPTICS_GROUPS, --groups=ELLIPTICS_GROUPS
			Comma separated list of groups [default: all]
		-l FILE, --log=FILE   Output log messages from library to file [default:hdb_tool.log]
		-L ELLIPTICS_LOG_LEVEL, --log-level=ELLIPTICS_LOG_LEVEL
			Elliptics client verbosity [default: 1]
		-u USERS, --user=USERS
			User whose logs should be aggregated
