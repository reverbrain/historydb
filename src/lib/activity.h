#ifndef HISTORY_SRC_LIB_ACTIVITIES_H
#define HISTORY_SRC_LIB_ACTIVITIES_H

#include <msgpack.hpp>
#include <map>

namespace history {

	struct activity {
		uint32_t						size;	// count of chunks
		std::map<std::string, uint32_t>	map;	// activity map

		MSGPACK_DEFINE(size, map);
	};
}

#endif //HISTORY_SRC_LIB_ACTIVITIES_H
