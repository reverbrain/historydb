add_library(historydb-fastcgi SHARED historydb-fastcgi.cpp)
target_link_libraries(historydb-fastcgi
	historydb
)

set_target_properties(historydb-fastcgi PROPERTIES
    VERSION ${DEBFULLVERSION}
    SOVERSION ${HISTORYDB_VERSION_ABI}
)

install(TARGETS historydb-fastcgi
    LIBRARY DESTINATION lib${LIB_SUFFIX}
    ARCHIVE DESTINATION lib${LIB_SUFFIX}
    BUNDLE DESTINATION library
)
