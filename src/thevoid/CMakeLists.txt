add_executable(historydb-thevoid webserver.cpp on_add_log.cpp on_add_activity.cpp on_add_log_with_activity.cpp on_get_active_users.cpp on_get_user_logs.cpp)
target_link_libraries(historydb-thevoid
	historydb
	thevoid
	swarm
	${Boost_SYSTEM_LIBRARY}
)
install(TARGETS
	historydb-thevoid
	RUNTIME DESTINATION bin COMPONENT runtime
)
