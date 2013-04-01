add_library(historydb SHARED provider.cpp)
target_link_libraries(historydb
	${ELLIPTICS_LIBRARIES}
	${ELLIPTICS_CPP_LIBRARIES}
	${MSGPACK_LIBRARIES}
)

set_target_properties(historydb PROPERTIES
    VERSION ${DEBFULLVERSION}
    SOVERSION ${HISTORYDB_VERSION_ABI}
)

install(TARGETS historydb
    LIBRARY DESTINATION lib${LIB_SUFFIX}
    ARCHIVE DESTINATION lib${LIB_SUFFIX}
    BUNDLE DESTINATION library
)
