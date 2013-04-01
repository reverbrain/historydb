project(historydb)
cmake_minimum_required(VERSION 2.8)
add_definitions(-std=c++0x -W -Wall)

FILE (READ "${CMAKE_CURRENT_SOURCE_DIR}/debian/changelog" DEBCHANGELOG)

string(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+)" DEBFULLVERSION "${DEBCHANGELOG}")
STRING (REGEX MATCH "([0-9]+\\.[0-9]+)" HISTORYDB_VERSION_ABI "${DEBFULLVERSION}")
STRING (REGEX MATCH "([0-9]+\\.[0-9]+$)" HISTORYDB_VERSION_MINOR "${DEBFULLVERSION}")

INCLUDE(cmake/locate_library.cmake)
LOCATE_LIBRARY(ELLIPTICS "elliptics/interface.h" "elliptics" "elliptics_cpp")
LOCATE_LIBRARY(ELLIPTICS_CPP "elliptics/cppdef.h" "elliptics_cpp" "elliptics_cpp")

find_package(Boost REQUIRED COMPONENTS system thread)

INCLUDE_DIRECTORIES(
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${ELLIPTICS_INCLUDE_DIRS}
)

LINK_DIRECTORIES(
	${ELLIPTICS_LIBRARY_DIRS}
)

add_subdirectory(src/app)
add_subdirectory(src/lib)

if(WITH_FASTCGI)
	add_subdirectory(src/fastcgi)
endif()

install(FILES
	include/historydb/iprovider.h
	DESTINATION include/historydb/
)
