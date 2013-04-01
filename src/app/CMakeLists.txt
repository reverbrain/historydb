add_executable(historydb_example main.cpp test2.cpp)
target_link_libraries(historydb_example
	historydb
	${Boost_THREAD_LIBRARY}
)
