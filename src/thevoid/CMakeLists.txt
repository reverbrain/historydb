add_executable(historydb-thevoid webserver.cpp)
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
