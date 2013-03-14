historydb
=========

History DB is a trully scalable (hundreds of millions updates per day) distributed archive system with per user and per day activity statistics.

History DB uses elliptics at the storage layer. All activity statistics are kept in files. Such files are devided in 2 type: user's statistics and activity's statistics. User's statistics files have id equal to username + timestamp (days from 1 january 1990). Such files contains all activities with full timestamps that user commited at the day. Those files are append-only. Activity's statistics files have id equal to activity id + timestamp (days from 1 january 1990) + number(0 to X). Numbers(0 to X) are used for separating huge files and for supporting multiple changes at one moment. Such files contains key-value serialized tree where key is username and value is how many times the user commited this activity for the day.
So each files conains statistics per day and for activity's statistics separates for each days by random numbers from 0 to X.

User guide.

Interface of the History DB presented in iprovider header file.

	CreateProvider() - creates instance of IProvider. The instance is used to manipulate the library.

	IProvider::Connect() - connects to elliptics. It creates only node and do not starts session.

	IProvider::Disconnect() - disconnects from elliptics. Kills node created in IProvider::Connect().

	IProvider::AddActivity() - add activity record to user's and activity's statistics.

	IProvider::GetActivities() - reads and returns all activities which is made by the user in specified time period.

	IProvider::ForEachActiveUser() - Iterates by active user for certain day and call for each such user callback functor.

	IProvider::ForEachUserActivities() - Iterates by activities for certain user in specified time period and call for each activity callback functor.






TODO:

Add:
	Project description:
		Tutorial
	Read function for certain user for specified time period
	Read activity statistics for specified time period
	Iterator function by active user for certain day
	Iterator function by certain user for specified time period

Remove:

Fix:
