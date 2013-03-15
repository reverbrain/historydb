historydb
=========

History DB is a trully scalable (hundreds of millions updates per day) distributed archive system with per user and per day activity statistics.

History DB uses elliptics at the storage layer. At the elliptics backend all data is stored in files. Each file has unique string id. Elliptics files for History DB are devides into user's logs files and activity's statistics and by day. User logs files have id: user_name + timestamp where timestamp is twenty character number. Activity statistics files have id: timestamp + number_from_0_to_x where timestamp is twenty character number and the number is a random number from 0 to X. Activity statistics file can has a custom key which sets by using special function for updating and reading statistics. Records in user logs files has follow structure: uint64_t_timestamp + uint32_t_size_of_custom_data + void*_custom_data. Each activity statistics file contains key-value tree where key is user name and value is the user activity statistics.

User guide.

Interface of the History DB presented in iprovider header file.

	CreateProvider() - creates instance of IProvider. The instance is used to manipulate the library.

	IProvider::Connect() - connects to elliptics. It creates only node and do not starts any session.

	IProvider::Disconnect() - disconnects from elliptics. Kills node created in IProvider::Connect().

	IProvider::SetSessionParameters() - sets parameters for all sessions. It includes vector of elliptics groups in which History DB will store data.

	IProvider::AddUserActivity() - add record to user logs and increased activity statistics for this user. 

	IProvider::RepartitionActivity() - number of methods which repartitions activity statistics tree with new parts.

	IProvider::ForUserLogs() - Iterates by user logs for certain user in specified time period and calls for each record callback functior.

	IProvider::ForActiveUser() - Iterates by activity statistics for certain day and calls for each user callback functor.

Elliptics backend.

At the elliptics backend all data is stored in files. Each file has unique string id. User logs and activity statistics have daily structure. User logs has id: user_name + timestamp where timestamp is twenty character number. Activity statistics has id: timestamp + number_from_0_to_X where timestamp is twenty character number and number - is a random number from 0 to X. Activity statistics file can has custom key which sets by using special functions. Record in user's logs file is timestamp + size_of_custom_data + custom_data. Each activity statistics file contain key-value tree with key <-> user name and value <-> the user activity statistic.


TODO:

Add:
	Project description:
		Tutorial

Remove:

Fix:
